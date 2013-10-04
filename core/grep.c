#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <liblazy/io.h>

/* the maximum length of an input line */
#define MAX_INPUT_LINE_SIZE (FILE_READING_BUFFER_SIZE)

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a regular expression */
	regex_t expression;

	/* an input line */
	char line[MAX_INPUT_LINE_SIZE];

	/* the input line length */
	size_t length;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* compile the regular expression */
	if (0 != regcomp(&expression, argv[1], REG_EXTENDED))
		goto end;

	do {
		/* read an input line */
		if (NULL == fgets((char *) &line, sizeof(line), stdin))
			break;

		/* if the line ends with a line break, terminate it */
		length = strlen((char *) &line);
		if ('\n' == line[length - 1])
			line[length - 1] = '\0';

		/* if the line matches the given expression, print it */
		if (0 == regexec(&expression, (char *) &line, 0, NULL, 0)) {
			if (0 > printf("%s\n", (char *) &line))
				goto free_expression;
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

free_expression:
	/* free the compiled regular expression */
	regfree(&expression);

end:
	return exit_code;
}
