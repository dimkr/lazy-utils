#include <stdlib.h>
#include <signal.h>

int main() {
	/* the exit code */
	int exit_code;

	/* send init a SIGUSR2 signal */
	if (0 == kill(1, SIGUSR2))
		exit_code = EXIT_SUCCESS;
	else
		exit_code = EXIT_FAILURE;

	return exit_code;
}
