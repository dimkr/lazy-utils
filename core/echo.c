#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a loop index */
	int i;
	
	/* make sure the command-line is valid */
	if (0 == argc)
		goto end;

	/* write all arguments to standard output */
	for (i = 0; argc > i; ++i) {
		if (0 > printf("%s\n", argv[i]))
			goto end;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
