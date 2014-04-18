#ifndef _COMPAT_STDLIB_H_INCLUDED
#	define _COMPAT_STDLIB_H_INCLUDED

#	include_next <stdlib.h>
#	include <sys/types.h> /* u_int32_t */

#	define setprogname(x) argv[0] = x
#	define getprogname() argv[0]

u_int32_t arc4random(void);

#endif
