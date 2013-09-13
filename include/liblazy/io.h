#ifndef _IO_H_INCLUDED
#	define _IO_H_INCLUDED

#	include <stdio.h>

/* the buffer size used for chunked file operations */
#	define FILE_READING_BUFFER_SIZE (BUFSIZ)

char *file_read(const char *path, size_t *len);

#endif
