#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <liblazy/proc.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the sent signal */
	int sent_signal;

	/* the process ID, in textual form */
	char *pid_textual;

	/* the process ID */
	pid_t pid;

	/* parse the command-line */
	switch (argc) {
		case 3:
			/* decide which signal to send */
			sent_signal = signal_option_to_int(argv[1]);
			if (-1 == sent_signal)
				goto end;

			pid_textual = argv[2];
			break;

		case 2:
			sent_signal = DEFAULT_TERMINATION_SIGNAL;
			pid_textual = argv[1];
			break;

		default:
			goto end;
	}

	/* convert the process ID to numeric form; do not allow sending a signal to
	 * -1 (i.e all processes except init) or 0 (an invalid PID), but do not
	 * check whether the PID exceeds the limit specified in
	 * /proc/sys/kernel/pid_max, since kill() will fail and it's good enough */
	pid = (pid_t) atoi(pid_textual);
	if (0 >= pid)
		goto end;

	/* send the signal */
	if (-1 == kill(pid, sent_signal))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
