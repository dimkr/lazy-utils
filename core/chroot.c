#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* make sure the number of command-line arguments is valid */
	if (3 > argc)
		goto end;

	/* change the file system root */
	if (-1 == chroot(argv[1]))
		goto end;

	/* execute the passed command-line */
	(void) execvp(argv[2], &argv[2]);

end:
	return EXIT_FAILURE;
}
