#ifndef _DAEMON_H_INCLUDED
#	define _DAEMON_H_INCLUDED

#	include <stdbool.h>

#	define PROCESS_WORKING_DIRETORY "/run"

bool daemonize();

#endif
