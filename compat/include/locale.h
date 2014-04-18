#ifndef _COMPAT_LOCALE_H_INCLUDED
#	define _COMPAT_LOCALE_H_INCLUDED

#	include_next <locale.h>

#	ifndef LC_CTYPE
#		include <bits/locale.h>
#		define LC_CTYPE (__LC_CTYPE)
#	endif

#endif
