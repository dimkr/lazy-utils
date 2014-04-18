#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <liblazy/io.h>

/* the output file permissions */
#define FILE_PERMISSIONS (0600)

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the reading buffer */
	unsigned char buffer[FILE_READING_BUFFER_SIZE];

	/* the read data size */
	ssize_t size;

	/* the output file */
	int fd;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* open the output file */
	fd = open(argv[1], O_WRONLY | O_CREAT | O_APPEND, FILE_PERMISSIONS);
	if (-1 == fd)
		goto end;

	do {
		/* read from the standard input pipe */
		size = read(STDIN_FILENO, (void *) &buffer, sizeof(buffer));
		switch (size) {
			case (-1):
				goto close_file;

			case 0:
				goto success;
		}

		/* write the read data to both the output file and standard output */
		if (size != write(fd, (void *) &buffer, (size_t) size))
			goto close_file;
		if (size != write(STDOUT_FILENO, (void *) &buffer, (size_t) size))
			goto close_file;
	} while (1);

success:
	/* report success */
	exit_code = EXIT_SUCCESS;

close_file:
	/* close the output file */
	(void) close(fd);

end:
	return exit_code;
}
