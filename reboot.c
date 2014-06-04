#include <stdlib.h>
#include <signal.h>

#include "common.h"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT("Usage: reboot\n");
		goto end;
	}

	/* send init a SIGUSR2 signal */
	if (-1 == kill(1, SIGUSR2)) {
		goto end;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
