#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

/* created directory permissions */
#define DIRECTORY_PERMISSIONS (0777)

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* create the specified directory */
	if (-1 != mkdir(argv[1], DIRECTORY_PERMISSIONS))
		exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
