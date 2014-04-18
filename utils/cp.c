#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/sendfile.h>

/* the copied file permissions */
#define FILE_PERMISSIONS (0644)

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;
	
	/* file descriptors */
	int source;
	int dest;

	/* the source file attributes */
	struct stat source_attributes;

	/* the destination file attributes */
	struct stat dest_attributes;

	/* the destination path */
	char *dest_path;
	char path[PATH_MAX];

	/* the offset to copy from */
	off_t offset;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* get the source file size */
	if (-1 == stat(argv[1], &source_attributes))
		goto end;

	/* check whether the destination file exists; if yes and it's a directory, append the base file name to its path */
	if (-1 == stat(argv[1], &dest_attributes))
		dest_path = argv[2];
	else {
		if (!(S_ISDIR(dest_attributes.st_mode)))
			dest_path = argv[2];
		else {
			if (sizeof(path) <= snprintf((char *) &path,
			                             sizeof(path),
			                             "%s/%s",
			                             argv[1],
			                             basename(argv[2])))
				goto end;
			dest_path = (char *) &path;
		}
	}


	/* open the source file */
	source = open(argv[1], O_RDONLY);
	if (-1 == source)
		goto end;

	/* create the destination file */
	dest = open(dest_path, O_CREAT | O_WRONLY, FILE_PERMISSIONS);
	if (-1 == dest)
		goto close_source;
	
	/* copy the file */
	offset = 0;	
	if ((ssize_t) source_attributes.st_size != sendfile(
	                                     dest,
	                                     source,
	                                     &offset,
	                                     (size_t) source_attributes.st_size))
		goto close_dest;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_dest:
	/* close the destination file */
	(void) close(dest);

close_source:
	/* close the source file */
	(void) close(source);

end:
	return exit_code;
}
