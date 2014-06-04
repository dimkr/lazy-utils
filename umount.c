#include <stdlib.h>
#include <sys/mount.h>

#include "common.h"

/* the usage message */
#	define USAGE "Usage: umount TARGET\n"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure one argument was passed */
	if (2 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* unmount the file system */
	if (-1 == umount2(argv[1], MNT_DETACH | UMOUNT_NOFOLLOW)) {
		goto end;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
