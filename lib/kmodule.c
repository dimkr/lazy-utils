#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <liblazy/io.h>
#include <liblazy/kmodule.h>

char *_locate_module_by_name(const char *name) {
	/* the return value */
	char *path = NULL;

	/* the kernel modules list */
	FILE *module_list;

	/* a line in the modules list */
	char line[PATH_MAX];

	/* the module path length */
	int path_length;

	/* open the module list */
	module_list = fopen(KMODULE_LIST_PATH, "r");
	if (NULL == module_list)
		goto end;

	do {
		/* read one line */
		if (NULL == fgets((char *) &line, sizeof(line), module_list))
			goto close_module_list;

		/* locate the module name */
		path = strstr((char *) &line, name);
		if (NULL == path)
			continue;

		/* if the modules list begins with the module name, it's corrupt */
		if (path == (char *) &line)
			goto close_module_list;

		/* make sure the module name is not a substring */
		if ('/' != path[-1])
			continue;

		/* strip the trailing line break, if there is any */
		path_length = strlen(path);
		if ('\n' == path[path_length - 1])
			path[path_length - 1] = '\0';

		/* return a pointer to the whole line */
		path = (char *) &line;
		break;
	} while (1);

	/* duplicate the path */
	path = strdup(path);

close_module_list:
	/* close the module list */
	(void) fclose(module_list);

end:
	return path;
}

bool kmodule_open(kmodule_t *module, const char *name, const char *path) {
	/* the return value */
	bool is_success = false;

	/* the kernel module file name */
	char name_with_extension[MAXNAMLEN];

	if (NULL == path) {
		/* format the module file name */
		(void) sprintf((char *) &name_with_extension,
		               "%s.%s",
		               name,
		               KMODULE_FILE_NAME_EXTENSION);

		/* if the module was specified by its name, locate it */
		if (NULL != name)
			return kmodule_open(
			             module,
			             NULL,
			             _locate_module_by_name((char *) &name_with_extension));
		else
			goto end;
	}

	/* read the module */
	module->contents = file_read(path, &module->size);
	if (NULL == module->contents)
		goto end;
	module->path = path;
	module->name = name;

	/* report success */
	is_success = true;

end:
	return is_success;
}

void _locate_fields(kmodule_t *module,
                    kmodule_info_t *info,
                    unsigned int count) {
	/* loop indices */
	unsigned int i;
	size_t j;

	/* field name length */
	int length;

	for (i = 0; count > i; ++i) {
		/* get the field name length */
		length = strlen(g_kmodule_fields[i]);

		for (j = 0; (module->size - length) > j; ++j) {
			/* skip NULL bytes */
			if ('\0' == module->contents[j])
				continue;

			/* compare the kernel module contents at the current offset with the
			 * field name */
			if (0 != strncmp(g_kmodule_fields[i],
			                 (char *) &module->contents[j],
			                 length))
				continue;

			/* if the field name is not followed by a '=' character, continue to
			 * the next offset */
			if ('=' != module->contents[length + j])
				continue;

			/* otherwise, save a pointer to the character right after the '=' */
			info->_fields[i] = (char *) &module->contents[1 + length + j];
			goto next;
		}

		/* if the field was not found, initialize its value with a NULL
		 * pointer */
		info->_fields[i] = NULL;

next:
		;
	}
}

void kmodule_get_info(kmodule_t *module, kmodule_info_t *info) {
	_locate_fields(module, info, ARRAY_SIZE(g_kmodule_fields));
}

char *kmodule_get_dependencies(kmodule_t *module) {
	/* the kernel module information */
	kmodule_info_t info;

	/* locate the module dependencies field */
	_locate_fields(module, &info, 1 + KMODULE_INFO_DEPENDENCIES_INDEX);

	return info.mod_depends;
}

void kmodule_close(kmodule_t *module) {
	/* free the buffer containing the kernel module contents */
	free(module->contents);
}

bool kmodule_load(const char *name) {
	/* the return value */
	bool is_success = false;

	/* the kernel module */
	kmodule_t kernel_module;

	/* the kernel module dependencies */
	char *dependencies;

	/* a dependency module */
	char *dependency;

	/* strtok_r()'s position within the dependencies string */
	char *position;

	/* read the kernel module */
	if (false == kmodule_open(&kernel_module, name, NULL))
		goto end;

	/* obtain the kernel module dependencies; if it's either missing or empty,
	 * just load the module itself */
	dependencies = kmodule_get_dependencies(&kernel_module);
	if (NULL == dependencies)
		goto load_module;

	/* start splitting the dependencies list */
	dependency = strtok_r(dependencies, ",", &position);
	if (NULL == dependency)
		goto load_module;

	/* load each dependency */
	do {
		if (false == kmodule_load(dependency))
			goto close_module;
		dependency = strtok_r(NULL, ",", &position);
	} while (NULL != dependency);

load_module:
	/* load the module itself; if it's already loaded, it's fine */
	if (-1 == syscall(SYS_init_module,
	                  kernel_module.contents,
	                  kernel_module.size,
	                  "")) {
		if (EEXIST != errno)
			goto close_module;
	}

	/* report success */
	is_success = true;

close_module:
	/* close the kernel module */
	kmodule_close(&kernel_module);

end:
	return is_success;
}
