#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the full path */
	char path[PATH_MAX];

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* obtain the full path */
	if (NULL == realpath(argv[1], (char *) &path))
		goto end;

	/* print the full path */
	if (0 > printf("%s\n", (char *) &path))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}