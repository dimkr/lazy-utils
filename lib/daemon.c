#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <liblazy/daemon.h>

bool daemonize() {
	/* the return value */
	bool is_success = false;

	/* the process ID */
	pid_t pid;

	/* change the process working directory; it will be inherited by child
	 * processes */
	if (-1 == chdir(PROCESS_WORKING_DIRETORY))
		goto end;

	/* create a child process */
	pid = fork();
	switch (pid) {
		case (-1):
			goto end;

		case (0):
			break;

		default:
			(void) exit(EXIT_SUCCESS);
	}

	/* make the child process a session leader */
	if ((pid_t) -1 == setsid())
		goto end;

	/* create a child process, again */
	pid = fork();
	switch (pid) {
		case (-1):
			goto end;

		case (0):
			break;

		default:
			(void) exit(EXIT_SUCCESS);
	}

	/* replace the standard input pipe with /dev/null */
	if (-1 == close(STDIN_FILENO))
		goto end;
	if (-1 == open("/dev/null", O_RDONLY))
		goto end;

	/* do the same with the standard output and error output pipes */
	if (-1 == close(STDOUT_FILENO))
		goto end;
	if (STDOUT_FILENO != dup(STDIN_FILENO))
		goto end;
	if (-1 == close(STDERR_FILENO))
		goto end;
	if (STDERR_FILENO != dup(STDIN_FILENO))
		goto end;

	/* set the file permissions mask */
	(void) umask(0);

	/* report success */
	is_success = true;

end:
	return is_success;
}
