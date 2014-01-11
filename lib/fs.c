#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <liblazy/fs.h>

const char *fs_guess(const char *path) {
	/* the return value */
	const char *file_system = NULL;

	/* the device */
	int device;

	/* a loop index */
	unsigned int i;

	/* a reading buffer */
	unsigned char buffer[sizeof(uint64_t)];

	/* open the device */
	device = open(path, O_RDONLY);
	if (-1 == device)
		goto end;

	for (i = 0; ARRAY_SIZE(g_file_systems) > i; ++i) {
		/* go to the magic offset */
		if (g_file_systems[i].offset != lseek(device,
		                                      g_file_systems[i].offset,
		                                      SEEK_SET))
			goto close_device;

		/* read the magic */
		if (g_file_systems[i].size != read(device,
		                                   (void *) &buffer,
		                                   g_file_systems[i].size))
			goto close_device;

		/* if the magic matches, print the file system name and stop */
		if (0 == memcmp(&g_file_systems[i].magic,
		                (void *) &buffer,
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

bool fs_mount(const char *source,
              const char *target,
              const char *type,
              const int flags,
              void *data) {
	/* the return value */
	bool is_success = false;

	/* if no file system type was specified, try to detect it */
	if (NULL == type) {
		type = fs_guess(source);
		if (NULL == type)
			goto end;
	}

	/* mount the file system */
	if (-1 == mount(source, target, type, flags, data))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool fs_mount_brute(const char *source,
                    const char *target,
                    const int flags,
                    void *data) {
	/* the return value */
	bool is_success = false;

	/* a file system entry */
	char type[1 + MAX_SUPPORTED_FILE_SYSTEM_ENTRY_LENGTH];

	/* the list of supported file systems */
	FILE *file_systems;

	/* open the list of supported file systems */
	file_systems = fopen(SUPPORTED_FILE_SYSTEMS_LIST, "r");
	if (NULL == file_systems)
		goto end;

	do {
		/* read one supported file system */
		if (NULL == fgets((char *) &type, STRLEN(type), file_systems))
			break;

		/* if the file system does not require a device, skip it */
		if ('\t' != type[0])
			continue;

		/* strip the trailing line break */
		type[strlen(type) - 1] = '\0';

		/* mount the file system */
		if (0 == mount(source, target, (char *) &type[1], flags, data)) {
			is_success = true;
			break;
		}
	} while(1);

	/* close the supported file systems list */
	(void) fclose(file_systems);

end:
	return is_success;
}
