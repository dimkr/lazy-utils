#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "common.h"

/* the console device */
#define CONSOLE_DEVICE "/dev/console"

int main(int argc, char *argv[]) {
	/* the console device file descriptor */
	int console_fd = 0;

	/* make sure the number of command-line arguments is valid */
	if (2 > argc) {
		PRINT("Usage: cttyhack COMMAND\n");
		goto end;
	}

	/* make the process a session leader */
	if (-1 == setsid()) {
		goto end;
	}

	/* open the console device */
	console_fd = open(CONSOLE_DEVICE, O_RDWR);
	if (-1 == console_fd) {
		goto end;
	}

	/* set the process console */
	if (-1 == ioctl(console_fd, TIOCSCTTY, NULL)) {
		goto close_console;
	}

	/* attach all input and output pipes to the console device */
	if (-1 == dup2(console_fd, STDIN_FILENO)) {
		goto close_console;
	}
	if (-1 == dup2(console_fd, STDOUT_FILENO)) {
		goto close_console;
	}
	if (-1 == dup2(console_fd, STDERR_FILENO)) {
		goto close_console;
	}

	/* close the console device */
	(void) close(console_fd);

	/* execute the passed command-line */
	(void) execvp(argv[1], &argv[1]);

	goto end;

close_console:
	/* close the console device */
	(void) close(console_fd);

end:
	return EXIT_FAILURE;
}
