#include <stdlib.h>
#include <sys/klog.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a reading buffer */
	char *buffer;

	/* the kernel log size, in bytes */
	int log_size;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* open the kernel log */
	if (-1 == klogctl(1, NULL, 0))
		goto end;

	/* get the log buffer size */
	log_size = klogctl(10, NULL, 0);
	if (0 >= log_size)
		goto close_log;

	/* allocate memory for the entire log */
	buffer = malloc((size_t) log_size);
	if (NULL == buffer)
		goto close_log;

	/* read from the kernel log */
	log_size = klogctl(3, buffer, (size_t) log_size);
	if (0 >= log_size)
		goto free_buffer;

	/* write the buffer to standard output */
	if ((ssize_t) log_size != write(STDOUT_FILENO, buffer, (size_t) log_size))
		goto free_buffer;

	/* report success, always */
	exit_code = EXIT_SUCCESS;

free_buffer:
	/* free the allocated memory */
	free(buffer);

close_log:
	/* close the kernel log */
	(void) klogctl(0, NULL, 0);

end:
	return exit_code;
}
