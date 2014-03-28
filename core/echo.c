#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a loop index */
	int i;

	/* string size */
	size_t size;

	/* a flag which indicates whether a line break should be added */
	bool line_break;

	/* if no arguments were given, print a line break */
	if (1 == argc)
		goto print_line_break;

	/* otherwise, check whether the first argument is "-n" */
	if (0 == strcmp("-n", argv[1])) {
		line_break = false;
		if (2 == argc)
			goto end;
		i = 2;
	} else {
		line_break = true;
		i = 1;
	}

	/* print all arguments after "-n" */
	do {
		/* if the string is empty, ignore it */
		size = sizeof(char) * strlen(argv[i]);
		if (0 < size) {
			if ((ssize_t) size != write(STDOUT_FILENO, argv[i], size))
				goto end;
		}

		++i;
		if (argc == i)
			break;

		/* add a space after each argument */
		if (sizeof(char) != write(STDOUT_FILENO, " ", sizeof(char)))
			goto end;
	} while (1);

	/* add a terminating line break */
	if (true == line_break) {
print_line_break:
		if (sizeof(char) != write(STDOUT_FILENO, "\n", sizeof(char)))
			goto end;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
