#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* move the specified file */
	if (-1 == rename(argv[1], argv[2]))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
