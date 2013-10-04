#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <liblazy/io.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the file */
	int file;

	/* a buffer */
	char buffer[FILE_READING_BUFFER_SIZE];

	/* read chunk size */
	ssize_t chunk_size;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* open the file */
	file = open(argv[1], O_RDONLY);
	if (-1 == file)
		goto end;

	do {
		/* read a chunk from the file */
		chunk_size = read(file, (char *) &buffer, sizeof(buffer));
		if (-1 == chunk_size)
			goto close_file;

		/* print the read chunk */
		if (chunk_size != write(STDOUT_FILENO,
		                        (char *) &buffer,
		                        (size_t) chunk_size))
			goto close_file;

		/* if the chunk is smaller than the buffer size, the file end was
		 * reached */
		if (sizeof(buffer) > chunk_size)
			break;
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_file:
	/* close the file */
	(void) close(file);

end:
	return exit_code;
}
