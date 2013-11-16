#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

/* the maximum length of a command-line */
#define MAX_COMMAND_LINE_LENGTH (1024)

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the process ID */
	pid_t pid;

	/* a signal mask */
	sigset_t signal_mask;

	/* a received signal */
	siginfo_t received_signal;

	/* the process termination timeout */
	struct timespec timeout;

	/* the process command-line */
	char command_line[MAX_COMMAND_LINE_LENGTH];

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* convert the timeout to numeric form */
	timeout.tv_sec = atoi(argv[1]);
	if (0 >= timeout.tv_sec)
		goto end;

	/* format the shell command-line */
	if (sizeof(command_line) <= snprintf((char *) &command_line,
	                                     sizeof(command_line),
	                                     "exec %s",
	                                     argv[2]))
		goto end;

	/* add SIGCHLD to the signal mask */
	if (-1 == sigemptyset(&signal_mask))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGCHLD))
		goto end;

	/* block SIGCHLD signals */
	if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL))
		goto end;

	/* create a child process */
	pid = fork();
	switch (pid) {
		case (-1):
			goto end;

		case 0:
				/* execute the given command-line */
				(void) execlp("/bin/sh",
				              "sh",
				              "-c",
				              (char *) &command_line,
				              (char *) NULL);

				/* upon failure, terminate */
				(void) exit(EXIT_FAILURE);

				break;
	}

	/* wait until SIGCHLD is received */
	timeout.tv_nsec = 0;
	if (SIGCHLD == sigtimedwait(&signal_mask, &received_signal, &timeout)) {
		exit_code = EXIT_SUCCESS;
		goto end;
	}

	/* if the timeout expired, terminate the child process */
	if (EAGAIN == errno) {
		if (0 == kill(pid, SIGTERM))
			(void) waitpid(pid, NULL, 0);
	}

end:
	return exit_code;
}
