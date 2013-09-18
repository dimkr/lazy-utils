#include <stdlib.h>
#include <unistd.h>
#include <liblazy/io.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the file */
	file_t file;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* read the file */
	if (false == file_read(&file, argv[1]))
		goto end;

	/* print the file contents */
	if (file.size != write(STDOUT_FILENO, file.contents, file.size))
		goto close_file;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_file:
	/* close the file */
	file_close(&file);

end:
	return exit_code;
}
