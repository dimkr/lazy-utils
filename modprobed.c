#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <stdbool.h>

#include "common.h"
#include "daemon.h"
#include "find.h"
#include "module.h"
#include "cache.h"
#include "modprobed.h"

/* the maximum size of a module alias */
#define MAX_ALIAS_LENGTH (MAX_LENGTH)

/* the usage message */
#define USAGE "Usage: modprobed\n"

/* the listening backlog size */
#define BACKLOG_SIZE (50)

static bool _load_by_name_or_alias(const char *name_or_alias, void *cache);

static bool _load_by_path(const char *path, void *cache) {
	/* the module */
	module_t module = {{0}};

	/* the return value */
	bool result = true;

	/* open the module */
	if (false == module_open(&module, path)) {
		goto end;
	}

	/* write the module name to the system log */
	syslog(LOG_INFO, "Loading %s", module.name);

	/* load the module dependencies - ignore failures, because built-in modules
	 * are not handled; if a dependency is missing, the kernel will fail to
	 * resolve symbols without any damage */
	result = module_for_each_dependency(&module, _load_by_name_or_alias, cache);
	if (true == result) {

		/* load the module; report failure only upon failure to load it */
		result = module_load(&module);
	}

	/* close the module */
	module_close(&module);

end:
	return result;
}

static bool _load_by_name_or_alias(const char *name_or_alias, void *cache) {
	/* a cache entry */
	cache_entry_t *entry = NULL;

	/* locate the module, by either alias or name */
	entry = cache_find_module((cache_t *) cache, name_or_alias);
	if (NULL == entry) {
		entry = cache_find_alias((cache_t *) cache, name_or_alias);
		if (NULL == entry) {
			syslog(LOG_ERR, "Failed to locate %s", name_or_alias);
			return true;
		}
	}

	/* load it */
	return _load_by_path(entry->path, cache);
}

int main(int argc, char *argv[]) {
	/* a module alias */
	char alias[1 + MAX_ALIAS_LENGTH] = {'\0'};

	/* the Unix socket address */
	struct sockaddr_un unix_address = {0};

	/* the daemon data */
	daemon_t daemon_data = {{{0}}};

	/* module information cache */
	cache_t cache = {0};

	/* the alias size */
	ssize_t size = 0;

	/* the process ID */
	pid_t pid = (-1);

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a received signal */
	int received_signal = 0;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* open the system log */
	openlog("modprobed", LOG_NDELAY, LOG_DAEMON);

	/* gather information about available kernel modules */
	syslog(LOG_INFO, "Generating cache");
	if (false == cache_generate(&cache)) {
		goto close_log;
	}

	/* create a Unix socket */
	daemon_data.fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == daemon_data.fd) {
		goto free_cache;
	}

	/* bind the socket */
	unix_address.sun_family = AF_UNIX;
	(void) strcpy(unix_address.sun_path, MODPROBED_SOCKET_PATH);
	if (-1 == bind(daemon_data.fd,
	               (struct sockaddr *) &unix_address,
	               sizeof(unix_address))) {
		goto close_unix;
	}

	/* initialize the daemon */
	if (false == daemon_init(&daemon_data, DAEMON_WORKING_DIRECTORY, NULL)) {
		goto close_unix;
	}

	do {
		/* wait for a message */
		if (false == daemon_wait(&daemon_data, &received_signal)) {
			break;
		}

		/* if the received signal is a termination one, report success */
		if (SIGTERM == received_signal) {
			exit_code = EXIT_SUCCESS;
			break;
		}

		/* receive a module name or alias */
		size = recvfrom(daemon_data.fd,
		                alias,
		                (sizeof(alias) - 1),
		                0,
		                NULL,
		                NULL);
		switch (size) {
			case (-1):
				if (EAGAIN != errno) {
					goto close_unix;
				}

				/* fall through */

			case 0:
			case (sizeof(alias) - 1):
				continue;
		}

		/* terminate the alias */
		alias[size] = '\0';

		/* try to load the module, in a child process */
		pid = daemon_fork();
		switch (pid) {
			case 0:
				if (true == _load_by_name_or_alias(alias, &cache)) {
					exit_code = EXIT_SUCCESS;
				}
				goto close_unix;

			case (-1):
				goto close_unix;
		}
	} while (1);

close_unix:
	/* close the Unix socket */
	(void) close(daemon_data.fd);

	/* delete the Unix socket */
	if (0 != pid) {
		(void) unlink(MODPROBED_SOCKET_PATH);
	}

free_cache:
	/* free the cache */
	cache_free(&cache);

close_log:
	/* close the system log */
	closelog();

end:
	return exit_code;
}
