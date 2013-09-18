#ifndef _IO_H_INCLUDED
#	define _IO_H_INCLUDED

#	include <stdio.h>
#	include <stdbool.h>
#	include <sys/types.h>
#	include <dirent.h>

/* the buffer size used for chunked file operations */
# define FILE_READING_BUFFER_SIZE (BUFSIZ)

typedef struct {
	unsigned char *contents;
	size_t size;
} file_t;

bool file_read(file_t *file, const char *path);
void file_close(file_t *file);

typedef bool (*file_callback_t)(const char *path,
                                const char *name,
                                void *parameter,
                                struct dirent *entry);

bool file_for_each(const char *path,
                   const char *name,
                   void *parameter,
                   file_callback_t callback);
bool file_find(const char *path, const char *name, char *full_path);
bool file_find_wildcard(const char *path,
                        const char *pattern,
                        char *full_path);

#endif
