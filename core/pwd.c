#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the working directory */
	char working_directory[PATH_MAX];

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* get the working directory */
	if (NULL == getcwd((char *) &working_directory, sizeof(working_directory)))
		goto end;

	/* print the working direcotry */
	if (0 > printf("%s\n", (char *) &working_directory))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
