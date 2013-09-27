#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/syscall.h>
#include <liblazy/kmodule.h>

/* the application name in the system log */
#define LOG_IDENTITY "rmmod"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_USER);

	/* unload the kernel module */
	if (-1 == syscall(SYS_delete_module, argv[1], 0)) {
		syslog(LOG_ERR,
		       "failed to unload %s."KMODULE_FILE_NAME_EXTENSION,
		       argv[1]);
		goto close_system_log;
	}

	/* report success */
	syslog(LOG_INFO, "unloaded %s."KMODULE_FILE_NAME_EXTENSION, argv[1]);
	exit_code = EXIT_SUCCESS;

close_system_log:
	/* close the system log */
	closelog();

end:
	return exit_code;
}
