#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <liblazy/kmodule.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the kernel module */
	kmodule_t kernel_module;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* read the kernel module */
	if (false == kmodule_open(&kernel_module, NULL, argv[1]))
		goto end;

	/* call the init_module() system call, to load the module */
	if (-1 == syscall(SYS_init_module,
	                  kernel_module.contents,
	                  kernel_module.size,
	                  ""))
		goto close_module;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_module:
	/* close the kernel module */
	kmodule_close(&kernel_module);

end:
	return exit_code;
}
