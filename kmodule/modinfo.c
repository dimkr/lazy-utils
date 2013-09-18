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

	/* loop indices */
	unsigned int i;
	unsigned int j;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* read the kernel module, by name or path */
	if (false == kmodule_open(&module, argv[1], NULL)) {
		if (false == kmodule_open(&module, NULL, argv[1]))
			goto end;
	}

	/* obtain the kernel module information */
	if (false == kmodule_info_get(&module, &info))
		goto close_module;

	/* print the kernel module information */
	(void) printf("filename: %s\n", module.path);
	for (i = 0; ARRAY_SIZE(info._fields) > i; ++i) {
		for (j = 0; info._fields[i].count > j; ++j) {
			(void) printf("%s: %s\n",
			              g_kmodule_fields[i],
			              info._fields[i].values[j]);
		}
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

	/* free the module information */
	kmodule_info_free(&info);

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return exit_code;
}
