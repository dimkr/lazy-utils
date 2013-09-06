#include <stdlib.h>
#include <sys/mount.h>

/*
mount -t tmpfs run /run
       1  2    3   4
*/

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (5 != argc)
		goto end;

	/* mount the specified file system */
	if (-1 != mount(argv[3], argv[4], argv[2], 0, NULL))
		exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
