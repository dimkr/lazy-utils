#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the file attributes */
	struct stat attributes;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* get the file size */
	if (-1 == stat(argv[1], &attributes))
		goto end;

	/* if the file is not a regular file, report failure */
	if (!(S_ISREG(attributes.st_mode)))
		goto end;

	/* print the file size */
	if (0 > printf("%zd\t%s\n", attributes.st_size, argv[1]))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
