#ifndef _DAEMON_H_INCLUDED
#	define _DAEMON_H_INCLUDED

#	include <stdbool.h>
#	include <signal.h>

#	define DAEMON_WORKING_DIRECTORY "/run"

typedef struct {
	sigset_t signal_mask;
	int fd;
	int io_signal;
} daemon_t;

bool daemon_init(daemon_t *daemon, const char *working_directory);
bool daemon_daemonize(const char *working_directory);
bool daemon_wait(const daemon_t *daemon, int *received_signal);
pid_t daemon_fork();

#endif
