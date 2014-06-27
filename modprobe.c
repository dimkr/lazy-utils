#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>

#include "common.h"
#include "modprobed.h"

/* the usage message */
#define USAGE "Usage: modprobe MODULE\n"

int main(int argc, char *argv[]) {
	/* the Unix socket address */
	struct sockaddr_un unix_address = {0};

	/* the name or alias size */
	size_t size = 0;

	/* the Unix socket */
	int unix_socket = (-1);

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* get the name or alias length */
	size = strlen(argv[1]);
	if (0 == size) {
		goto end;
	}

	/* create a Unix socket */
	unix_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == unix_socket) {
		goto end;
	}

	/* send the module name or alias to modprobed */
	unix_address.sun_family = AF_UNIX;
	(void) strcpy(unix_address.sun_path, MODPROBED_SOCKET_PATH);
	size *= sizeof(char);
	if ((ssize_t) size != sendto(unix_socket,
	                             argv[1],
	                             size,
	                             0,
	                             (struct sockaddr *) &unix_address,
	                             sizeof(unix_address))) {
		goto close_unix;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

close_unix:
	/* close the Unix socket */
	(void) close(unix_socket);

end:
	return exit_code;
}
