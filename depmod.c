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

#include "common.h"
#include "daemon.h"
#include "find.h"
#include "module.h"
#include "cache.h"
#include "depmod.h"

/* the maximum size of a module alias */
#define MAX_ALIAS_LENGTH (MAX_LENGTH)

/* the usage message */
#define USAGE "Usage: depmod\n"

/* the listening backlog size */
#define BACKLOG_SIZE (50)

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

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a client socket */
	int client_socket = (-1);

	/* a received signal */
	int received_signal = 0;

	/* a cache entry */
	cache_entry_t *entry = NULL;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* create a Unix socket */
	daemon_data.fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == daemon_data.fd) {
		goto end;
	}

	/* bind the socket */
	unix_address.sun_family = AF_UNIX;
	(void) strcpy(unix_address.sun_path, DEPMOD_SOCKET_PATH);
	if (-1 == bind(daemon_data.fd,
	               (struct sockaddr *) &unix_address,
	               sizeof(unix_address))) {
		goto close_unix;
	}

	/* start listening */
	if (-1 == listen(daemon_data.fd, BACKLOG_SIZE)) {
		goto close_unix;
	}

	/* open the system log */
	openlog("depmod", LOG_NDELAY, LOG_DAEMON);

	/* gather information about available kernel modules */
	syslog(LOG_INFO, "Generating cache");
	if (false == cache_generate(&cache)) {
		goto close_log;
	}

	/* initialize the daemon */
	if (false == daemon_init(&daemon_data, DAEMON_WORKING_DIRECTORY)) {
		goto free_cache;
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

		/* accept a client */
		client_socket = accept(daemon_data.fd, NULL, NULL);
		if (-1 == client_socket) {
			break;
		}

		/* receive a module alias */
		size = recv(client_socket, alias, (sizeof(alias) - 1), 0);
		switch (size) {
			case (-1):
				if (EAGAIN != errno) {
					(void) close(client_socket);
					goto free_cache;
				}

				/* fall through */

			case 0:
				goto disconnect_client;
		}

		/* terminate the alias */
		alias[size] = '\0';

		/* locate the module, by either alias or name */
		entry = cache_find_module(&cache, alias);
		if (NULL == entry) {
			entry = cache_find_alias(&cache, alias);
			if (NULL == entry) {
				syslog(LOG_ERR, "Failed to locate %s", alias);
				goto disconnect_client;
			}
		}

		/* send the module path to the client */
		(void) send(client_socket,
		            entry->path,
		            sizeof(char) * strlen(entry->path),
		            0);

disconnect_client:
		/* disconnect the client */
		(void) close(client_socket);
	} while (1);

free_cache:
	/* free the cache */
	cache_free(&cache);

close_log:
	/* close the system log */
	closelog();

close_unix:
	/* close the Unix socket */
	(void) close(daemon_data.fd);

	/* delete the Unix socket */
	(void) unlink(DEPMOD_SOCKET_PATH);

end:
	return exit_code;
}
