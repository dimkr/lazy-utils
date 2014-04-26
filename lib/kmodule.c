#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <fnmatch.h>
#include <libgen.h>
#include <liblazy/io.h>
#include <liblazy/common.h>
#include <liblazy/kmodule.h>

bool kmodule_loader_init(kmodule_loader_t *loader) {
	/* the return value */
	bool is_success = false;

	/* open the list of loaded modules */
	loader->loaded_modules = fopen(LOADED_KERNEL_MODULES_LIST_PATH, "r");
	if (NULL == loader->loaded_modules)
		goto end;

	/* open the cache file */
	if (false == cache_open(&loader->cache, KERNEL_MODULE_CACHE_PATH))
		goto close_cache_file;

	/* report success */
	is_success = true;
	goto end;

close_cache_file:
	/* close the cache file */
	cache_close(&loader->cache);

end:
	return is_success;
}

void kmodule_loader_destroy(kmodule_loader_t *loader) {
	/* close the cache file */
	cache_close(&loader->cache);

	/* close the list of loaded modules */
	(void) fclose(loader->loaded_modules);
}

bool _get_module_name(const char *path, char *name) {
	/* the return value */
	bool is_success = false;

	/* a substring */
	char *position;

	/* copy the base file name and strip the extension */
	(void) strcpy(name, basename((char *) path));
	position = strchr(name, '.');
	if (NULL == position)
		goto end;
	position[0] = '\0';

	/* replace '-' with '_', as the kernel does in the list of loaded modules */
	do {
		position = strchr(name, '-');
		if (NULL == position)
			break;
		position[0] = '_';
	} while (1);

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool _open_module(kmodule_t *module) {
	/* the return value */
	bool is_success = false;

	/* a loop index */
	unsigned int i;

	/* get the module name */
	if (false == _get_module_name((char *) &module->path,
	                              (char *) &module->name))
		goto end;

	/* read the kernel module */
	if (false == file_read(&module->file, (char *) &module->path))
		goto end;

	/* initialize the module information fields */
	for (i = 0; ARRAY_SIZE(module->_fields) > i; ++i) {
		module->_fields[i].values = NULL;
		module->_fields[i].count = 0;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

void kmodule_close(kmodule_t *module) {
	/* loop indices */
	int i;

	/* free all module information fields */
	for (i = ARRAY_SIZE(module->_fields) - 1; 0 <= i; --i) {
		if (0 == module->_fields[i].count)
			continue;
		free(module->_fields[i].values);
	}

	/* close the kernel module */
	file_close(&module->file);
}

bool kmodule_open_module_by_path(kmodule_t *module, const char *path) {
	/* the return value */
	bool is_success = false;

	/* copy the module path */
	(void) strcpy((char *) &module->path, path);

	/* initialize the cached module details pointer */
	module->cache_entry = NULL;

	/* open the module */
	if (false == _open_module(module))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool kmodule_open_module_by_name(kmodule_loader_t *loader,
                                 kmodule_t *module,
                                 const char *name) {
	/* the return value */
	bool is_found = false;

	/* the kernel module path */
	char *path;

	/* the path length */
	size_t length;

	/* fetch the module path from the cache */
	if (false == cache_file_get_by_string(&loader->cache,
	                                      CACHE_TYPE_KERNEL_MODULE_PATH,
	                                      name,
	                                      (unsigned char **) &path,
	                                      &length))
		goto end;

	/* copy and terminate the path */
	(void) strncpy((char *) &module->path, path, length);
	module->path[length] = '\0';

	/* save a pointer to the cached module details */
	module->cache_entry = (cache_entry_header_t *) path;

	/* open the module */
	if (false == _open_module(module))
		goto end;

	/* report success */
	is_found = true;

end:
	return is_found;
}

typedef struct {
	const char *alias;
	uLong hash;
} _module_alias_match_t;

bool _does_module_match_alias(const cache_entry_header_t *entry,
                              const char *cached_alias,
                              _module_alias_match_t *match) {
	/* check whether given alias matches the cached one */
	if (0 != fnmatch(cached_alias, match->alias, 0))
		return false;

	/* save the module name hash */
	match->hash = entry->hash;

	return true;
}

bool _open_module_by_alias(kmodule_loader_t *loader,
                           kmodule_t *module,
                           const char *alias) {
	/* the return value */
	bool is_success = false;

	/* the module name search results */
	_module_alias_match_t match;

	/* the module path */
	char *path;

	/* find the module which matches the alias and obtain its name hash */
	match.alias = alias;
	if (false == cache_search(&loader->cache,
	                          CACHE_TYPE_KERNEL_MODULE_ALIASES,
	                          (cache_callback_t) _does_module_match_alias,
	                          &match))
		goto end;

	/* get the module path, using the hash */
	if (false == cache_file_get_by_hash(&loader->cache,
	                                    CACHE_TYPE_KERNEL_MODULE_PATH,
	                                    match.hash,
	                                    (unsigned char **) &path,
	                                    NULL))
		goto end;

	/* open the module */
	if (false == kmodule_open_module_by_path(module, path))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

char **_allocate_additional_value(kmodule_field_t *field) {
	/* the newly allocated value */
	char **new_value = NULL;

	/* the enlarged values array */
	char **values;

	/* the number of fields in the enlarged array */
	unsigned int count;

	/* enlarge the values array */
	count = 1 + field->count;
	values = realloc(field->values, ELEMENT_SIZE(field->values) * count);
	if (NULL == values)
		goto end;

	/* return a pointer to the new value */
	new_value = &values[field->count];
	field->values = values;
	field->count = count;

end:
	return new_value;
}

bool _get_field_values(kmodule_t *module,
                       kmodule_field_t *field,
                       const char *name) {
	/* the return value */
	bool is_success = false;

	/* the current position within the module contents */
	unsigned char *position;

	/* the current offset inside the module contents */
	size_t offset;

	/* the field value */
	char **value;

	/* the field name length */
	size_t length;

	/* if the field values were already found, do nothing */
	if (0 < field->count)
		goto success;

	/* get the field name length */
	length = strlen(name);

	/* start at the module beginning */
	position = module->file.contents;
	offset = 0;

	while ((module->file.size - length - (2 * sizeof(char))) > offset) {
		/* check whether the current position contains the field name */
		if (0 != strncmp((char *) position, name, length))
			goto next;

		/* check whether the field name ends with a '=' */
		if ('=' != position[length])
			goto next;

		/* allocate memory for an additional value */
		value = _allocate_additional_value(field);
		if (NULL == value)
			goto end;

		/* save a pointer to the field value */
		*value = (char *) &position[1 + length];

next:
		/* locate the first character of the field name */
		++position;
		offset = position - module->file.contents;
		position = memchr(position, name[0], module->file.size - offset);
		if (NULL == position)
			break;
	}

success:
	/* report succcess */
	is_success = true;

end:
	return is_success;
}

bool kmodule_get_info(kmodule_t *module) {
	/* the return value */
	bool is_success = false;

	/* a loop index */
	unsigned int i;

	/* get the values of all fields */
	for (i = 0; ARRAY_SIZE(g_kmodule_fields) > i; ++i) {
		if (false == _get_field_values(module,
		                               &module->_fields[i],
		                               g_kmodule_fields[i]))
			goto end;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

char *_get_dependencies(kmodule_t *module) {
	/* the module dependencies */
	char *dependencies = NULL;

	/* locate the module dependencies field */
	if (false == _get_field_values(module,
	                               &module->mod_depends,
	                               KERNEL_MODULE_DEPENDENCIES_FIELD))
		goto end;

	/* assume the dependencies field appears only once */
	if (0 == module->mod_depends.count)
		goto end;
	dependencies = module->mod_depends.values[0];

end:
	return dependencies;
}

bool _is_module_loaded(kmodule_loader_t *loader, const kmodule_t *module) {
	/* the return value */
	bool is_loaded = false;

	/* a line in the list of loaded modules */
	char line[LOADED_KERNEL_MODULES_LIST_MAX_ENTRY_SIZE];

	/* a substring */
	char *position;

	/* start at the list head */
	rewind(loader->loaded_modules);

	do {
		/* read an entry */
		if (NULL == fgets((char *) &line, sizeof(line), loader->loaded_modules))
			break;

		/* locate the first space and terminate the module name */
		position = strchr((char *) &line, ' ');
		if (NULL == position)
			break;
		position[0] = '\0';

		/* compare the module names */
		if (0 == strcmp((char *) module->name, (char *) &line)) {
			is_loaded = true;
			break;
		}
	} while (1);

	return is_loaded;
}

bool _is_blacklisted(const cache_entry_header_t *entry,
                     const char *value,
                     void *hash) {
	if ((uLong) ((intptr_t) hash) == entry->hash)
		return true;

	return false;
}

bool _is_module_blacklisted(kmodule_loader_t *loader, const kmodule_t *module) {
	return cache_search(&loader->cache,
	                    CACHE_TYPE_KERNEL_MODULE_BLACKLIST,
	                    (cache_callback_t) _is_blacklisted,
	                    (void *) (intptr_t) HASH_STRING(module->name));
}

bool _should_load(kmodule_loader_t *loader, const kmodule_t *module) {
	if (true == _is_module_loaded(loader, module))
		return false;

	if (true == _is_module_blacklisted(loader, module))
		return false;

	return true;
}

bool _load_module(kmodule_loader_t *loader,
                  kmodule_t *module,
                  const char *parameters,
                  bool with_dependencies) {
	/* the return value */
	bool is_success = false;

	/* the module dependencies */
	char *dependencies;

	/* a single dependency */
	char *dependency;

	/* strtok_r()'s current position within the dependencies list */
	char *position;

	/* if the module should not be loaded, do nothing */
	if (false == _should_load(loader, module)) {
		is_success = true;
		goto end;
	}

	if (false == with_dependencies)
		dependencies = NULL;
	else {
		/* get the module dependencies */
		dependencies = _get_dependencies(module);

		if (NULL != dependencies) {
			/* obtain a writable copy of the dependencies list */
			dependencies = strdup(dependencies);
			if (NULL == dependencies)
				goto end;

			/* start splitting the dependencies list; if it's empty, do
			 * nothing */
			dependency = strtok_r(dependencies, ",", &position);
			if (NULL == dependency)
				goto load_module;

			/* load each dependency */
			do {
				if (false == kmodule_load_by_name(loader, dependency, "", true))
					goto free_dependencies;
				dependency = strtok_r(NULL, ",", &position);
			} while (NULL != dependency);
		}
	}

load_module:
	/* load the module */
	if (-1 == syscall(SYS_init_module,
	                  module->file.contents,
	                  module->file.size,
	                  parameters))
		goto free_dependencies;

	/* report success */
	is_success = true;

free_dependencies:
	/* free the module dependencies list */
	if (NULL != dependencies)
		free(dependencies);

end:
	return is_success;
}

bool kmodule_load_by_name(kmodule_loader_t *loader,
                          const char *name,
                          const char *parameters,
                          bool with_dependencies) {
	/* the return value */
	bool is_success = false;

	/* the kernel module */
	kmodule_t module;

	/* locate the kernel module */
	if (false == kmodule_open_module_by_name(loader, &module, name))
		goto end;

	/* load the module */
	if (false == _load_module(loader, &module, parameters, with_dependencies))
		goto close_module;

	/* report success */
	is_success = true;

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return is_success;
}

bool kmodule_load_by_alias(kmodule_loader_t *loader,
                           const char *alias,
                           const char *parameters,
                           bool with_dependencies) {
	/* the return value */
	bool is_success = false;

	/* the kernel module */
	kmodule_t module;

	/* locate the kernel module */
	if (false == _open_module_by_alias(loader, &module, alias))
		goto end;

	/* load the module */
	if (false == _load_module(loader, &module, parameters, with_dependencies))
		goto close_module;

	/* report success */
	is_success = true;

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return is_success;
}
