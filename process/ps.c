#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <liblazy/common.h>
#include <liblazy/proc.h>

/* the format of a printed entry */
#define OUTPUT_FORMAT "%-5s %-5s %-5s %s\n"

bool _print_process_details(const char *pid, void *unused) {
	/* the return value */
	bool should_continue = true;

	/* the process command-line */
	char command_line[MAX_COMMAND_LINE_SIZE];

	/* the process command-line size, in bytes */
	ssize_t command_line_size;

	/* the process information */
	process_info_t info;

	/* read the process command-line */
	command_line_size = STRLEN(command_line);
	if (false == get_process_command_line(pid,
	                                      (char *) &command_line,
	                                      &command_line_size))
		goto end;

	/* read the process information */
	if (false == get_process_info(pid, &info))
		goto end;

	/* print the process details */
	if (0 > printf(OUTPUT_FORMAT,
	               pid,
	               info.ppid,
	               info.state,
	               (char *) &command_line))
		should_continue = false;

end:
	return should_continue;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* print the column headers */
	if (0 > printf(OUTPUT_FORMAT, "PID", "PPID", "STATE", "NAME"))
		goto end;

	/* print the process list */
	if (false == for_each_process(_print_process_details, NULL))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
