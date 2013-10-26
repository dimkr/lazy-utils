#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a kernel module loader */
	kmodule_loader_t loader;

	/* the kernel module */
	kmodule_t module;

	/* loop indices */
	unsigned int i;
	unsigned int j;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* initialize the kernel module loader */
	if (false == kmodule_loader_init(&loader))
		goto end;

	/* find the kernel module, by name */
	if (false == kmodule_open_module_by_name(&loader, &module, argv[1]))
		goto destroy_loader;

	/* obtain the kernel module information */
	if (false == kmodule_get_info(&module))
		goto close_module;

	/* print the kernel module information */
	(void) printf("filename: %s\n", (char *) &module.path);
	for (i = 0; ARRAY_SIZE(module._fields) > i; ++i) {
		for (j = 0; module._fields[i].count > j; ++j) {
			(void) printf("%s: %s\n",
			              g_kmodule_fields[i],
			              module._fields[i].values[j]);
		}
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

close_module:
	/* close the kernel module */
	kmodule_close(&module);

destroy_loader:
	/* destroy the kernel module loader */
	kmodule_loader_destroy(&loader);

end:
	return exit_code;
}
