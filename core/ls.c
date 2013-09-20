#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

/* the fallback if no directory is specified */
#define DEFAULT_DIRCTORY "."

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a directory path */
	char *path = DEFAULT_DIRCTORY;

	/* a directory */
	DIR *directory;

	/* a file under the directory */
	struct dirent file;
	struct dirent *file_pointer;

	/* parse the command-line */
	switch (argc) {
		case (2):
			path = argv[1];
			break;

		case (1):
			break;

		default:
			goto end;
	}

	/* open the directory */
	directory = opendir(path);
	if (NULL == directory)
		goto end;

	do {
		/* read an entry; if the last file was reached or an error occurred,
		 * stop */
		if (0 != readdir_r(directory, &file, &file_pointer))
			goto close_directory;
		if (NULL == file_pointer)
			break;

		/* print the file name */
		if (0 > printf("%s  ", (char *) &file_pointer->d_name))
			goto close_directory;
	} while (1);

	/* print a line break */
	if (EOF == fputs("\n", stdout))
		goto close_directory;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_directory:
	/* close the directory */
	(void) closedir(directory);

end:
	return exit_code;
}
