#include <stdlib.h>
#include <signal.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* send init a SIGUSR1 signal */
	if (-1 == kill(1, SIGUSR1))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
