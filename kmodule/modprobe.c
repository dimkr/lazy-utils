#include <stdlib.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* load the kernel module and its dependencies, in reverse order */
	if (false == kmodule_load(argv[1], NULL, true))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
