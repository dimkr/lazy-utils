#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <linux/loop.h>
#include <errno.h>
#include <liblazy/io.h>
#include <liblazy/fs.h>

/* the number of available loop devices; should be equal to
 * CONFIG_BLK_DEV_LOOP_MIN_COUNT */
#define LOOP_DEVICES_COUNT (8)

/* the mounted file systems list */
#define MOUNTED_FILE_SYSTEMS_LIST_PATH "/proc/mounts"

bool _associate_with_loop_device(const char *device_path,
                                 int file,
                                 const char *file_path,
                                 int mode) {
	/* the return value */
	bool is_success = false;

	/* the loopback device file descriptor */
	int device;

	/* the loopback device information */
	struct loop_info info;

	/* open the loopback device */
	device = open(device_path, mode);
	if (-1 == device)
		goto end;

	/* get the loopback device status */
	if (-1 != ioctl(device, LOOP_GET_STATUS, &info))
		goto close_device;

	/* if the device is already used, skip it; the return value of
	 * LOOP_GET_STATUS is undocumented; see loop_get_status() in
	 * drivers/block/loop.c, in the kernel sources */
	if (ENXIO != errno)
		goto close_device;

	/* associate the loopback device with the file */
	(void) memset(&info, 0, sizeof(info));
	(void) strcpy((char *) &info.lo_name, file_path);
	if (0 != ioctl(device, LOOP_SET_FD, file))
		goto close_device;

	/* initialize the device status */
	if (0 != ioctl(device, LOOP_SET_STATUS, &info))
		goto unassociate_device;

	/* report success */
	is_success = true;
	goto close_device;

unassociate_device:
	/* unassociate the loopback device */
	(void) ioctl(device, LOOP_CLR_FD, 0);

close_device:
	/* close the loopback device */
	(void) close(device);

end:
	return is_success;
}

bool _mount_file(const char *file_path, char *loop_device, int mode) {
	/* the return value */
	bool is_success = false;

	/* the file descriptor */
	int fd;

	/* the loop device number */
	unsigned int i;

	/* open the file */
	fd = open(file_path, mode);
	if (-1 == fd)
		goto end;

	for (i = 0; LOOP_DEVICES_COUNT > i; ++i) {
		/* format the loop device path */
		(void) sprintf(loop_device, "/dev/loop%u", i);

		/* try to associate the device with the file */
		if (false == _associate_with_loop_device(loop_device,
		                                         fd,
		                                         file_path,
		                                         mode))
			continue;

		/* report success */
		is_success = true;
		break;
	}

	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

bool _parse_options(const char *type,
                    char *options,
                    int *flags,
                    bool *is_loop,
                    void **data) {
	/* the return value */
	bool is_valid = false;

	/* strtok_r()'s position inside the options string */
	char *position;

	/* a token in the options string */
	char *token;

	/* the options string length */
	size_t length;

	/* if both the bind mount and mount point moving flags were passed, report
	 * failure */
	if ((MS_BIND == (*flags & MS_BIND)) && (MS_MOVE == (*flags & MS_MOVE)))
		goto end;

	/* make sure no file system type was specified for bind mounts or mount
	 * point moving, since this makes no sense */
	if ((MS_BIND == (*flags & MS_BIND)) || (MS_MOVE == (*flags & MS_MOVE))) {
		if (NULL != type)
			goto end;
	}

	/* if no options string was specified, report success */
	if (NULL == options)
		goto valid;

	/* if the options string is blank, report failure */
	length = strlen(options);
	if (0 == length)
		goto end;

	/* start parsing the options string */
	token = strtok_r(options, ",", &position);
	if (NULL == token)
		goto end;

	/* split all tokens and set the matching bits */
	do {
		if (0 == strcmp("loop", token))
			*is_loop = true;
		else {
			if (0 == strcmp("ro", token))
				*flags |= MS_RDONLY;
			else {
				if (0 == strcmp("noexec", token))
					*flags |= MS_NOEXEC;
				else {
					if (0 == strcmp("nodev", token))
						*flags |= MS_NODEV;
					else {
						if (0 == strcmp("nosuid", token))
							*flags |= MS_NOSUID;
						else {
							/* if the option is unrecognized, append it to the
							 * string passed to the driver */
							if (NULL != *data) {
								(void) sprintf((char *) *data,
								               "%s,%s",
								               (char *) *data,
								               token);
							} else {
								/* allocate memory for an additional options
								 * string */
								*data = malloc(sizeof(char) * length);
								if (NULL == *data)
									goto end;

								/* copy the token to the allocated string */
								(void) strcpy((char *) *data, token);
							}
						}
					}
				}
			}
		}
		token = strtok_r(NULL, ",", &position);
	} while (NULL != token);

valid:
	/* report success */
	is_valid = true;

end:
	return is_valid;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the file system source */
	char *source;

	/* a flag which indicates whether the file system source is a regular
	 * file */
	bool is_loop = false;

	/* loop device path */
	char loop_device[PATH_MAX];

	/* the loop device mounting mode */
	int loop_mode;

	/* mount options */
	char *options = NULL;

	/* a pointer passed to the mount() system call */
	void *data = NULL;

	/* the file system type */
	const char *type = NULL;

	/* the mount point flags */
	int flags = 0;

	/* a command-line option */
	int option;

	/* if no command-line arguments were specified, list mounted file systems */
	if (1 == argc) {
		if (true == file_print(MOUNTED_FILE_SYSTEMS_LIST_PATH))
			exit_code = EXIT_SUCCESS;
		goto end;
	}

	/* parse the command-line */
	do {
		option = getopt(argc, argv, "BRMt:o:");
		if (-1 == option)
			break;

		switch (option) {
			case 'B':
				flags |= MS_BIND;
				break;

			case 'R':
				flags |= (MS_BIND | MS_REC);
				break;

			case 'M':
				flags |= MS_MOVE;
				break;

			case 't':
				type = optarg;
				break;

			case 'o':
				options = optarg;
				break;

			default:
				goto end;
		}
	} while (1);

	/* make sure there are two additional command-line arguments: the file
	 * system source and the mount point */
	if (argc != (2 + optind))
		goto end;

	/* parse the options string */
	if (false == _parse_options(type, options, &flags, &is_loop, &data))
		goto end;

	/* if the file system source is a regular file, try to pair it with a loop
	 * device; then, use the loop device as the file system source */
	if (false == is_loop)
		source = argv[optind];
	else {
		/* translate the mounting mode to a file opening mode constant */
		if (MS_RDONLY == (flags & MS_RDONLY))
			loop_mode = O_RDONLY;
		else
			loop_mode = O_RDWR;

		if (false == _mount_file(argv[optind],
		                         (char *) &loop_device,
		                         loop_mode))
			goto free_data;
		source = (char *) &loop_device;
	}

	/* mount the specified file system */
	if (false == fs_mount_brute(source, argv[1 + optind], flags, data)) {
		if (false == fs_mount(source, argv[1 + optind], type, flags, data))
			goto free_data;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

free_data:
	/* free the data passed to the driver */
	if (NULL != data)
		free(data);

end:
	return exit_code;
}
