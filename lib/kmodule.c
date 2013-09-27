#include <libgen.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <fnmatch.h>
#include <syslog.h>
#include <limits.h>
#include <liblazy/io.h>
#include <liblazy/kmodule.h>

bool kmodule_loader_init(kmodule_loader_t *loader, bool is_quiet) {
	/* open the system log */
	if (false == is_quiet)
		openlog(KMODULE_LOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_USER);

	/* initialize the logging flag */
	loader->is_quiet = is_quiet;

	/* initialize the cache files */
	loader->modules_list = NULL;
	loader->dependencies_list = NULL;
	loader->aliases_list = NULL;
	loader->loaded_modules_list = NULL;

	return true;
}

void kmodule_loader_destroy(kmodule_loader_t *loader) {
	/* close the cache files */
	if (NULL != loader->modules_list)
		(void) fclose(loader->modules_list);
	if (NULL != loader->dependencies_list)
		(void) fclose(loader->dependencies_list);
	if (NULL != loader->aliases_list)
		(void) fclose(loader->aliases_list);

	/* close the system log */
	if (false == loader->is_quiet)
		closelog();
}

void _loader_log(kmodule_loader_t *loader,
                 int priority,
                 const char *format,
                 const char *arg) {
	if (false == loader->is_quiet)
		syslog(priority, format, arg);
}

void _open_cache_file(kmodule_loader_t *loader,
                      const char *path,
                      FILE **handle) {
	/* if the file was already opened, do nothing */
	if (NULL != *handle)
		goto rewind;

	/* otherwise, open it */
	*handle = fopen(path, "r");
	if (NULL == *handle)
		return;

rewind:
	/* return to the beginning of the file */
	rewind(*handle);
}

FILE *_get_aliases_list(kmodule_loader_t *loader) {
	_open_cache_file(loader, KMODULE_ALIAS_LIST_PATH, &loader->aliases_list);
	return loader->aliases_list;
}

FILE *_get_modules_list(kmodule_loader_t *loader) {
	_open_cache_file(loader, KMODULE_LIST_PATH, &loader->modules_list);
	return loader->modules_list;
}

FILE *_get_dependencies_list(kmodule_loader_t *loader) {
	_open_cache_file(loader,
	                 KMODULE_DEPENDENCIES_LIST_PATH,
	                 &loader->dependencies_list);
	return loader->dependencies_list;
}

FILE *_get_loaded_modules_list(kmodule_loader_t *loader) {
	_open_cache_file(loader,
	                 KMODULE_LOADED_MODULES_LIST_PATH,
	                 &loader->loaded_modules_list);
	return loader->loaded_modules_list;
}

bool _locate_module_by_name(kmodule_loader_t *loader,
                            const char *name,
                            char *path) {
	/* the return value */
	bool is_found = false;

	/* the kernel modules list */
	FILE *modules_list;

	/* a line in the kernel modules list */
	char line[PATH_MAX];

	/* the line length */
	int line_length;

	/* the kernel module path */
	char *module_path;

	/* the kernel module file name */
	char name_with_extension[NAME_MAX];

	/* format the module file name */
	(void) sprintf((char *) &name_with_extension,
	               "%s.%s",
	               name,
	               KMODULE_FILE_NAME_EXTENSION);

	/* open the kernel modules list */
	modules_list = _get_modules_list(loader);
	if (NULL == modules_list)
		goto end;

	do {
		/* read one line */
		if (NULL == fgets((char *) &line, sizeof(line), modules_list))
			goto end;

		/* strip the trailing line break, if there is any */
		line_length = strlen(line);
		if ('\n' == line[line_length - sizeof(char)])
			line[line_length - sizeof(char)] = '\0';

		/* locate the separator between the module name and its path */
		module_path = strchr((char *) &line, KMODULE_LIST_DELIMETER);
		if (NULL == module_path)
			goto end;

		/* replace the separator with a NULL byte and skip it */
		module_path[0] = '\0';
		++module_path;

		/* check whether the name matches */
		if (0 != strcmp((char *) &line, (char *) &name_with_extension))
			continue;

		/* return the full file path */
		(void) strcpy(path, module_path);

		/* report success */
		is_found = true;
		break;
	} while (1);

end:
	return is_found;
}

bool kmodule_open(kmodule_loader_t *loader,
                  kmodule_t *module,
                  const char *name,
                  const char *path) {
	/* the return value */
	bool is_success = false;

	if (NULL == path) {
		/* if the module was specified by its name, locate it */
		if (NULL != name) {
			if (false == _locate_module_by_name(loader,
			                                    name,
			                                    (char *) &module->_path)) {
				_loader_log(loader,
				            LOG_ERR,
				            "failed to locate %s."KMODULE_FILE_NAME_EXTENSION,
				            name);
				goto end;
			}
			return kmodule_open(loader, module, NULL, (char *) &module->_path);
		} else
			goto end;
	}

	/* copy the path, to prevent use-after-free errors */
	if ((char *) &module->_path != path)
		(void) strcpy((char *) &module->_path, path);
	module->path = (char *) &module->_path;

	/* initialize the other structure members */
	module->mod_contents = NULL;
	module->name = name;
	module->loader = loader;

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool _get_module_contents(kmodule_t *module) {
	if (NULL != module->mod_contents)
		return true;

	/* read the module */
	if (false == file_read(&module->file, module->path))
		return false;

	return true;
}

void kmodule_close(kmodule_t *module) {
	/* free the kernel module contents */
	if (NULL != module->mod_contents)
		file_close(&module->file);
}

bool _find_info_field(kmodule_t *module,
                      const char *name,
                      kmodule_var_t *field) {
	/* the return value */
	bool is_success = false;

	/* a loop index */
	size_t j;

	/* field name length */
	int length;

	/* a values array */
	const char **values;

	/* read the module contents */
	if (false == _get_module_contents(module)) {
		_loader_log(module->loader, LOG_ERR, "failed to read %s", module->path);
		goto end;
	}

	/* get the field name length */
	length = strlen(name);

	/* initialize the field values array */
	field->values = NULL;
	field->count = 0;

	for (j = 0; (module->mod_size - length) > j; ++j) {
		/* skip NULL bytes */
		if ('\0' == module->mod_contents[j])
			continue;

		/* compare the kernel module contents at the current offset with the
		 * field name */
		if (0 != strncmp(name, (char *) &module->mod_contents[j], length))
			continue;

		/* if the field name is not followed by a '=' character, continue to
		 * the next offset */
		if ('=' != module->mod_contents[length + j])
			continue;

		/* enlarge the field values array */
		values = realloc(field->values,
		                 (ELEMENT_SIZE(field->values) * (1 + field->count)));
		if (NULL == values)
			goto end;
		field->values = values;

		/* save a pointer to the character right after the '=' */
		field->values[field->count] = \
		              (char *) &module->mod_contents[sizeof(char) + length + j];
		++(field->count);
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

void _free_info_field(kmodule_var_t *field) {
	/* free the values array */
	if (NULL != field->values)
		free(field->values);
}

bool kmodule_info_get(kmodule_t *module, kmodule_info_t *info) {
	/* the return value */
	bool is_success = false;

	/* a loop index */
	unsigned int i;

	/* locate all fields */
	for (i = 0; ARRAY_SIZE(g_kmodule_fields) > i; ++i) {
		if (false == _find_info_field(module,
		                              g_kmodule_fields[i],
		                              &info->_fields[i]))
			goto end;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

void kmodule_info_free(kmodule_info_t *info) {
	/* a loop index */
	unsigned int i;

	/* free all fields */
	for (i = 0; ARRAY_SIZE(info->_fields) > i; ++i)
		_free_info_field(&info->_fields[i]);
}

char *_get_module_dependencies(kmodule_t *module) {
	/* the kernel module dependencies list */
	FILE *dependencies_list;

	/* a line in the kernel modules list */
	char line[PATH_MAX];

	/* the line length */
	int line_length;

	/* the kernel module dependencies */
	char *dependencies;

	/* the return value */
	char *return_value = NULL;

	/* open the kernel modules list */
	dependencies_list = _get_dependencies_list(module->loader);
	if (NULL == dependencies_list)
		goto end;

	do {
		/* read one line */
		if (NULL == fgets((char *) &line, sizeof(line), dependencies_list))
			goto end;

		/* strip the trailing line break, if there is any */
		line_length = strlen(line);
		if ('\n' == line[line_length - sizeof(char)])
			line[line_length - sizeof(char)] = '\0';

		/* locate the separator between the module path and its dependencies */
		dependencies = strchr((char *) &line, KMODULE_LIST_DELIMETER);
		if (NULL == dependencies)
			goto end;

		/* replace the separator with a NULL byte and skip it */
		dependencies[0] = '\0';
		++dependencies;

		/* check whether the path matches */
		if (0 != strcmp((char *) &line, module->path))
			continue;

		/* return a writable copy of the dependencies list */
		return_value = strdup(dependencies);

		break;
	} while (1);

end:
	return return_value;
}

bool _load_module(kmodule_t *module,
                  bool with_dependencies) {
	/* the return value */
	bool is_success = false;

	/* the kernel module dependencies */
	char *dependencies = NULL;

	/* a dependency module */
	char *dependency;

	/* strtok_r()'s position within the dependencies string */
	char *position;

	if (true == with_dependencies) {
		/* obtain the kernel module dependencies; if it's either missing or
		 * empty, just load the module itself */
		dependencies = _get_module_dependencies(module);
		if (NULL == dependencies)
			goto end;

		/* start splitting the dependencies list */
		dependency = strtok_r(dependencies, ",", &position);
		if (NULL == dependency)
			goto load_module;

		/* load each dependency */
		do {
			if (false == kmodule_load(module->loader, dependency, NULL, true))
				goto end;
			dependency = strtok_r(NULL, ",", &position);
		} while (NULL != dependency);
	}

load_module:
	/* read the module contents */
	if (false == _get_module_contents(module))
		goto free_dependencies;

	/* load the module itself; if it's already loaded, it's fine */
	if (-1 == syscall(SYS_init_module,
	                  module->mod_contents,
	                  module->mod_size,
	                  "")) {
		if (EEXIST != errno)
			goto free_dependencies;
	}

	/* report success */
	is_success = true;

free_dependencies:
	/* free the dependencies string */
	if (true == with_dependencies)
		free(dependencies);

end:
	if (false == is_success)
		_loader_log(module->loader, LOG_ERR, "failed to load %s", module->path);
	else
		_loader_log(module->loader, LOG_INFO, "loaded %s", module->path);

	return is_success;
}

bool _is_module_loaded(kmodule_loader_t *loader,
                       const char *name,
                       const char *path) {
	/* the return value */
	bool is_loaded = false;

	/* the list of loaded modules */
	FILE *loaded_modules;

	/* a line in the loaded modules list */
	char line[1 + KMODULE_MAX_LOADED_MODULE_ENTRY_LENGTH];

	/* the position of a delimeter in a string */
	char *delimeter;

	/* the file base name */
	char base_name[PATH_MAX];

	if (NULL == name) {
		/* copy the path, so it can be modified by basename() */
		(void) strcpy((char *) &base_name, path);

		/* get the base file name */
		name = basename((char *) &base_name);

		/* strip the extension */
		delimeter = strrchr(name, '.');
		if (NULL == delimeter)
			goto end;
		delimeter[0] = '\0';
	}

	/* open the list of loaded modules */
	loaded_modules = _get_loaded_modules_list(loader);
	if (NULL == loaded_modules)
		goto end;

	do {
		/* read an entry */
		if (NULL == fgets((char *) &line, STRLEN(line), loaded_modules))
			break;

		/* separate the module name and terminate it */
		delimeter = strchr((char *) &line, ' ');
		if (NULL == delimeter)
			continue;
		delimeter[0] = '\0';

		/* if the module name matches, stop */
		if (0 == strcmp((char *) &line, name)) {
			is_loaded = true;
			break;
		}
	} while (1);

end:
	return is_loaded;
}

bool _kmodule_actually_load(kmodule_loader_t *loader,
                            const char *name,
                            const char *path,
                            bool with_dependencies) {
	/* the return value */
	bool is_success = false;

	/* the kernel module */
	kmodule_t module;

	/* read the kernel module */
	if (false == kmodule_open(loader, &module, name, path))
		goto end;

	/* load the module */
	if (false == _load_module(&module, with_dependencies))
		goto close_module;

	/* report success */
	is_success = true;

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return is_success;
}

bool kmodule_load(kmodule_loader_t *loader,
                  const char *name,
                  const char *path,
                  bool with_dependencies) {
	/* if the module is already loaded, do nothing and report success */
	if (true == _is_module_loaded(loader, name, path))
		return true;

	/* otherwise, load it */
	return _kmodule_actually_load(loader, name, path, with_dependencies);
}

bool _does_alias_match(const char *alias, kmodule_info_t *module_info) {
	/* the return value */
	bool does_match = false;

	/* a loop index */
	unsigned int i;

	/* match the given alias against all aliases provided by the module */
	for (i = 0; module_info->mod_alias.count > i; ++i) {
		if (0 == fnmatch(module_info->mod_alias.values[i], alias, 0)) {
			does_match = true;
			break;
		}
	}

	return does_match;
}

bool _find_module_for_alias(kmodule_loader_t *loader,
                            const char *alias,
                            kmodule_t *module) {
	/* the return value */
	bool is_found = false;

	/* the module aliases list */
	FILE *aliases_list;

	/* a line in the aliases list */
	char line[PATH_MAX];

	/* the line length */
	int line_length;

	/* the kernel module path */
	char *path;

	/* open the module aliases list */
	aliases_list = _get_aliases_list(loader);
	if (NULL == aliases_list)
		goto end;

	do {
		/* read one line */
		if (NULL == fgets((char *) &line, sizeof(line), aliases_list))
			goto end;

		/* strip the trailing line break, if there is any */
		line_length = strlen(line);
		if ('\n' == line[line_length - sizeof(char)])
			line[line_length - sizeof(char)] = '\0';

		/* locate the separator between the alias and the module path */
		path = strchr((char *) &line, KMODULE_LIST_DELIMETER);
		if (NULL == path)
			goto end;

		/* replace the separator with a NULL byte and skip it */
		path[0] = '\0';
		++path;

		/* check whether the alias matches */
		if (0 != fnmatch((char *) &line, alias, 0))
			continue;

		/* open the kernel module */
		if (false == kmodule_open(loader, module, NULL, path))
			continue;

		/* report success */
		is_found = true;
		break;
	} while (1);

end:
	return is_found;
}

bool kmodule_load_by_alias(kmodule_loader_t *loader, const char *alias) {
	/* the return value */
	bool is_success = false;

	/* the kernel module */
	kmodule_t module;

	/* locate the most appropriate module for the given alias */
	if (false == _find_module_for_alias(loader, alias, &module)) {
		_loader_log(loader,
		            LOG_ERR,
		            "failed to find a kernel module for %s",
		            alias);
		goto end;
	}

	/* if the module is already loaded, do nothing */
	if (false == _is_module_loaded(loader, module.name, module.path)) {

		/* otherwise, load the kernel module and its dependencies */
		if (false == _load_module(&module, true))
			goto close_module;
	}

	/* report success */
	is_success = true;

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return is_success;
}
