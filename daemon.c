#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <paths.h>
#include <sched.h>

#include "daemon.h"

bool daemon_daemonize(const char *working_directory) {
	/* the return value */
	bool result = false;

	/* change the process working directory */
	if (-1 == chdir(working_directory)) {
		goto end;
	}

	/* create a child process */
	switch (fork()) {
		case (-1):
			goto end;

		case 0:
			break;

		default:
			exit(EXIT_SUCCESS);
	}

	/* make the child process a session leader */
	if (((pid_t) -1) == setsid()) {
		goto end;
	}

	/* create a child process, again */
	switch (fork()) {
		case (-1):
			goto end;

		case 0:
			break;

		default:
			exit(EXIT_SUCCESS);
	}

	/* replace the standard input pipe with /dev/null */
	if (-1 == close(STDIN_FILENO)) {
		goto end;
	}
	if (-1 == open(_PATH_DEVNULL, O_RDONLY)) {
		goto end;
	}

	/* do the same with the standard output and error output pipes */
	if (-1 == close(STDOUT_FILENO)) {
 		goto end;
	}
	if (-1 == open(_PATH_DEVNULL, O_WRONLY)) {
		goto end;
	}
	if (-1 == close(STDERR_FILENO)) {
		goto end;
	}
	if (STDERR_FILENO != dup(STDIN_FILENO)) {
		goto end;
	}

	/* make the daemon mount namespace private, so it's impossible to replace
	 * files it tries to access by mounting a file system */
	if (-1 == unshare(CLONE_NEWNS)) {
		goto end;
	}

	/* set the file permissions mask */
	(void) umask(0);

	/* report success */
	result = true;

end:
	return result;
}

bool daemon_init(daemon_t *daemon, const char *working_directory) {
	/* a signal action */
	struct sigaction signal_action = {{0}};

	/* the socket flags */
	int flags = 0;

	/* the return value */
	bool result = false;

	/* get the file descriptor flags */
	flags = fcntl(daemon->fd, F_GETFL);
	if (-1 == flags) {
		goto end;
	}

	/* daemonize */
	if (false == daemon_daemonize(working_directory)) {
		goto end;
	}

	/* pick the minimum real-time signal */
	daemon->io_signal = SIGRTMIN;

	/* block io_signal, SIGCHLD and SIGTERM signals */
	if (-1 == sigemptyset(&daemon->signal_mask)) {
		goto end;
	}
	if (-1 == sigaddset(&daemon->signal_mask, daemon->io_signal)) {
		goto end;
	}
	if (-1 == sigaddset(&daemon->signal_mask, SIGTERM)) {
		goto end;
	}
	if (-1 == sigprocmask(SIG_SETMASK, &daemon->signal_mask, NULL)) {
		goto end;
	}

	/* prevent child processes from becoming zombie processes */
	signal_action.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
	signal_action.sa_sigaction = NULL;
	if (-1 == sigemptyset(&signal_action.sa_mask)) {
		goto end;
	}
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL)) {
		goto end;
	}

	/* set the file descriptor I/O signal */
	if (-1 == fcntl(daemon->fd, F_SETSIG, daemon->io_signal)) {
 		goto end;
	}

	/* enable non-blocking, asynchronous I/O */
	if (-1 == fcntl(daemon->fd, F_SETFL, flags | O_NONBLOCK | O_ASYNC)) {
		goto end;
	}

	/* change the file descriptor ownership */
	if (-1 == fcntl(daemon->fd, F_SETOWN, getpid())) {
		goto end;
	}

	/* report success */
	result = true;

end:
	return result;
}

bool daemon_wait(const daemon_t *daemon, int *received_signal) {
	if (0 != sigwait(&daemon->signal_mask, received_signal)) {
		return false;
	}

	if ((daemon->io_signal != *received_signal) &&
	    (SIGTERM != *received_signal)) {
		return false;
	}

	return true;
}

pid_t daemon_fork() {
	/* a signal mask */
	sigset_t signal_mask = {{0}};

	/* the return value */
	pid_t pid = (-1);

	/* empty the signal mask */
	if (-1 == sigemptyset(&signal_mask)) {
		return (-1);
	}

	/* spawn a child process */
	pid = fork();
	if (0 == pid) {
		/* reset the child process signal mask */
		if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL)) {
			exit(EXIT_FAILURE);
		}
	}

	return pid;
}
