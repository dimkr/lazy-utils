#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <liblazy/common.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the list of loaded modules */
	FILE *loaded_modules;

	/* a kernel module's name */
	char *name;

	/* a kernel module's size */
	char *size;

	/* kernel modules that depend on a given module */
	char *dependent_modules;

	/* the number of dependent kernel modules */
	char *dependent_modules_count;

	/* a line in the loaded modules list */
	char line[1 + KMODULE_MAX_LOADED_MODULE_ENTRY_LENGTH];

	/* strtok_r()'s position within the line */
	char *position;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* open the list of loaded modules */
	loaded_modules = fopen(KMODULE_LOADED_MODULES_LIST_PATH, "r");
	if (NULL == loaded_modules)
		goto end;

	/* print the column headers */
	(void) printf("Module                    Size    Used by\n");

	do {
		/* read an entry */
		if (NULL == fgets((char *) &line, STRLEN(line), loaded_modules))
			break;

		/* parse the entry */
		name = strtok_r((char *) &line, " ", &position);
		if (NULL == name)
			goto close_list;

		size = strtok_r(NULL, " ", &position);
		if (NULL == size)
			goto close_list;

		dependent_modules_count = strtok_r(NULL, " ", &position);
		if (NULL == dependent_modules_count)
			goto close_list;

		dependent_modules = strtok_r(NULL, " ", &position);
		if (NULL == dependent_modules)
			goto close_list;

		/* print the entry */
		if ('-' == dependent_modules[0])
			dependent_modules[0] = '\0';
		else
			dependent_modules[strlen(dependent_modules) - 1] = '\0';
		(void) printf("%-24s %7s %2s %s\n",
		              name,
		              size,
		              dependent_modules_count,
		              dependent_modules);
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_list:
	/* close the list of loaded modules */
	(void) fclose(loaded_modules);

end:
	return exit_code;
}
