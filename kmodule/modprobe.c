#include <stdlib.h>
#include <string.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a kernel module loader */
	kmodule_loader_t loader;

	/* the loaded module */
	char *module;

	/* a flag which indicates whether the module loading should be logged */
	bool is_quiet = false;

	/* parse the command-line */
	switch (argc) {
		case (2):
			module = argv[1];
			break;

		case (3):
			if (0 != strcmp(argv[1], "-q"))
				goto end;
			is_quiet = true;
			module = argv[2];
			break;

		default:
			goto end;
	}

	/* initialize the kernel module loader */
	if (false == kmodule_loader_init(&loader, is_quiet))
		goto end;

	/* load the kernel module and its dependencies, in reverse order */
	if (false == kmodule_load(&loader, module, NULL, true)) {

		/* it the module was not found, the specified name could be an alias -
		 * the kernel passes aliases (i.e for file system names) in
		 * call_modprobe() */
		if (false == kmodule_load_by_alias(&loader, module))
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
