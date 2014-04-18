#ifndef _COMPAT_CDEFS_H_INCLUDED
#	define _COMPAT_CDEFS_H_INCLUDED

#	define __dead __attribute__((noreturn))

#	define __IDSTRING(_n, _s) __attribute__((used)) static const char _n[] = _s
#	define __RCSID(_s) __IDSTRING(rcsid, _s)
#	define __COPYRIGHT(_s) __IDSTRING(copyright, _s)

#endif
