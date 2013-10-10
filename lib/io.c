#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <liblazy/io.h>

bool file_read(file_t *file, const char *path) {
	/* the return value */
	bool is_success = false;

	/* the file information */
	struct stat file_info;

	/* the file descriptor */
	int fd;

	/* get the file size */
	if (-1 == lstat(path, &file_info))
		goto end;

	/* open the file */
	fd = open(path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* map the file to memory */
	file->size = (size_t) file_info.st_size;
	file->contents = mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (NULL == file->contents)
		goto close_file;

	/* report success */
	is_success = true;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

void file_close(file_t *file) {
	/* unmap the file contents */
	(void) munmap(file->contents, file->size);
}

bool _plain_search_callback(const char *path,
                            const char *name,
                            char *full_path,
                            struct dirent *entry) {
	/* if the file name matches, return its full path */
	if (0 != strcmp(name, (char *) &entry->d_name))
		return false;

	(void) sprintf(full_path, "%s/%s", path, (char *) &entry->d_name);
	return true;
}

bool file_for_each(const char *path,
                   const char *name,
                   void *parameter,
                   file_callback_t callback) {
	/* the return value */
	bool is_found = false;

	/* the directory */
	DIR *directory;

	/* the directory contents */
	struct dirent contents;
	struct dirent *contents_pointer;

	/* sub-directory path */
	char *sub_directory = NULL;

	/* open the directory */
	directory = opendir(path);
	if (NULL == directory)
		goto end;

	do {
		/* list one entry under the directory */
		if (0 != readdir_r(directory, &contents, &contents_pointer))
			goto free_sub_directory_path;

		/* if the last entry was reached without a match, report failure */
		if (NULL == contents_pointer)
			goto free_sub_directory_path;

		if ((0 == strcmp(".", (char *) &contents_pointer->d_name)) ||
		    (0 == strcmp("..", (char *) &contents_pointer->d_name)))
			continue;

		switch (contents_pointer->d_type) {
			case DT_REG:
				/* run the callback */
				if (true == callback(path, name, parameter, contents_pointer))
					goto success;
				break;

			case DT_DIR:
				/* allocate memory for the sub-directory path */
				if (NULL == sub_directory) {
					sub_directory = malloc(sizeof(char) * PATH_MAX);
					if (NULL == sub_directory)
						goto free_sub_directory_path;
				}

				/* obtain the full sub-directory path */
				(void) sprintf(sub_directory,
				               "%s/%s",
				               path,
				               (char *) &contents_pointer->d_name);

				/* recurse into the sub-directory */
				if (true == file_for_each(sub_directory,
				                          name,
				                          parameter,
				                          callback))
					goto success;
				break;
		}
	} while (1);

	goto free_sub_directory_path;

success:
	/* report success */
	is_found = true;

free_sub_directory_path:
	/* free the sub-directory path */
	if (NULL != sub_directory)
		free(sub_directory);

	/* close the directory */
	(void) closedir(directory);

end:
	return is_found;
}

bool file_find(const char *path, const char *name, char *full_path) {
	return file_for_each(path,
	                     name,
	                     full_path,
	                     (file_callback_t) _plain_search_callback);
}

bool _file_log(const int source,
               ssize_t (*reader)(int fd, void *buffer, size_t len),
               const int destination) {
	/* the return value */
	bool is_success = false;

	/* the reading buffer */
	unsigned char buffer[FILE_READING_BUFFER_SIZE];

	/* the number of bytes read into the buffer */
	ssize_t bytes_read;

	/* a signal mask */
	sigset_t signal_mask;

	/* a received signal */
	int received_signal;

	/* the signal which indicates there is data to log */
	int io_signal;

	/* pick the minimum real-time signal */
	io_signal = SIGRTMIN;

	/* start blocking io_signal and SIGTERM */
	if (-1 == sigemptyset(&signal_mask))
		goto end;
	if (-1 == sigaddset(&signal_mask, io_signal))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto end;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto end;

	/* enable non-blocking, asynchronous I/O */
	if (false == file_enable_async_io(source, io_signal))
		goto end;

	do {
		/* read from the message source */
		bytes_read = reader(source, (char *) &buffer, sizeof(buffer));
		switch (bytes_read) {
			case (-1):
				if (EAGAIN != errno)
					goto end;
				break;

			/* if the buffer is empty, do nothing */
			case (0):
				break;

			default:
				/* otherwise, write the buffer to the log file */
				if (bytes_read != write(destination,
				                        (char *) &buffer,
				                        (size_t) bytes_read))
					goto end;
				break;
		}

		/* wait for more data to arrive, by waiting for a signal */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto end;

		/* if the received signal is a termination signal, stop */
		if (SIGTERM == received_signal)
			break;
	} while (1);

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool file_log_from_file(const int source, const int destination) {
	return _file_log(source, read, destination);
}

ssize_t _dgram_socket_read(int fd, void *buffer, size_t len) {
	return recvfrom(fd, buffer, len, 0, NULL, NULL);
}

bool file_log_from_dgram_socket(const int source, const int destination) {
	return _file_log(source, _dgram_socket_read, destination);
}

bool file_enable_async_io(const int fd, const int io_signal) {
	/* the return value */
	bool is_success = false;

	/* file descriptor flags */
	int flags;

	/* get the message source flags */
	flags = fcntl(fd, F_GETFL);
	if (-1 == flags)
		goto end;

	/* set the received signal */
	if (-1 == fcntl(fd, F_SETSIG, io_signal))
		goto end;

	/* enable non-blocking, asynchronous I/O */
	if (-1 == fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_ASYNC))
		goto end;

	/* change the message source ownership */
	if (-1 == fcntl(fd, F_SETOWN, getpid()))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool file_print(const char *path) {
	/* the return value */
	bool is_success = false;

	/* the file */
	int file;

	/* a buffer */
	char buffer[FILE_READING_BUFFER_SIZE];

	/* read chunk size */
	ssize_t chunk_size;

	/* open the file */
	file = open(path, O_RDONLY);
	if (-1 == file)
		goto end;

	do {
		/* read a chunk from the file */
		chunk_size = read(file, (char *) &buffer, sizeof(buffer));
		if (-1 == chunk_size)
			goto close_file;

		/* print the read chunk */
		if (chunk_size != write(STDOUT_FILENO,
		                        (char *) &buffer,
		                        (size_t) chunk_size))
			goto close_file;

		/* if the chunk is smaller than the buffer size, the file end was
		 * reached */
		if (sizeof(buffer) > chunk_size)
			break;
	} while (1);

	/* report success */
	is_success = true;

close_file:
	/* close the file */
	(void) close(file);

end:
	return is_success;
}
