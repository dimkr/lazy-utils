#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* flush all file system buffers */
	sync();

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
