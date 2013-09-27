#include <stdlib.h>
#include <stdio.h>
#include <fnmatch.h>
#include <limits.h>
#include <liblazy/io.h>
#include <liblazy/kmodule.h>

/* the kernel modules list */
FILE *g_module_list;

/* the kernel modules aliases list */
FILE *g_alias_list;

/* the kernel modules dependencies list */
FILE *g_dependencies_list;

bool _add_module_to_lists(const char *path,
                          const char *pattern,
                          kmodule_loader_t *loader,
                          struct dirent *entry) {
	/* a flag which indicates whether the module listing failed */
	bool should_stop = true;

	/* the kernel module path */
	char full_path[PATH_MAX];

	/* the kernel module */
	kmodule_t module;

	/* the kernel module information */
	kmodule_info_t info;

	/* a loop index */
	unsigned int i;

	/* if the file is not a kernel module, skip it */
	if (0 != fnmatch(pattern, (char *) &entry->d_name, 0))
		return false;

	/* obtain the full kernel module path */
	(void) sprintf((char *) &full_path,
	               "%s/%s",
	               path,
	               (char *) &entry->d_name);

	/* open the kernel module */
	if (false == kmodule_open(loader, &module, NULL, (char *) &full_path))
		goto end;

	/* obtain the kernel module information */
	if (false == kmodule_info_get(&module, &info))
		goto close_module;

	/* add the module to the modules list */
	if (0 > fprintf(g_module_list,
	                "%s%c%s\n",
	                (char *) &entry->d_name,
	                KMODULE_LIST_DELIMETER,
	                (char *) &full_path))
		goto free_info;

	/* add the module to the module aliases list */
	for (i = 0; info.mod_alias.count > i; ++i) {
		if (0 > fprintf(g_alias_list,
		                "%s%c%s\n",
		                info.mod_alias.values[i],
		                KMODULE_LIST_DELIMETER,
		                (char *) &full_path))
			goto free_info;
	}

	/* add the module to the module dependencies list */
	if (0 > fprintf(g_dependencies_list,
	                "%s%c%s\n",
					(char *) &full_path,
					KMODULE_LIST_DELIMETER,
					info.mod_depends.values[0]))
		goto free_info;

	/* everything went fine - continue to the next module */
	should_stop = false;

free_info:
	/* free the kernel module information */
	kmodule_info_free(&info);

close_module:
	/* close the kernel module */
	kmodule_close(&module);

end:
	return should_stop;
}

bool _generate_modules_list() {
	/* the return value */
	bool is_success = false;

	/* a kernel module loader */
	kmodule_loader_t loader;

	/* initialize the kernel module loader */
	if (false == kmodule_loader_init(&loader, true))
		goto end;

	/* open the module list */
	g_module_list = fopen(KMODULE_LIST_PATH, "w");
	if (NULL == g_module_list)
		goto destroy_loader;

	/* open the alias list */
	g_alias_list = fopen(KMODULE_ALIAS_LIST_PATH, "w");
	if (NULL == g_alias_list)
		goto close_module_list;

	/* open the dependencies list */
	g_dependencies_list = fopen(KMODULE_DEPENDENCIES_LIST_PATH, "w");
	if (NULL == g_dependencies_list)
		goto close_alias_list;

	/* list all kernel modules */
	if (true == file_for_each(KMODULE_DIRECTORY,
	                          "*."KMODULE_FILE_NAME_EXTENSION,
	                          &loader,
	                          (file_callback_t) _add_module_to_lists))
		goto close_dependencies_list;

	/* report success */
	is_success = true;

close_dependencies_list:
	/* close the dependencies list */
	(void) fclose(g_dependencies_list);

close_alias_list:
	/* close the aliases list */
	(void) fclose(g_alias_list);

close_module_list:
	/* close the module list */
	(void) fclose(g_module_list);

destroy_loader:
	/* destroy the kernel module loader */
	kmodule_loader_destroy(&loader);

end:
	return is_success;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* generate a list of all modules */
	if (false == _generate_modules_list())
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
