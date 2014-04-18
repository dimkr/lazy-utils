#ifndef _COMPAT_STRING_H_INCLUDED
#	define _COMPAT_STRING_H_INCLUDED

#	include_next <string.h>

#	define strlcpy strncpy
#	define strlcat strncat

#endif
