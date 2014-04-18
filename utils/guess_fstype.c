#include <stdlib.h>
#include <stdio.h>
#include <liblazy/fs.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the block device file system */
	const char *file_system;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* open the device */
	file_system = fs_guess(argv[1]);
	if (NULL == file_system)
		goto end;

	/* print the file system name */
	if (0 > printf("%s\n", file_system))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
