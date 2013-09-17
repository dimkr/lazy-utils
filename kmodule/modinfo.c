#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the kernel module */
	kmodule_t module;

	/* the kernel module information */
	kmodule_info_t info;

	/* a loop index */
	unsigned int i;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* read the kernel module, by name or path */
	if (false == kmodule_open(&module, argv[1], NULL)) {
		if (false == kmodule_open(&module, NULL, argv[1]))
			goto end;
	}

	/* obtain the kernel module information */
	kmodule_get_info(&module, &info);

	/* print the kernel module information */
	(void) printf("filename: %s\n", module.path);
	for (i = 0; ARRAY_SIZE(info._fields) > i; ++i) {
		if (NULL != info._fields[i])
			(void) printf("%s: %s\n", g_kmodule_fields[i], info._fields[i]);
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

	/* close the kernel module */
	kmodule_close(&module);

end:
	return exit_code;
}
