#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>
#include <syslog.h>

#include "common.h"
#include "module.h"
#include "depmod.h"

/* the usage message */
#define USAGE "Usage: modprobe MODULE\n"

static bool _load_module(const char *name_or_alias, void *unused) {
	/* the module path */
	char path[PATH_MAX] = {'\0'};

	/* the Unix socket address */
	struct sockaddr_un unix_address = {0};

	/* the module */
	module_t module = {{0}};

	/* the module alias or name size */
	size_t size = 0;

	/* the number of received bytes */
	ssize_t bytes_received = 0;

	/* the Unix socket */
	int unix_socket = (-1);

	/* the return value */
	bool result = false;

	assert(NULL != name_or_alias);

	(void) unused;

	/* get the module name or alias size */
	size = strlen(name_or_alias);
	if (0 == size) {
		goto end;
	}
	size *= sizeof(char);

	/* create a Unix socket */
	unix_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == unix_socket) {
		goto end;
	}

	/* connect the socket */
	unix_address.sun_family = AF_UNIX;
	(void) strcpy(unix_address.sun_path, DEPMOD_SOCKET_PATH);
	if (-1 == connect(unix_socket,
	                  (struct sockaddr *) &unix_address,
	                  sizeof(unix_address))) {
		goto close_unix;
	}

	/* send the module name or alias to the loader */
	if ((ssize_t) size != send(unix_socket, name_or_alias, size, 0)) {
		goto close_unix;
	}

	/* receive the module path */
	bytes_received = recv(unix_socket, path, (sizeof(path) - 1), 0);
	if (0 >= bytes_received) {
		goto close_unix;
	}

	/* terminate the path */
	path[bytes_received] = '\0';

	/* open the module */
	if (false == module_open(&module, path)) {
		goto close_unix;
	}

	/* load the module dependencies - ignore failures, because depmod doesn't
	 * recognize built-in modules; if a dependency is missing, the kernel will
	 * fail to resolve symbols without any damage */
	(void) module_for_each_dependency(&module, _load_module, NULL);

	/* open the system log */
	openlog("modprobe", LOG_NDELAY | LOG_PID, LOG_USER);

	/* write the module name to the system log */
	syslog(LOG_INFO, "Loading %s", module.name);

	/* close the system log */
	closelog();

	/* load the module */
	if (false == module_load(&module)) {
		goto close_module;
	}

	/* report success */
	result = true;

close_module:
	/* close the module */
	module_close(&module);

close_unix:
	/* close the Unix socket */
	(void) close(unix_socket);

end:
	return result;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* load the specified module */
	if (false == _load_module(argv[1], NULL)) {
		goto end;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
