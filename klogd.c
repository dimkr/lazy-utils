#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <syslog.h>
#include <unistd.h>

/* the maximum size of a message */
#define MAX_MESSAGE_SIZE (4096)

/* the log message source */
#define LOG_IDENTITY "kernel"

int main() {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a message */
	char message[MAX_MESSAGE_SIZE];

	/* the size of a message */
	int message_size;

	/* open the kernel log */
	if (-1 == klogctl(1, NULL, 0))
		goto end;

	/* disable writing of kernel log messages to the console */
	if (-1 == klogctl(6, NULL, 0))
		goto close_kernel_log;

	/* open the system log */
	openlog(LOG_IDENTITY, 0, LOG_KERN);

	/* daemonize */
	if (-1 == daemon(0, 0))
		goto close_system_log;

	do {
		/* read a message */
		message_size = klogctl(2, (char *) &message, sizeof(message));
		if (-1 == message_size)
			goto close_system_log;

		if (0 != message_size) {


			/* pass the message to syslogd */
			syslog(LOG_INFO, "%s", (char *) &message);
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_system_log:
	/* close the system log */
	closelog();

	/* re-enable writing of kernel log messages to the console */
	(void) klogctl(7, NULL, 0);

close_kernel_log:
	/* close the kernel file */
	(void) klogctl(0, NULL, 0);

end:
	return exit_code;
}
