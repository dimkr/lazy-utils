#include <stdlib.h>
#include <syslog.h>
#include <sys/klog.h>

#include "common.h"
#include "daemon.h"

/* the maximum length of a kernel log message */
#define MAX_MESSAGE_LENGTH (MAX_LENGTH)

/* the usage message */
#define USAGE "Usage: klogd\n"

int main(int argc, char *argv[]) {
	/* a log message */
	char message[1 + MAX_MESSAGE_LENGTH] = {'\0'};

	/* the message size */
	int size = 0;

	/* the message priority */
	int priority = 0;

	/* the message offset */
	unsigned int offset = 0;

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* open the kernel log */
	if (-1 == klogctl(1, NULL, 0)) {
		goto end;
	}

	/* disable output of kernel messages to the console */
	if (-1 == klogctl(6, NULL, 0)) {
		goto end;
	}

	/* open the system log */
	openlog("kernel", LOG_NDELAY, LOG_KERN);

	/* daemonize */
	if (false == daemon_daemonize(DAEMON_WORKING_DIRECTORY)) {
		goto close_log;
	}

	do {
		/* read a log message */
		size = klogctl(2, message, sizeof(message));
		switch (size) {
			case (-1):
				goto close_log;

			case 0:
				continue;
		}

		/* parse the message priority */
		priority = LOG_INFO;
		if (3 >= size) {
			offset = 0;
		} else {
			if (('<' == message[0]) && ('>' == message[2])) {
				priority = message[1] - '0';
				offset = 3;
			}
		}

		/* terminate the log message */
		message[size] = '\0';

		/* write the message to the system log */
		syslog(priority, message + offset);
	} while (1);

close_log:
	/* close the system log */
	closelog();

	/* re-enable output of kernel log messages */
	(void) klogctl(7, NULL, 0);

	/* close the kernel log */
	(void) klogctl(0, NULL, 0);

end:
	return exit_code;
}
