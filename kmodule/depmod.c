#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <fnmatch.h>
#include <string.h>
#include <liblazy/cache.h>
#include <liblazy/kmodule.h>

bool _cache_module(const char *path,
                   const char *pattern,
                   const int cache_file,
                   struct dirent *entry) {
	/* the return value */
	bool should_stop = true;

	/* the kernel module */
	kmodule_t module;

	/* the kernel module path */
	char module_path[PATH_MAX];

	/* a loop index */
	unsigned int i;

	/* the module name length */
	size_t length;

	/* if the file is not a kernel module, skip it */
	if (0 != fnmatch(pattern, (char *) &entry->d_name, 0))
		return false;

	/* obtain the full path to the kernel module */
	(void) sprintf((char *) &module_path,
	               "%s/%s",
	               path,
	               (char *) &entry->d_name);

	/* open the kernel module */
	if (false == kmodule_open_module_by_path(&module, (char *) &module_path))
		goto end;

	/* obtain the kernel module aliases */
	if (false == kmodule_get_info(&module))
		goto close_module;

	/* strip the file name extension */
	length = strlen((char *) &entry->d_name);
	length -= STRLEN(KERNEL_MODULE_FILE_NAME_EXTENSION) + sizeof(char);
	entry->d_name[length] = '\0';

	/* cache the kernel module path and its aliases */
	if (false == cache_file_add(
	                  cache_file,
	                  CACHE_TYPE_KERNEL_MODULE_PATH,
	                  (const unsigned char *) &entry->d_name,
	                  length,
	                  (const unsigned char *) &module_path,
	                  sizeof(char) * (1 + strlen((const char *) &module_path))))
		goto close_module;

	/* cache the module aliases */
	if (0 < module.mod_alias.count) {
		for (i = 0; module.mod_alias.count > i; ++i) {
			if (false == cache_file_add(
				       cache_file,
				       CACHE_TYPE_KERNEL_MODULE_ALIASES,
				       (const unsigned char *) &entry->d_name,
				       length,
				       (const unsigned char *) module.mod_alias.values[i],
				       sizeof(char) * (1 + strlen(module.mod_alias.values[i]))))
				goto close_module;
		}
	}

	/* if everything went fine, go on */
	should_stop = false;

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return should_stop;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the cache file */
	int cache_file;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* open the cache file */
	cache_file = open(KERNEL_MODULE_CACHE_PATH,
	                  O_CREAT | O_RDWR,
	                  S_IRUSR);
	if (-1 == cache_file)
		goto end;

	/* list all kernel modules */
	if (true == file_for_each(KERNEL_MODULE_DIRECTORY,
	                          "*."KERNEL_MODULE_FILE_NAME_EXTENSION,
	                          (void *) (intptr_t) cache_file,
	                          (file_callback_t) _cache_module))
		goto close_cache_file;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_cache_file:
	/* close the cache file */
	(void) close(cache_file);

end:
	return exit_code;
}
