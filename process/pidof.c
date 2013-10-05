#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <liblazy/proc.h>

bool _find_matching_processes(const char *pid, char *name) {
	/* the return value */
	bool should_continue = true;

	/* the process information */
	process_info_t info;

	/* read the process information */
	if (false == get_process_info(pid, &info))
		goto end;

	/* if the process name doesn't match, do nothing */
	if (0 != strcmp(name, info.name))
		goto end;

	/* print the process ID */
	if (0 > printf("%s\n", pid)) {
		should_continue = false;
		goto end;
	}

end:
	return should_continue;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* find matching processes */
	if (false == for_each_process((process_callback_t) _find_matching_processes,
	                              argv[1]))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
