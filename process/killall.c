#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <liblazy/proc.h>

typedef struct {
	char *name;
	int sent_signal;
} parameters_t;

bool _kill_matching_processes(const char *pid, parameters_t *parameters) {
	/* the return value */
	bool should_continue = true;

	/* the process information */
	process_info_t info;

	/* read the process information */
	if (false == get_process_info(pid, &info))
		goto end;

	/* if the process name doesn't match, do nothing */
	if (0 != strcmp(parameters->name, info.name))
		goto end;

	/* send the signal */
	if (-1 == kill((pid_t) atoi(pid), parameters->sent_signal))
		goto end;

end:
	return should_continue;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the callback parameters */
	parameters_t parameters;

	/* parse the command-line */
	switch (argc) {
		case 3:
			/* decide which signal to send */
			parameters.sent_signal = signal_option_to_int(argv[1]);
			if (-1 == parameters.sent_signal)
				goto end;

			parameters.name = argv[2];
			break;

		case 2:
			parameters.sent_signal = DEFAULT_TERMINATION_SIGNAL;
			parameters.name = argv[1];
			break;

		default:
			goto end;
	}

	/* kill matching processes */
	if (false == for_each_process((process_callback_t) _kill_matching_processes,
	                              &parameters))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
