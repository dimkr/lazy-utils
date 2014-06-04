#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fnmatch.h>

#if defined(__GLIBC__) && (!defined(_DIRENT_HAVE_D_TYPE))
#	error "the d_type member of struct dirent is required"
#endif

#include "common.h"
#include "find.h"

bool find_all(const char *directory,
              const char *pattern,
              const file_callback_t callback,
              void *arg) {
	/* a file path */
	char path[PATH_MAX] = {'\0'};

	/* the directory handle */
	DIR *handle = NULL;

	/* a file under the directory */
	struct dirent entry = {0};
	struct dirent *file = NULL;

	/* the return value */
	bool result = false;

	/* open the directory */
	handle = opendir(directory);
	if (NULL == handle) {
		goto end;
	}

	do {
		/* read the name of one file under the directory */
		if (0 != readdir_r(handle, &entry, &file)) {
			goto close_directory;
		}
		if (NULL == file) {
			break;
		}

		switch (file->d_type) {
			case DT_REG:
				/* if the file is a regular file, check whether its name
				 * matches the pattern */
				if (0 == fnmatch(pattern, file->d_name, FNM_NOESCAPE)) {

					/* format the file path */
					if (sizeof(path) <= snprintf(path,
					                             sizeof(path),
					                             "%s/%s",
					                             directory,
					                             file->d_name)) {
						goto close_directory;
					}

					/* run the callback */
					if (false == callback(path, arg)) {
						goto close_directory;
					}
				}
				break;

			case DT_DIR:
				/* ignore relative paths */
				if (0 == strcmp(".", file->d_name)) {
					continue;
				}
				if (0 == strcmp("..", file->d_name)) {
					continue;
				}

				/* format the sub-directory path */
				if (sizeof(path) <= snprintf(path,
				                             sizeof(path),
				                             "%s/%s",
				                             directory,
				                             file->d_name)) {
					goto close_directory;
				}

				/* recurse into the sub directory */
				if (false == find_all(path, pattern, callback, arg)) {
					goto close_directory;
				}

				break;
		}
	} while (1);

	/* report success */
	result = true;

close_directory:
	/* close the directory */
	(void) closedir(handle);

end:
	return result;
}
