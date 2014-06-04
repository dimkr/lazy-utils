#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <libgen.h>
#include <errno.h>

#include "common.h"
#include "module.h"

bool module_open(module_t *module, const char *path) {
	/* a loop index */
	size_t i = 0;

	/* the module file name extension */
	char *extension = NULL;

	assert(NULL != module);
	assert(NULL != path);

	/* get the module size */
	if (-1 == stat(path, &module->attributes)) {
		goto failure;
	}

	/* copy the module path */
	module->path = strdup(path);
	if (NULL == module->path) {
		goto failure;
	}

	/* get the module name */
	module->name = basename(module->path);
	if (NULL == module->name) {
		goto free_path;
	}

	/* strip the module file name extension */
	extension = strchr(module->name, '.');
	if (NULL == extension) {
		goto free_path;
	}
	extension[0] = '\0';

	/* replace all hyphens in the module name with underscores */
	for ( ; strlen(module->name) > i ; ++i) {
		if ('-' == module->name[i]) {
			module->name[i] = '_';
		}
	}

	/* open the module for reading */
	module->fd = open(path, O_RDONLY);
	if (-1 == module->fd) {
		goto free_path;
	}

	/* map the module to memory*/
	module->contents = mmap(NULL,
	                        (size_t) module->attributes.st_size,
	                        PROT_READ,
	                        MAP_PRIVATE,
	                        module->fd,
	                        0);
	if (NULL == module->contents) {
		goto close_module;
	}

	return true;

	/* unmap the module contents */
	(void) munmap(module->contents, (size_t) module->attributes.st_size);

close_module:
	/* close the module */
	(void) close(module->fd);

free_path:
	/* free the module path */
	free(module->path);

failure:
	return false;
}

void module_close(module_t *module) {
	assert(NULL != module);
	assert(NULL != module->contents);
	assert(NULL != module->path);

	/* unmap the module contents */
	(void) munmap(module->contents, (size_t) module->attributes.st_size);

	/* close the module */
	(void) close(module->fd);

	/* free the module path */
	free(module->path);
}

bool module_load(module_t *module) {
	assert(NULL != module);
	assert(NULL != module->contents);
	assert(0 < module->attributes.st_size);

	if (-1 == syscall(SYS_init_module,
	                  module->contents,
	                  (unsigned long) module->attributes.st_size,
	                  "")) {
		if (EEXIST != errno) {
			return false;
		}
	}

	return true;
}

bool module_for_each_dependency(module_t *module,
                                const dependency_callback_t callback,
                                void *arg) {
	/* the current offset within the module contents */
	off_t offset = 0;

	/* the module dependencies */
	char *dependencies = NULL;

	/* strtok_r()'s position within the dependencies list */
	char *position = NULL;

	/* a single dependency */
	char *dependency = NULL;

	/* the return value */
	bool result = false;

	assert(NULL != module);
	assert(NULL != module->contents);
	assert(STRLEN("depends=") < module->attributes.st_size);
	assert(NULL != callback);

	/* find the module dependencies */
	for ( ;
	     module->attributes.st_size - STRLEN("depends=") > offset;
	     ++offset) {
		if (0 == strncmp((char *) (module->contents + offset),
		                 "depends=",
		                 STRLEN("depends="))) {
			dependencies = (char *) (module->contents + \
			                         offset + \
			                         STRLEN("depends="));
			break;
		}
	}

	/* if the dependencies were not found, report failure */
	if (NULL == dependencies) {
		goto end;
	}

	/* if the dependencies list is empty, report success */
	if ('\0' == dependencies[0]) {
		result = true;
		goto end;
	}

	/* duplicate the dependencies list */
	dependencies = strdup(dependencies);
	if (NULL == dependencies) {
		goto end;
	}

	/* run the callback for each dependency */
	dependency = strtok_r(dependencies, ",", &position);
	while (NULL != dependency) {
		if (false == callback(dependency, arg)) {
			goto free_dependencies;
		}
		dependency = strtok_r(NULL, ",", &position);
	}

	/* report success */
	result = true;

free_dependencies:
	/* free the dependencies list */
	free(dependencies);

end:
	return result;
}

bool module_for_each_alias(module_t *module,
                           const alias_callback_t callback,
                           void *arg) {
	/* the current offset within the module contents */
	off_t offset = 0;

	assert(NULL != module);
	assert(NULL != module->contents);
	assert(STRLEN("depends=") < module->attributes.st_size);
	assert(NULL != callback);

	/* find the module aliases and run the callback for each */
	for ( ;
	     module->attributes.st_size - STRLEN("alias=") > offset;
	     ++offset) {
		if (0 == strncmp((char *) (module->contents + offset),
		                 "alias=",
		                 STRLEN("alias="))) {
			if (false == callback(module->name,
			                      (char *) (module->contents + \
			                                offset + \
			                                STRLEN("alias=")),
			                      arg)) {
				return false;
			}
		}
	}

	return true;
}
