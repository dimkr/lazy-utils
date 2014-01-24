#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <liblazy/common.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the link target */
	char path[PATH_MAX];

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* obtain the link target */
	if (-1 == readlink(argv[1], (char *) &path, STRLEN(path)))
		goto end;

	/* print the link target */
	if (0 > printf("%s\n", (char *) &path))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
