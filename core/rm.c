#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

bool _delete(const char *path, const bool is_recursive);

bool _empty_directory(const char *path) {
	/* the return value */
	bool is_success = false;

	/* a directory */
	DIR *directory;

	/* a file under the directory */
	struct dirent file;
	struct dirent *file_pointer;

	/* the working directory */
	char working_directory[PATH_MAX];

	/* get the current working directory */
	if (NULL == getcwd((char *) &working_directory, sizeof(working_directory)))
		goto end;

	/* make the directory the working directory */
	if (-1 == chdir(path))
		goto end;

	/* open it */
	directory = opendir(".");
	if (NULL == directory)
		goto change_working_directory;

	do {
		/* enumerate the directory contents */
		if (0 != readdir_r(directory, &file, &file_pointer))
			goto close_directory;
		if (NULL == file_pointer)
			break;

		/* ignore relative paths */
		if (0 == strcmp(".", (char *) &file_pointer->d_name))
			continue;
		if (0 == strcmp("..", (char *) &file_pointer->d_name))
			continue;

		/* delete the directory contents */
		if (false == _delete((char *) &file_pointer->d_name, true))
			goto close_directory;
	} while (1);

	/* report success */
	is_success = true;

close_directory:
	/* close the directory */
	(void) closedir(directory);

change_working_directory:
	/* return to the original working directory */
	if (-1 == chdir((char *) &working_directory))
		is_success = false;

end:
	return is_success;
}

bool _delete(const char *path, const bool is_recursive) {
	/* the return value */
	bool is_success = false;

	/* the deleted file information */
	struct stat deleted_file;

	/* obtain the deleted file details */
	if (-1 == lstat(path, &deleted_file))
		goto end;

	/* if the file is a regular file, delete it */
	if (!(S_ISDIR(deleted_file.st_mode))) {
		if (-1 == unlink(path))
			goto end;
	} else {
		/* otherwise, empty the directory */
		if (true == is_recursive) {
			if (false == _empty_directory(path))
				goto end;
		}

		/* delete the directory itself */
		if (-1 == rmdir(path))
			goto end;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the deleted file or directory */
	const char *path;

	/* a flag which indicates whether the deletion is recursive */
	bool is_recursive = false;

	/* parse the command-line */
	switch (argc) {
		case 2:
			path = argv[1];
			is_recursive = false;
			break;

		case 3:
			if (0 != strcmp("-r", argv[1]))
				goto end;
			path = argv[2];
			is_recursive = true;
			break;

		default:
			goto end;
	}

	/* delete the specified file or directory */
	if (false == _delete(path, is_recursive))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
