#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "common.h"
#include "daemon.h"

/* the init script path */
#define INIT_SCRIPT_PATH "/etc/rc.d/rc.sysinit"

/* the shutdown script path */
#define SHUTDOWN_SCRIPT_PATH "/etc/rc.d/rc.shutdown"

bool _run_script(const char *path, pid_t *pid) {
	/* the return value */
	bool is_success = false;

	/* the process ID */
	pid_t parent_pid = (-1);

	/* get the process ID */
	parent_pid = getpid();

	/* create a child process */
	*pid = daemon_fork();
	switch (*pid) {
		case (-1):
			goto end;

		case 0:
			/* in the child process, run the init script */
			(void) execl(path, path, (char *) NULL);

			/* upon failure, send a signal to the parent process */
			(void) kill(parent_pid, SIGTERM);

			break;

		default:
			is_success = true;
			break;
	}

end:
	return is_success;
}

int main() {
	/* the process exit code */
	int exit_code = EXIT_FAILURE;

	/* a signal action */
	struct sigaction signal_action;

	/* a signal mask used for waiting */
	sigset_t signal_mask = {{0}};

	/* the init script PID */
	pid_t script_pid = -1;

	/* the signal received */
	siginfo_t received_signal = {0};

	/* the command passed to reboot() */
	int reboot_command = 0;

	/* prevent child processes from becoming zombie processes */
	signal_action.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
	signal_action.sa_sigaction = NULL;
	if (-1 == sigemptyset(&signal_action.sa_mask)) {
		goto end;
	}
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL)) {
		goto end;
	}

	/* block SIGUSR1 and SIGUSR2 signals */
	(void) memcpy(&signal_mask, &signal_action.sa_mask, sizeof(signal_mask));
	if (-1 == sigaddset(&signal_mask, SIGUSR1)) {
		goto end;
	}
	if (-1 == sigaddset(&signal_mask, SIGUSR2)) {
		goto end;
	}
	if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL)) {
		goto end;
	}

	/* run the init script */
	if (false == _run_script(INIT_SCRIPT_PATH, &script_pid)) {
		goto end;
	}

	/* wait until either SIGUSR1 or SIGUSR2 is received */
	if (0 == sigwaitinfo(&signal_mask, &received_signal)) {
		if (script_pid != received_signal.si_pid) {
			exit_code = EXIT_SUCCESS;
		}
	}

	/* ask all processes to terminate */
	PRINT("Terminating all processes\n");
	if (0 == kill(-1, SIGTERM)) {

		/* upon success, give them 2 seconds and kill them brutally */
		(void) sleep(2);
		(void) kill(-1, SIGKILL);
	}

	/* disable the SIGCHLD signal handler, to make it possible to wait for the
	 * shutdown script to terminate */
	if (-1 == sigemptyset(&signal_action.sa_mask)) {
		goto flush;
	}
	signal_action.sa_flags = 0;
	signal_action.sa_handler = SIG_IGN;
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL)) {
		goto flush;
	}

	/* run the shutdown script */
	PRINT("Running the shutdown script\n");
	if (true == _run_script(SHUTDOWN_SCRIPT_PATH, &script_pid)) {

		/* wait for the shutdown script to terminate */
		(void) waitpid(script_pid, NULL, 0);
	}

flush:
	/* flush all file systems */
	PRINT("Flushing file system buffers\n");
	sync();

	/* reboot or shut down the system */
	switch (received_signal.si_signo) {
		case SIGUSR1:
			PRINT("Shutting down\n");
			reboot_command = RB_POWER_OFF;
			break;

		case SIGUSR2:
			PRINT("Rebooting\n");
			reboot_command = RB_AUTOBOOT;
			break;

		default:
			goto end;
	}
	(void) reboot(reboot_command);

end:
	return exit_code;
}
