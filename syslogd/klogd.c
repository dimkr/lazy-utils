#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>

/* the maximum size of a message */
#define MAX_MESSAGE_SIZE (4096)

/* the log message source */
#define LOG_IDENTITY "kernel"

void _close_system_log() {
	/* write another log message, which indicates when the daemon terminated */
	syslog(LOG_NOTICE, "klogd has terminated");

	/* close the system log */
	closelog();

	/* re-enable writing of kernel log messages to the console */
	(void) klogctl(7, NULL, 0);
}

void _close_kernel_log() {
	/* close the kernel file */
	(void) klogctl(0, NULL, 0);
}

void _signal_handler(int signal_type) {
	/* free all resources */
	_close_system_log();
	_close_kernel_log();

	/* terminate */
	exit(EXIT_SUCCESS);
}

int main() {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a message */
	char message[MAX_MESSAGE_SIZE];

	/* the size of a message */
	int message_size;

	/* a signal action */
	struct sigaction signal_action;

	/* add SIGINT and SIGTERM to the mask of signals blocked during the signal
	 * handler execution */
	if (-1 == sigemptyset(&signal_action.sa_mask))
		goto end;
	if (-1 == sigaddset(&signal_action.sa_mask, SIGINT))
		goto end;
	if (-1 == sigaddset(&signal_action.sa_mask, SIGTERM))
		goto end;

	/* open the kernel log */
	if (-1 == klogctl(1, NULL, 0))
		goto end;

	/* disable writing of kernel log messages to the console */
	if (-1 == klogctl(6, NULL, 0))
		goto close_kernel_log;

	/* open the system log */
	openlog(LOG_IDENTITY, 0, LOG_KERN);

	/* write a log message which indicates when the daemon started running */
	syslog(LOG_NOTICE, "klogd has started");

	/* daemonize */
	if (-1 == daemon(0, 0))
		goto close_system_log;

	/* assign signal handlers for both SIGINT and SIGTERM */
	signal_action.sa_flags = 0;
	signal_action.sa_handler = _signal_handler;
	if (-1 == sigaction(SIGINT, &signal_action, NULL))
		goto close_system_log;
	if (-1 == sigaction(SIGTERM, &signal_action, NULL))
		goto close_system_log;

	do {
		/* read a message */
		message_size = klogctl(2, (char *) &message, sizeof(message));
		switch (message_size) {
			case (-1):
				goto close_system_log;

			case (0):
				break;

			default:
				/* pass the message to syslogd */
				syslog(LOG_INFO, "%s", (char *) &message);
				break;
		}
	} while (1);

close_system_log:
	_close_system_log();

close_kernel_log:
	_close_kernel_log();

end:
	return exit_code;
}
