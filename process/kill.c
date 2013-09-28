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

	/* the process ID */
	pid_t pid;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* convert the process ID to numeric form; do not allow sending a signal to
	 * -1 (i.e all processes except init) or 0 (an invalid PID), but do not
	 * check whether the PID exceeds the limit specified in
	 * /proc/sys/kernel/pid_max, since kill() will fail and it's good enough */
	pid = (pid_t) atoi(argv[2]);
	if (0 >= pid)
		goto end;

	/* make sure the first argument begins with a dash */
	if ('-' != argv[1][0])
		goto end;

	/* make sure the first argument isn't a dash */
	if (1 == strlen(argv[1]))
		goto end;

	/* decide which signal to send */
	sent_signal = signal_name_to_int((char *) &argv[1][1]);
	if (-1 == sent_signal)
		goto end;

	/* send the signal */
	if (-1 == kill(pid, sent_signal))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
