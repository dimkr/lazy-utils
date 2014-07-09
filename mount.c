#include <stdbool.h>
#include <string.h>
#include <sys/mount.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/magic.h>
#include <assert.h>

#include "common.h"

/* the usage message */
#define USAGE "Usage: mount [-B|-R|-M] [-t TYPE] [-o OPTIONS] SOURCE TARGET\n"

typedef struct {
	const char *text;
	int flag;
} option_t;

typedef struct {
	char name[1 + STRLEN("squashfs")];
	uint64_t magic;
	size_t size;
	off_t offset;
} fs_magic_t;

static const option_t g_options[] = {
	{ "ro", MS_RDONLY },
	{ "noexec", MS_NOEXEC },
	{ "nodev", MS_NODEV },
	{ "nosuid", MS_NOSUID }
};

static const fs_magic_t g_file_systems[] = {
	{
		"ext4",
		EXT4_SUPER_MAGIC,
		sizeof(uint16_t),
		1024 + /* the first superblock is at offset 1024 */ \
		56 /* the offset of s_magic inside struct ext2_super_block */
	},
	{
		"xfs",
		0x58465342, /* XFS_SB_MAGIC, defined in fs/xfs/xfs_sb.h */
		sizeof(uint32_t),
		0 /* xfs_sb_t begins with XFS_SB_MAGIC */
	},
	{
		"squashfs",
		SQUASHFS_MAGIC,
		sizeof(uint32_t),
		0 /* struct squashfs_super_block begins with SQUASHFS_MAGIC */
	},
};

static bool _parse_options(const char *options, int *flags, char **data) {
	/* the options string size */
	size_t size = 0;

	/* a loop index */
	unsigned int i = 0;

	/* the return value */
	bool result = false;

	/* a flag which indiciates whether an option is recognized */
	bool is_found = false;

	/* a copy of the options string */
	char *copy = NULL;

	/* a single option */
	char *option = NULL;

	/* strtok_r()'s position within the options list */
	char *position = NULL;

	assert(NULL != options);
	assert(NULL != flags);
	assert(NULL != data);

	/* duplicate the options string */
	size = sizeof(char) * (1 + strlen(options));
	copy = malloc(size);
	if (NULL == copy) {
		goto end;
	}
	(void) memcpy(copy, options, size);

	/* allocate memory for unrecognized options */
	*data = malloc(size);
	if (NULL == *data) {
		goto free_options;
	}
	(*data)[0] = '\0';

	/* set the specified flags */
	option = strtok_r(copy, ",", &position);
	while (NULL != option) {
		is_found = false;
		for (i = 0; ARRAY_SIZE(g_options) > i; ++i) {
			if (0 == strcmp(g_options[i].text, option)) {
				*flags |= g_options[i].flag;
				is_found = true;
				break;
			}
		}
		if (false == is_found) {
			if (0 == strlen(*data)) {
				(void) strcpy(*data, option);
			} else {
				(void) sprintf(*data, "%s,%s", *data, option);
			}
		}
		option = strtok_r(NULL, ",", &position);
	}

	/* report success */
	result = true;

free_options:
	/* free the options string */
	free(copy);

end:
	return result;
}

static const char *_guess_type(const char *path) {
	/* a reading buffer */
	unsigned char buffer[sizeof(uint64_t)] = {0};

	/* the device file descriptor */
	int device = (-1);

	/* a loop index */
	unsigned int i = 0;

	/* the return value */
	const char *file_system = NULL;

	assert(NULL != path);

	/* open the device */
	device = open(path, O_RDONLY);
	if (-1 == device) {
		goto end;
	}

	for ( ; ARRAY_SIZE(g_file_systems) > i; ++i) {
		/* go to the magic offset */
		if (g_file_systems[i].offset != lseek(device,
		                                      g_file_systems[i].offset,
		                                      SEEK_SET)) {
			goto close_device;
		}

		/* read the magic */
		if (g_file_systems[i].size != read(device,
		                                   buffer,
		                                   g_file_systems[i].size)) {
			goto close_device;
		}

		/* if the magic matches, print the file system name and stop */
		if (0 == memcmp(&g_file_systems[i].magic,
		                buffer,
		                g_file_systems[i].size)) {
			file_system = g_file_systems[i].name;
			break;
		}
	}

close_device:
	/* close the device */
	(void) close(device);

end:
	return file_system;
}

static bool _mount_brute(const char *source,
                         const char *dest,
                         const int flags,
                         const void *data) {
	/* an entry in the list of supported file systems */
	char entry[STRLEN("nodev\tanon_inodefs")] = {'\0'};

	/* the return value */
	bool result = false;

	/* the list of supported file systems */
	FILE *list = NULL;

	assert(NULL != source);
	assert(NULL != dest);

	/* open the list of supported file systems */
	list = fopen("/proc/filesystems", "r");
	if (NULL == list) {
		goto end;
	}

	do {
		/* read an entry */
		if (NULL == fgets(entry, STRLEN(entry), list)) {
			break;
		}

		/* if the file system does not require a device (i.e like tmpfs), skip
		 * it */
		if ('\t' != entry[0]) {
			continue;
		}

		/* strip the trailing line break */
		entry[strlen(entry) - 1] = '\0';

		/* try to mount the file system */
		if (0 == mount(source, dest, &entry[1], flags, data)) {
			result = true;
			break;
		}
	} while (1);

	/* close the file systems list */
	(void) fclose(list);

end:
	return result;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* mount() flags */
	int flags = 0;

	/* a command-line option */
	int option = 0;

	/* the operation mode */
	char mode = 'm';

	/* file system specific options */
	char *data = NULL;

	/* file system type */
	const char *type = NULL;

	/* file system options */
	const char *options = NULL;

	/* parse the command-line */
	do {
		option = getopt(argc, argv, "BRMt:o:");
		if (-1 == option) {
			break;
		}

		switch (option) {
			case 'B':
				mode = option;
				flags = MS_BIND;
				break;

			case 'R':
				mode = option;
				flags = (MS_BIND | MS_REC);
				break;

			case 'M':
				mode = option;
				flags = MS_MOVE;
				break;

			case 't':
				type = optarg;
				break;

			case 'o':
				options = optarg;
				break;

			default:
				PRINT(USAGE);
				goto end;
		}
	} while (1);

	/* make sure there are two additional command-line arguments: the file
	 * system source and the mount point */
	if (argc != (2 + optind)) {
		PRINT(USAGE);
		goto end;
	}

	if ('m' == mode) {
		/* parse the options string */
		if (NULL != options) {
			if (false == _parse_options(options, &flags, &data)) {
				goto end;
			}
		}

		/* if no file system type was specified, try to guess */
		if (NULL == type) {
			type = _guess_type(argv[optind]);
		}

		/* if the file system was not recognized, use brute force */
		if (NULL == type) {
			if (true == _mount_brute(argv[optind],
			                         argv[1 + optind],
			                         flags,
			                         data)) {
				goto success;
			} else {
				goto free_data;
			}
		}
	}

	/* mount the file system */
	if (-1 == mount(argv[optind], argv[1 + optind], type, flags, data)) {
		goto free_data;
	}

success:
	/* report success */
	exit_code = EXIT_SUCCESS;

free_data:
	/* free file system specific options */
	if (NULL != data) {
		free(data);
	}

end:
	return exit_code;
}
