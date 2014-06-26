#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "common.h"
#include "find.h"
#include "daemon.h"

/* the maximum size of a netlink message */
#define MAX_MESSAGE_SIZE (sizeof(char) * (1 + MAX_LENGTH))

/* the usage message */
#define USAGE "Usage: devd\n"

static bool _handle_existing_device(const char *path, void *unused) {
	/* the alias attributes */
	struct stat attributes = {0};

	/* the return value */
	bool result = false;

	/* the alias length */
	ssize_t length = 0;

	/* the file descriptor */
	int fd = (-1);

	/* the module alias */
	char *alias = NULL;

	assert(NULL != path);

	/* get the alias size */
	if (-1 == stat(path, &attributes)) {
		goto end;
	}

	/* allocate memory for the module alias */
	alias = malloc((size_t) attributes.st_size);
	if (NULL == alias) {
		goto end;
	}

	/* open the file */
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		goto free_alias;
	}

	/* read the alias and terminate it */
	length = read(fd, alias, (size_t) attributes.st_size);
	if (0 >= length) {
		goto close_file;
	}
	alias[length - 1] = '\0';

	/* run modprobe */
	switch (daemon_fork()) {
		case 0:
			(void) execlp("modprobe", "modprobe", alias, (char *) NULL);
			exit(EXIT_FAILURE);

		case (-1):
			goto close_file;

		default:
			result = true;
			break;
	}

close_file:
	/* close the file */
	(void) close(fd);

free_alias:
	/* free the alias */
	free(alias);

end:
	return result;
}

static bool _handle_new_device(unsigned char *message, const size_t len) {
	/* the return value */
	bool result = true;

	/* the current position inside the message */
	char *position = NULL;

	/* the delimeter position */
	char *delimeter = NULL;

	/* the event type */
	char *action = NULL;

	/* the kernel module alias */
	char *alias = NULL;

	/* the kernel module name */
	char *name = NULL;

	assert(NULL != message);
	assert(0 < len);

	/* locate the @ sign at the message beginning */
	position = strchr((char *) message, '@');
	if (NULL == position) {
		goto end;
	}

	do {
		/* locate the delimeter between the field name and its value */
		delimeter = strchr(position, '=');
		if (NULL == delimeter) {
			position += (1 + strlen(position));
			continue;
		}

		/* filter the action and module alias fields */
		if (0 == strncmp("MODALIAS=", position, STRLEN("MODALIAS="))) {
			alias = ++delimeter;
		} else {
			if (0 == strncmp("ACTION=", position, STRLEN("ACTION="))) {
				action = ++delimeter;
			} else {
				if (0 == strncmp("DRIVER=", position, STRLEN("DRIVER="))) {
					name = ++delimeter;
				}
			}
		}

		/* locate the line end and skip past the null byte */
		position += (1 + strlen(delimeter));
	} while (len >= ((unsigned char *) position - message));

	/* if no module name or alias was specified, do nothing */
	if (NULL == name) {
		if (NULL == alias) {
			goto end;
		}
		name = alias;
	}

	/* if a device was added, run modprobe */
	if (0 == strcmp("add", action)) {
		switch (daemon_fork()) {
			case 0:
				(void) execlp("modprobe", "modprobe", name, (char *) NULL);
				exit(EXIT_FAILURE);

			case (-1):
				goto end;
		}
	}

	/* report success */
	result = true;

end:
	return result;
}

int main(int argc, char *argv[]) {
	/* the receiving buffer */
	unsigned char buffer[MAX_MESSAGE_SIZE] = {0};

	/* the daemon data */
	daemon_t daemon_data = {{{0}}};

	/* the netlink socket address */
	struct sockaddr_nl netlink_address = {0};

	/* the received message size */
	ssize_t message_size = 0;

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a received signal */
	int received_signal = 0;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* create a netlink socket */
	daemon_data.fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (-1 == daemon_data.fd) {
		goto end;
	}

	/* bind the socket */
	netlink_address.nl_family = AF_NETLINK;
	netlink_address.nl_pid = getpid();
	netlink_address.nl_groups = (-1);
	if (-1 == bind(daemon_data.fd,
	               (struct sockaddr *) &netlink_address,
	               sizeof(netlink_address))) {
		goto close_netlink;
	}

	/* open the system log*/
	openlog("devd", LOG_NDELAY, LOG_DAEMON);

	/* write a log message before existing devices are handled */
	syslog(LOG_INFO, "Handling existing devices");

	/* load kernel modules for existing devices - each device has a file named
	 * "modalias" which specifies the matching module alias */
	if (false == find_all("/sys/devices",
	                      "modalias",
	                      (file_callback_t) _handle_existing_device,
	                      NULL)) {
		goto close_log;
	}

	/* initialize the daemon */
	if (false == daemon_init(&daemon_data, DAEMON_WORKING_DIRECTORY, NULL)) {
		goto close_log;
	}

	/* write another log message when newly added devices are handled */
	syslog(LOG_INFO, "Handling new devices");

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

		/* receive a message */
		message_size = recv(daemon_data.fd, buffer, (sizeof(buffer) - 1), 0);
		switch (message_size) {
			case (-1):
				if (EAGAIN != errno) {
					goto close_log;
				}

				/* fall through */

			case 0:
				continue;
		}

		/* terminate the message */
		buffer[message_size] = '\0';

		/* handle the received message */
		(void) _handle_new_device(buffer, (size_t) message_size);

	} while (1);

close_log:
	/* close the system log */
	closelog();

close_netlink:
	/* close the netlink socket */
	(void) close(daemon_data.fd);

end:
	return exit_code;
}
