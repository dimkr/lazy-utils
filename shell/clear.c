#include <stdlib.h>
#include <unistd.h>
#include <liblazy/common.h>

/* the VT100 escape sequence used to clear the screen */
#define ESCAPE_SEQUENCE "\033[2J"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* write the escape sequence to standard output */
	if (STRLEN(ESCAPE_SEQUENCE) != write(STDOUT_FILENO,
	                                     ESCAPE_SEQUENCE,
	                                     STRLEN(ESCAPE_SEQUENCE)))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
