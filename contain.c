#include <stdlib.h>
#include <sched.h>
#include <linux/sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mount.h>
#include <errno.h>
#include <sys/prctl.h>
#include <assert.h>

#include "common.h"

/* the usage message */
#define USAGE "Usage: contain ROOT COMMAND...\n"

/* the namespace flags of clone() */
#define NAMESPACES (CLONE_NEWIPC | \
                    CLONE_NEWNS | \
                    CLONE_NEWPID | \
                    CLONE_NEWUTS | \
                    CLONE_UNTRACED)

/* the child process stack size */
#define STACK_SIZE (8 * 1024)

/* possible exit codes, except EXIT_SUCCESS and EXIT_FAILURE */
enum exit_codes {
	EXIT_POWEROFF = 2,
	EXIT_REBOOT   = 3,
	EXIT_KILLED   = 4
};

/* the child process stack */
static unsigned char stack[STACK_SIZE] = {0};

static int _init(void *argv) {
	/* a signal mask */
	sigset_t signal_mask = {{0}};

	/* a received signal */
	siginfo_t received_signal = {0};

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the init script PID */
	pid_t script_pid = (-1);

	assert(NULL != argv);
	assert(NULL != ((char **) argv)[2]);

	/* block SIGCHLD, SIGUSR1 and SIGUSR2 signals */
	if (-1 == sigemptyset(&signal_mask)) {
		goto end;
	}
	if (-1 == sigaddset(&signal_mask, SIGCHLD)) {
		goto end;
	}
	if (-1 == sigaddset(&signal_mask, SIGUSR1)) {
		goto end;
	}
	if (-1 == sigaddset(&signal_mask, SIGUSR2)) {
		goto end;
	}
	if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL)) {
		goto end;
	}

	/* re-unmount /proc, so it doesn't mention processes running outside of the
	 * sandbox */
	if (-1 == umount("/proc")) {
		switch (errno) {
			case EINVAL:
				break;

			case ENOENT:
				goto run_script;

			default:
				goto end;
		}
	}
	if (-1 == mount("proc", "/proc", "proc", 0, NULL)) {
		goto end;
	}

run_script:
	/* spawn a child process, for the init script */
	script_pid = fork();
	switch (script_pid) {
		case (-1):
			goto end;

		case 0:
			/* reset the init script signal mask */
			if (-1 == sigemptyset(&signal_mask)) {
				goto terminate_script;
			}
			if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL)) {
				goto terminate_script;
			}

			/* run the init script */
			(void) execvp(((char **) argv)[2], &(((char *const *) argv)[2]));

terminate_script:
			exit(EXIT_FAILURE);
	}

	/* make the process a subreaper, so it reaps orphan processes */
	if (0 != prctl(PR_SET_CHILD_SUBREAPER, 1UL, 0UL, 0UL, 0UL)) {
		goto end;
	}

	do {
		/* wait for a signal */
		if (-1 == sigwaitinfo(&signal_mask, &received_signal)) {
			goto end;
		}

		switch (received_signal.si_signo) {
			/* destroy zombie processes */
			case SIGCHLD:
				if (received_signal.si_pid != waitpid(received_signal.si_pid,
				                                      NULL,
				                                      WNOHANG)) {
					if (ECHILD == errno) {
						continue;
					}
				}

				/* if the init script terminated, it's unclear what the init
				 * process should do - act as if reboot(8) was executed */
				if (script_pid == received_signal.si_pid) {
					exit_code = EXIT_REBOOT;
				}
				break;

			/* stop when anything but SIGCHLD is received */
			case SIGUSR1:
				exit_code = EXIT_POWEROFF;
				break;

			case SIGUSR2:
				exit_code = EXIT_REBOOT;
				break;

			case SIGTERM:
				exit_code = EXIT_KILLED;
				break;
		}
	} while (EXIT_FAILURE == exit_code);

	/* terminate all the processes inside the sandbox */
	if (0 == kill(-1, SIGTERM)) {
		(void) sleep(2);
		(void) kill(-1, SIGKILL);
	}

end:
	return exit_code;
}

int main(int argc, char *argv[]) {
	/* the init process ID */
	pid_t init_pid = 0;

	/* the child process exit status */
	int status = 0;

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (3 > argc) {
		PRINT(USAGE);
		goto end;
	}

	/* change the file system root */
	if (-1 == chroot(argv[1])) {
		goto end;
	}

	/* create a sandboxed child process which will act as an init process - for
	 * extra portability, point at the stack middle */
	init_pid = clone(_init,
	                 stack + (sizeof(stack) / 2),
	                 SIGCHLD | NAMESPACES,
	                 argv);
	if (-1 == init_pid) {
		goto end;
	}

	/* wait for the init process to terminate and pass its exit code */
	if (init_pid != waitpid(init_pid, &status, 0)) {
		goto end;
	}
	exit_code = WEXITSTATUS(status);

end:
	return exit_code;
}
