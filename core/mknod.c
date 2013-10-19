#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* the device node permissions */
#define DEVICE_NODE_PERMISSIONS (0600)

bool _is_numeric(const char  *string) {
	size_t i;

	for (i = 0; strlen(string) > i; ++i) {
		if (0 == isdigit(string[i]))
			return false;
	}

	return true;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the device node creation mode */
	mode_t mode;

	/* make sure the number of command-line arguments is valid */
	if (5 != argc)
		goto end;

	/* make sure the device is either a character device or a block device */
	if (0 == strcmp("c", argv[2]))
		mode = S_IFCHR;
	else {
		if (0 == strcmp("b", argv[2]))
			mode = S_IFBLK;
		else
			goto end;
	}

	/* make sure the device major and minor numbers are numeric */
	if (false == _is_numeric(argv[3]))
		goto end;
	if (false == _is_numeric(argv[4]))
		goto end;

	/* create the device node */
	if (-1 == mknod(argv[1],
	                DEVICE_NODE_PERMISSIONS | mode,
	                makedev(atoi(argv[3]), atoi(argv[4]))))
		goto end;

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
