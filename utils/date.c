#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the current time */
	time_t now;
	char now_textual[26];

	/* the output size */
	size_t size;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* get the current date and time */
	now = time(NULL);
	if (NULL == ctime_r(&now, (char *) &now_textual))
		goto end;

	/* print it */
	size = sizeof(char) * strlen((const char *) &now_textual);
	if ((ssize_t) size != write(STDOUT_FILENO, (void *) &now_textual, size))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
