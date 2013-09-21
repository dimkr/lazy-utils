#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>

/* the log message source */
#define LOG_IDENTITY "logd"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the message source */
	int source;

	/* the log file */
	int log_file;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* open the message source */
	source = open(argv[1], O_RDONLY);
	if (-1 == source)
		goto end;

	/* open the log file */
	log_file = open(argv[2], O_CREAT | O_APPEND | O_WRONLY, S_IWUSR | S_IRUSR);
	if (-1 == log_file)
		goto close_source;

	/* daemonize */
	if (false == daemonize())
		goto close_log_file;

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_KERN);

	/* write a log message which indicates when the daemon started running */
	syslog(LOG_INFO, "logd has started");

	/* log all data written to the message source until a termination signal is
	 * received */
	if (false == file_log_from_file(source, log_file))
		goto close_system_log;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_system_log:
	/* write another log message, which indicates when the daemon terminated */
	syslog(LOG_INFO, "logd has terminated");

	/* close the system log */
	closelog();

close_log_file:
	/* close the log file */
	(void ) close(log_file);

close_source:
	/* close the message source */
	(void) close(source);

end:
	return exit_code;
}
