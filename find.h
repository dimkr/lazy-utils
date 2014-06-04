#ifndef _FIND_H_INCLUDED
#	define _FIND_H_INCLUDED

#	include <stdbool.h>

typedef bool (*file_callback_t)(const char *path, void *arg);

bool find_all(const char *directory,
              const char *pattern,
              const file_callback_t callback,
              void *arg);

#endif
