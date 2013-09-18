#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <liblazy/io.h>

bool file_read(file_t *file, const char *path) {
	/* the return value */
	bool is_success = false;

	/* the file information */
	struct stat file_info;

	/* the file descriptor */
	int fd;

	/* get the file size */
	if (-1 == lstat(path, &file_info))
		goto end;

	/* open the file */
	fd = open(path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* map the file to memory */
	file->size = (size_t) file_info.st_size;
	file->contents = mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (NULL == file->contents)
		goto close_file;

	/* report success */
	is_success = true;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

void file_close(file_t *file) {
	/* unmap the file contents */
	(void) munmap(file->contents, file->size);
}

bool _plain_search_callback(const char *path,
                            const char *name,
                            char *full_path,
                            struct dirent *entry) {
	/* if the file name matches, return its full path */
	if (0 != strcmp(name, (char *) &entry->d_name))
		return false;

	(void) sprintf(full_path, "%s/%s", path, (char *) &entry->d_name);
	return true;
}

bool file_for_each(const char *path,
                   const char *name,
                   void *parameter,
                   file_callback_t callback) {
	/* the return value */
	bool is_found = false;

	/* the directory */
	DIR *directory;

	/* the directory contents */
	struct dirent contents;
	struct dirent *contents_pointer;

	/* sub-directory path */
	char *sub_directory = NULL;

	/* open the directory */
	directory = opendir(path);
	if (NULL == directory)
		goto end;

	do {
		/* list one entry under the directory */
		if (0 != readdir_r(directory, &contents, &contents_pointer))
			goto free_sub_directory_path;

		/* if the last entry was reached without a match, report failure */
		if (NULL == contents_pointer)
			goto free_sub_directory_path;

		if ((0 == strcmp(".", (char *) &contents_pointer->d_name)) ||
		    (0 == strcmp("..", (char *) &contents_pointer->d_name)))
			continue;

		switch (contents_pointer->d_type) {
			case DT_REG:
				/* run the callback */
				if (true == callback(path, name, parameter, contents_pointer))
					goto success;
				break;

			case DT_DIR:
				/* allocate memory for the sub-directory path */
				if (NULL == sub_directory) {
					sub_directory = malloc(sizeof(char) * PATH_MAX);
					if (NULL == sub_directory)
						goto free_sub_directory_path;
				}

				/* obtain the full sub-directory path */
				(void) sprintf(sub_directory,
				               "%s/%s",
				               path,
				               (char *) &contents_pointer->d_name);

				/* recurse into the sub-directory */
				if (true == file_for_each(sub_directory,
				                          name,
				                          parameter,
				                          callback))
					goto success;
				break;
		}
	} while (1);

	goto free_sub_directory_path;

success:
	/* report success */
	is_found = true;

free_sub_directory_path:
	/* free the sub-directory path */
	if (NULL != sub_directory)
		free(sub_directory);

	/* close the directory */
	(void) closedir(directory);

end:
	return is_found;
}

bool file_find(const char *path, const char *name, char *full_path) {
	return file_for_each(path,
	                     name,
	                     full_path,
	                     (file_callback_t) _plain_search_callback);
}
