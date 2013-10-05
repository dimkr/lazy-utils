#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a link creation function */
	int (*create_link)(const char *destination, const char *path);

	/* the link target */
	char *destination;

	/* the link path */
	char *path;

	/* parse the command-line */
	switch (argc) {
		case 3:
			destination = argv[2];
			path = argv[1];
			create_link = link;
			break;

		case 4:
			if (0 != strcmp("-s", argv[1]))
				goto end;
			destination = argv[3];
			path = argv[2];
			create_link = symlink;
			break;

		default:
			goto end;
	}

	/* create the link */
	if (-1 == create_link(path, destination))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
