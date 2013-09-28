#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the number of seconds to sleep */
	int interval;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* convert the interval to numeric form */
	interval = atoi(argv[1]);
	if (0 >= interval)
		goto end;

	/* read the file */
	if (0 != sleep((unsigned int) interval))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
