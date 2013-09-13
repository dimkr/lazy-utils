#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <liblazy/io.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the kernel module */
	char *kernel_module;

	/* the kernel module size */
	size_t kernel_module_size;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* read the kernel module */
	kernel_module = file_read(argv[1], &kernel_module_size);
	if (NULL == kernel_module)
		goto end;

	/* call the init_module() system call, to load the module */
	if (-1 == syscall(SYS_init_module, kernel_module, kernel_module_size, ""))
		goto free_kernel_module;

	/* report success */
	exit_code = EXIT_SUCCESS;

free_kernel_module:
	/* free the buffer containing the kernel module */
	free(kernel_module);

end:
	return exit_code;
}
