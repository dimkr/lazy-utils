#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

/* the environment variable containing the executable search path */
#define SEARCH_PATH_ENVIRONMENT_VARIABLE "PATH"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the executable search path */
	char *search_path;

	/* a directory under the search path */
	char *directory;

	/* strtok_r()'s position within the search path */
	char *position;

	/* the executable path */
	char executable_path[PATH_MAX];

	/* the executable details */
	struct stat executable_details;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* get the executable search path */
	search_path = getenv(SEARCH_PATH_ENVIRONMENT_VARIABLE);
	if (NULL == search_path)
		goto end;

	/* duplicate the environment variable value */
	search_path = strdup(search_path);
	if (NULL == search_path)
		goto end;

	/* terminate the first directory path */
	directory = strtok_r(search_path, ":", &position);
	if (NULL == directory)
		goto free_search_path;

	do {
		/* format the executable path */
		(void) sprintf((char *) &executable_path, "%s/%s", directory, argv[1]);

		/* if the executable does not exist, skip it */
		if (-1 == stat((char *) &executable_path, &executable_details))
			goto next;

		/* if the executable is not a regular file (or a link to a regular
		 * file), skip it */
		if (!(S_ISREG(executable_details.st_mode)))
			goto next;

		/* if the file is not executable, skip it */
		if ((0 == (S_IXUSR & executable_details.st_mode)) &&
		    (0 == (S_IXGRP & executable_details.st_mode)) &&
		    (0 == (S_IXOTH & executable_details.st_mode)))
			goto next;

		/* print the executable path */
		if (0 > printf("%s\n", (char *) &executable_path))
			goto free_search_path;

		break;

next:
		/* continue to the next directory */
		directory = strtok_r(NULL, ":", &position);
	} while (NULL != directory);

	/* report success */
	exit_code = EXIT_SUCCESS;

free_search_path:
	/* free the environment variable value copy */
	free(search_path);

end:
	return exit_code;
}
