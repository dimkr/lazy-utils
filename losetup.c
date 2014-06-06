#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/loop.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"

/* the usage message */
#define USAGE "Usage: losetup [-d] DEVICE [FILE]\n"

static int _associate(const char *device_path,
                      const char *file_path,
                      const int flags) {
	/* the loopback device information */
	struct loop_info info = {0};

	/* the loopback device descriptor */
	int device = (-1);

	/* the file descriptor */
	int fd = (-1);

	/* the return value */
	int result = EXIT_FAILURE;

	assert(NULL != device_path);
	assert(NULL != file_path);

	/* open the file; if the file resides on a read-only file system, open it in
	 * read-only mode */
	fd = open(file_path, flags);
	if (-1 == fd) {
		goto end;
	}

	/* open the loopback device */
	device = open(device_path, flags);
	if (-1 == device) {
		goto close_file;
	}

	/* get the loopback device status */
	if (-1 != ioctl(device, LOOP_GET_STATUS, &info)) {
		goto close_device;
	}

	/* if the device is already used, skip it; the return value of
	 * LOOP_GET_STATUS is undocumented; see loop_get_status() in
	 * drivers/block/loop.c, in the kernel sources */
	if (ENXIO != errno) {
		goto close_device;
	}

	/* associate the loopback device with the file */
	(void) strcpy(info.lo_name, file_path);
	if (0 != ioctl(device, LOOP_SET_FD, fd)) {
		goto close_device;
	}

	/* initialize the device status */
	if (0 != ioctl(device, LOOP_SET_STATUS, &info)) {
		(void) ioctl(device, LOOP_CLR_FD, 0);
		goto close_device;
	}

	/* report success */
	result = EXIT_SUCCESS;

close_device:
	/* close the loopback device */
	(void) close(device);

close_file:
	/* close the file */
	(void) close(fd);

end:
	return result;
}

static int _unassociate(const char *device_path) {
	/* the loopback device descriptor */
	int device = (-1);

	/* the return value */
	int result = EXIT_FAILURE;

	assert(NULL != device_path);

	/* open the loopback device */
	device = open(device_path, O_RDONLY);
	if (-1 == device) {
		goto end;
	}

	/* unassociate the loopback device */
	if (0 != ioctl(device, LOOP_CLR_FD, 0)) {
		goto close_device;
	}

	/* report success */
	result = EXIT_SUCCESS;

close_device:
	/* close the loopback device */
	(void) close(device);

end:
	return result;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* open() flags */
	int flags = O_RDWR;

	/* a command-line option */
	int option = 0;

	/* the operation mode */
	char mode = 'a';

	/* the required number or arguments */
	int required_argc = 2;

	/* parse the command-line */
	do {
		option = getopt(argc, argv, "dr");
		if (-1 == option) {
			break;
		}

		switch (option) {
			case 'd':
				mode = option;
				required_argc = 1;
				break;

			case 'r':
				flags = O_RDONLY;
				break;

			default:
				PRINT(USAGE);
				goto end;
		}
	} while (1);

	/* make sure the right number of arguments was passed */
	if ((argc - optind) != required_argc) {
		PRINT(USAGE);
		goto end;
	}

	if ('d' == mode) {
		exit_code = _unassociate(argv[optind]);
	} else {
		exit_code = _associate(argv[optind], argv[1 + optind], flags);
	}

end:
	return exit_code;
}
