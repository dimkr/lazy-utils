#ifndef _MIME_H_INCLUDED
#	define _MIME_H_INCLUDED

typedef struct {
	const char *extension;
	unsigned int length;
	const char *mime_type;
} mime_type_t;

const char *mime_type_guess(const char *path);

#endif
