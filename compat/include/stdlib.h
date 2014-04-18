#ifndef _COMPAT_STDLIB_H_INCLUDED
#	define _COMPAT_STDLIB_H_INCLUDED

#	include_next <stdlib.h>
#	include <sys/types.h> /* u_int32_t */

u_int32_t arc4random(void);

#endif
