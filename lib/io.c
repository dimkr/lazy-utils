#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <liblazy/io.h>

char *file_read(const char *path, size_t *len) {
	/* the return value */
	char *contents = NULL;

	/* the enlarged contents buffer */
	char *more_contents;

	/* the enlarged contents buffer size */
	size_t more_len;

	/* the file descriptor */
	int fd;

	ssize_t chunk_size;

	/* open the file */
	fd = open(path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* initialize the file size counter */
	contents = NULL;
	*len = 0;

	do {
		/* allocate a chunk of memory */
		more_len = FILE_READING_BUFFER_SIZE + *len;
		more_contents = realloc(contents, more_len);
		if (NULL == more_contents)
			goto free_contents;
		contents = more_contents;

		/* read a chunk */
		chunk_size = read(fd,
		                  (void *) &contents[*len],
		                  FILE_READING_BUFFER_SIZE);
		switch (chunk_size) {
			case (0):
				goto close_file;

			case (-1):
				goto free_contents;

			default:
				*len += chunk_size;
				if (FILE_READING_BUFFER_SIZE != chunk_size)
					goto close_file;
				break;
		}
	} while (1);

free_contents:
	/* free the buffer containing the file contents */
	free(contents);
	contents = NULL;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return contents;
}
