#ifndef _COMMON_H_INCLUDED
#	define _COMMON_H_INCLUDED

#	define STRLEN(x) (sizeof(x) - sizeof(char))
#	define ELEMENT_SIZE(x) (sizeof(x[0]))
#	define ARRAY_SIZE(x) (sizeof(x) / ELEMENT_SIZE(x))

#endif
