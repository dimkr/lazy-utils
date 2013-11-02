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
