#ifndef _COMPAT_STDIO_H_INCLUDED
#	define _COMPAT_STDIO_H_INCLUDED

#	include_next <stdio.h>

char *fgetln(FILE *fp, size_t *lenp);

#endif
