#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

/* the temporary file path template */
#define TEMPORARY_FILE_PATH_TEMPLATE "/tmp/tmp.XXXXXX"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a flag which indicates whether the created file is a directory */
	bool is_directory = false;

	/* the created file path */
	char path[] = TEMPORARY_FILE_PATH_TEMPLATE;

	/* the path length */
	int length;

	/* the created file descriptor */
	int fd;

	/* parse the command-line */
	switch (argc) {
		case (2):
			if (0 != strcmp("-d", argv[1]))
				goto end;
			is_directory = true;
			break;

		case (1):
			break;

		default:
			goto end;
	}

	/* create a temporary file */
	if (true == is_directory) {
		if (NULL == mkdtemp((char *) &path))
			goto end;
	} else {
		fd = mkstemp((char *) &path);
		if (-1 == fd)
			goto end;
		(void) close(fd);
	}

	/* print the created file path */
	length = strlen((char *) &path);
	if ((ssize_t) length != write(STDOUT_FILENO,
	                              (char *) &path,
	                              (size_t) length))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
