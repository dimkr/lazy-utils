#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

/* the usage message */
#define USAGE "Usage: suspend\n"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the system state file */
	int fd = (-1);

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* open the system state file */
	fd = open("/sys/power/state", O_WRONLY);
	if (-1 == fd) {
		goto end;
	}

	/* suspend the system */
	if (STRLEN("mem") != write(fd, "mem", STRLEN("mem"))) {
		goto close_file;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

close_file:
	/* close the system state file */
	(void) close(fd);

end:
	return exit_code;
}
