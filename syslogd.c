#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "daemon.h"

/* the socket path */
#define SOCKET_PATH "/dev/log"

/* the log file path */
#define LOG_FILE_PATH "/var/log/messages"

/* the log file permissions */
#define LOG_FILE_PERMISSIONS (0644)

/* the maximum length of a log message */
#define MAX_MESSAGE_LENGTH (MAX_LENGTH)

/* the usage message */
#define USAGE "Usage: syslogd\n"

int main(int argc, char *argv[]) {
	/* a log message */
	char message[1 + MAX_MESSAGE_LENGTH] = {'\0'};

	/* the Unix socket address */
	struct sockaddr_un unix_address = {0};

	/* the daemon data */
	daemon_t daemon_data = {{{0}}};

	/* the message size */
	ssize_t size = 0;

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the log file */
	int log_file = 0;

	/* a received signal */
	int received_signal = 0;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* open the log file */
	log_file = open(LOG_FILE_PATH,
	                O_WRONLY | O_APPEND | O_CREAT,
	                LOG_FILE_PERMISSIONS);
	if (-1 == log_file) {
			goto end;
	}

	/* create a Unix socket */
	daemon_data.fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == daemon_data.fd) {
		goto close_log;
	}

	/* bind the socket */
	unix_address.sun_family = AF_UNIX;
	(void) strcpy(unix_address.sun_path, SOCKET_PATH);
	if (-1 == bind(daemon_data.fd,
	               (struct sockaddr *) &unix_address,
	               sizeof(unix_address))) {
		goto close_unix;
	}

	/* initialize the daemon */
	if (false == daemon_init(&daemon_data)) {
		goto close_unix;
	}

	do {
		/* wait for a message */
		if (false == daemon_wait(&daemon_data, &received_signal)) {
			goto close_unix;
		}

		/* if the received signal is a termination one, report success */
		if (SIGTERM == received_signal) {
			exit_code = EXIT_SUCCESS;
			goto close_unix;
		}

		/* receive a log message */
		size = recvfrom(daemon_data.fd,
		                message,
		                (sizeof(message) - 1),
		                0,
		                NULL,
		                NULL);
		switch (size) {
			case (-1):
				if (EAGAIN != errno) {
					goto close_unix;
				}

				/* fall through */

			case 0:
				continue;
		}

		/* write the message to the log */
		if (size != write(log_file, message, (size_t) size)) {
			goto close_unix;
		}
	} while (1);

close_unix:
	/* close the Unix socket */
	(void) close(daemon_data.fd);

	/* delete the Unix socket */
	(void) unlink(SOCKET_PATH);

close_log:
	/* close the log file */
	(void) close(log_file);

end:
	return exit_code;
}
