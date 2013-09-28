#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <liblazy/proc.h>

typedef struct {
	char *name;
	const char *signal;
} parameters_t;

bool _kill_matching_processes(const char *pid, parameters_t *parameters) {
	/* the return value */
	bool should_continue = true;

	/* the process information */
	process_info_t info;

	/* a process ID */
	pid_t child_process;

	/* read the process information */
	if (false == get_process_info(pid, &info))
		goto end;

	/* if the process name doesn't match, do nothing */
	if (0 != strcmp(parameters->name, info.name))
		goto end;

	/* create a child process */
	child_process = fork();
	switch (child_process) {
		case (-1):
			should_continue = false;
			break;

		case 0:
			/* run kill for each process */
			(void) execlp("kill",
			              "kill",
			              parameters->signal,
			              pid,
			              (char *) NULL);
			break;

		default:
			/* wait for the child process to terminate */
			if (child_process != waitpid(child_process, NULL, 0))
				should_continue = false;
			break;
	}

end:
	return should_continue;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the callback parameters */
	parameters_t parameters;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* kill matching processes */
	parameters.signal = argv[1];
	parameters.name = argv[2];
	if (false == for_each_process((process_callback_t) _kill_matching_processes,
	                              &parameters))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
