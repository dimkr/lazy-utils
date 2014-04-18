#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* the created file permissions */
#define FILE_PERMISSIONS (S_IWUSR | S_IRUSR)

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the file descriptor */
	int fd;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* open or create the file */
	fd = open(argv[1], O_CREAT | O_RDWR, FILE_PERMISSIONS);
	if (-1 == fd)
		goto end;

	/* close the file */
	(void) close(fd);

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
