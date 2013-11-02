#include <stdlib.h>
#include <syslog.h>

/* the log message source */
#define LOG_IDENTITY "logger"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY, LOG_USER);

	/* write the message to the system log */
	syslog(LOG_INFO, "%s\n", argv[1]);

	/* close the system log */
	closelog();

	/* report success, always */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
