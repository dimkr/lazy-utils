#ifndef _COMMON_H_INCLUDED
#	define _COMMON_H_INCLUDED

#	include <unistd.h>

#	define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#	define STRLEN(str) (ARRAY_SIZE(str) - 1)

#	define PRINT(x) (void) write(STDOUT_FILENO, x, sizeof(x))

#	define MAX_LENGTH (2047)

#endif
