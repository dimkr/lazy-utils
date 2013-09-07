#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/reboot.h>

/* the init script path */
#define INIT_SCRIPT_PATH "/etc/init.d/startup"

/* the shutdown script path */
#define SHUTDOWN_SCRIPT_PATH "/etc/init.d/shutdown"

#define PRINT(x) (void) write(STDOUT_FILENO, x, sizeof(x))

void _signal_handler(int type, siginfo_t *info, void *context) {
	(void) waitpid(info->si_pid, NULL, WNOHANG);
}

bool _run_script(const char *path, pid_t *pid) {
	/* the return value */
	bool is_success = false;

	/* the process ID */
	pid_t parent_pid;

	/* get the process ID */
	parent_pid = getpid();

	/* create a child process */
	*pid = fork();
	switch (*pid) {
		case (-1):
			goto end;

		case (0):
			/* in the child process, run the init script */
			(void) execl(path, path, NULL);

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
	sigset_t signal_mask;

	/* the init script PID */
	pid_t script_pid;

	/* the signal received */
	siginfo_t received_signal;

	/* the command passed to reboot() */
	int reboot_command;

	/* assign a signal handler for SIGCHLD, which destroys zombie processes */
	signal_action.sa_flags = SA_SIGINFO;
	signal_action.sa_sigaction = _signal_handler;
	if (-1 == sigemptyset(&signal_action.sa_mask))
		goto end;
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL))
		goto end;

	/* block SIGUSR1 and SIGUSR2 signals */
	(void) memcpy(&signal_mask, &signal_action.sa_mask, sizeof(signal_mask));
	if (-1 == sigaddset(&signal_mask, SIGUSR1))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGUSR2))
		goto end;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto end;

	/* run the init script */
	if (false == _run_script(INIT_SCRIPT_PATH, &script_pid))
		goto end;

	/* wait until either SIGUSR1 or SIGUSR2 is received */
	if (0 == sigwaitinfo(&signal_mask, &received_signal)) {
		if (script_pid != received_signal.si_pid)
			exit_code = EXIT_SUCCESS;
	}

	/* ask all processes to terminate */
	PRINT("Terminating all processes\n");
	if (0 == kill(-1, SIGTERM)) {

		/* upon success, give them 2 seconds and kill them brutally */
		(void) sleep(2);
		(void) kill(-1, SIGKILL);
	}

	/* run the shutdown script */
	PRINT("Running the shutdown script\n");
	if (true == _run_script(SHUTDOWN_SCRIPT_PATH, &script_pid)) {

		/* wait for the shutdown script to terminate */
		(void) waitpid(script_pid, NULL, 0);
	}

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
	}
	(void) reboot(reboot_command);

end:
	return exit_code;
}
