/*
 * this applet was built using documentation found at:
 *   - https://www.kernel.org/doc/pending/hotplug.txt
 *   - http://www.linuxfromscratch.org/lfs/view/6.2/chapter07/udev.html
 */

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include <linux/netlink.h>
#include <liblazy/io.h>
#include <liblazy/kmodule.h>

#define SYSFS_MOUNT_POINT "/sys"

#define MODULE_ALIAS_FILE_NAME "modalias"

#define MAX_MODULE_ALIAS_LENGTH (1023)

/* the source of system log messages */
#define LOG_IDENTITY "kmoduled"

/* the buffer size for received messages */
#define BUFFER_SIZE (1 + FILE_READING_BUFFER_SIZE)

bool _load_module_for_device(const char *alias) {
	if (true == kmodule_load_by_alias(alias))
		return true;

	syslog(LOG_ERR, "failed to load a kernel module for %s\n", alias);
	return false;
}

bool _handle_existing_device(const char *path,
                             const char *name,
                             void *unused,
                             struct dirent *entry) {
	/* the return value */
	bool should_stop = true;

	/* the module alias file path */
	char module_alias_path[PATH_MAX];

	/* the module alias */
	char module_alias[1 + MAX_MODULE_ALIAS_LENGTH];

	/* the module alias length */
	ssize_t module_alias_length;

	/* the module alias file */
	int fd;

	/* if the listed file is not a module alias file, skip it */
	if (0 != strcmp(MODULE_ALIAS_FILE_NAME, (char *) &entry->d_name))
		return false;

	/* open the module alias file */
	(void) sprintf((char *) &module_alias_path,
	               "%s/%s",
	               path,
	               (char *) &entry->d_name);
	fd = open((char *) &module_alias_path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* read the file contents */
	module_alias_length = read(fd,
	                           (char *) &module_alias,
	                           STRLEN(module_alias));
	if (0 >= module_alias_length)
		goto close_file;

	/* terminate the file contents */
	module_alias[module_alias_length - sizeof(char)] = '\0';

	/* load the matching kernel module */
	if (false == _load_module_for_device((char *) &module_alias))
		goto close_file;

	/* if everything went fine, continue to the next device */
	should_stop = false;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return false;
	return should_stop;
}

bool _handle_existing_devices() {
	return file_for_each(SYSFS_MOUNT_POINT,
	                     MODULE_ALIAS_FILE_NAME,
	                     NULL,
	                     (file_callback_t) _handle_existing_device);
}

bool _handle_new_device(unsigned char *message, size_t len) {
	/* the return value */
	bool is_success = true;

	/* the current position inside the message */
	char *position;

	/* the delimeter position */
	char *delimeter;

	/* the event type */
	char *action;

	/* the kernel module alias */
	char *module_alias;

	/* locate the @ sign at the message beginning */
	position = strchr((char *) message, '@');
	if (NULL == position)
		goto end;

	/* initialize the action and the module alias */
	action = NULL;
	module_alias = NULL;

	do {
		/* locate the delimeter between the field name and its value */
		delimeter = strchr(position, '=');
		if (NULL == delimeter) {
			position += (1 + strlen(position));
			continue;
		}

		/* filter the action and module alias fields */
		if (0 == strncmp("MODALIAS=", position, STRLEN("MODALIAS="))) {
			module_alias = ++delimeter;
		} else {
			if (0 == strncmp("ACTION=", position, STRLEN("ACTION=")))
				action = ++delimeter;
		}

		/* locate the line end and skip past the NULL byte */
		position += (1 + strlen(delimeter));
	} while (len >= ((unsigned char *) position - message));


	/* if no module alias was specified, do nothing */
	if (NULL == module_alias)
		goto end;

	/* if a device was added, load its kernel module */
	if (0 == strcmp("add", action)) {
		if (false == _load_module_for_device(module_alias))
			goto end;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a signal mask */
	sigset_t signal_mask;

	/* a netlink socket */
	int ipc_socket;

	/* the socket flags */
	int flags;

	/* a received signal */
	int received_signal;

	/* the socket address */
	struct sockaddr_nl address;

	/* the receiving buffer */
	unsigned char buffer[BUFFER_SIZE];

	/* the received message size */
	ssize_t message_size;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* block SIGIO, SIGINT and SIGTERM signals */
	if (-1 == sigemptyset(&signal_mask))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGINT))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto end;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto end;

	/* create a netlink socket */
	ipc_socket = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (-1 == ipc_socket)
		goto end;

	/* get the IPC socket flags */
	flags = fcntl(ipc_socket, F_GETFL);
	if (-1 == flags)
		goto close_socket;

	/* enable non-blocking, asynchronous I/O */
	if (-1 == fcntl(ipc_socket, F_SETFL, flags | O_NONBLOCK | O_ASYNC))
		goto close_socket;

	/* daemonize */
	if (-1 == daemon(0, 0))
		goto end;

	/* bind the socket */
	address.nl_family = AF_NETLINK;
	address.nl_pid = getpid();
	address.nl_groups = (-1);
	if (-1 == bind(ipc_socket, (struct sockaddr *) &address, sizeof(address)))
		goto close_socket;

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY, LOG_USER);

	/* load kernel modules for existing devices */
	syslog(LOG_INFO, "loading kernel modules for existing devices\n");
	if (true == _handle_existing_devices())
		goto close_socket;

	syslog(LOG_INFO, "waiting for uevents\n");

	/* change the IPC socket ownership */
	if (-1 == fcntl(ipc_socket, F_SETOWN, address.nl_pid))
		goto close_socket;

	do {
		/* receive a message */
		message_size = recv(ipc_socket,
		                    (unsigned char *) &buffer,
		                    (sizeof(buffer) - 1),
		                    0);
		switch (message_size) {
			case (-1):
				if (EAGAIN != errno)
					goto close_socket;
				break;

			case (0):
				break;

			default:
				/* terminate the message */
				buffer[message_size] = '\0';

				/* handle the received message */
				(void) _handle_new_device((unsigned char *) &buffer,
				                          (size_t) message_size);

				break;
		}

		/* wait until a signal is received */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto close_socket;

		/* if the received signal is a termination one, stop */
		if (SIGIO != received_signal) {
			syslog(LOG_INFO, "shutting down\n");
			break;
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the netlink socket */
	(void) close(ipc_socket);

	/* close the system log */
	closelog();

end:
	return exit_code;
}
