#include <stdlib.h>
#include <string.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a kernel module loader */
	kmodule_loader_t loader;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* initialize the kernel module loader */
	if (false == kmodule_loader_init(&loader))
		goto end;

	/* load the kernel module and its dependencies, in reverse order */
	if (false == kmodule_load_by_name(&loader, argv[1], "", true)) {
		if (false == kmodule_load_by_alias(&loader, argv[1], "", true))
			goto destroy_loader;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

destroy_loader:
	/* destroy the kernel module loader */
	kmodule_loader_destroy(&loader);

end:
	return exit_code;
}
