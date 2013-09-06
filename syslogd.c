#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* the IPC socket backlog size */
#define BACKLOG_SIZE (10)

/* the IPC socket path */
#define IPC_SOCKET_PATH "/dev/log"

/* the log file path */
#define LOG_FILE_PATH "/var/log/messages"

/* the maximum size of a message */
#define MAX_MESSAGE_SIZE (4096)

int main() {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the log file */
	int log_file;

	/* the Unix socket */
	int ipc_socket;

	/* the Unix socket address */
	struct sockaddr_un ipc_socket_address;

	/* a message */
	char message[MAX_MESSAGE_SIZE];

	/* the size of a message */
	ssize_t message_size;

	/* open the log file */
	log_file = open(LOG_FILE_PATH,
	                O_CREAT | O_APPEND | O_WRONLY,
	                S_IWUSR | S_IRUSR);
	if (-1 == log_file)
		goto end;

	/* create the IPC socket */
    ipc_socket_address.sun_family = AF_UNIX;
    (void) strcpy(ipc_socket_address.sun_path, IPC_SOCKET_PATH);
	ipc_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == ipc_socket)
		goto close_log_file;

	/* bind the socket */
	if (-1 == bind(ipc_socket,
	               (struct sockaddr *) &ipc_socket_address,
	               sizeof(ipc_socket_address)))
		goto close_socket;

	/* daemonize */
	if (-1 == daemon(0, 0))
		goto end;

	do {
		/* receive a message */
		message_size = recvfrom(ipc_socket,
		                        (char *) &message,
		                        sizeof(message),
		                        0,
		                        NULL,
		                        NULL);
		if (-1 == message_size)
			goto close_socket;

		/* write the message to the log file */
		if (0 != message_size) {
			if (message_size != write(log_file,
			                          (char *) &message,
			                          (size_t) message_size))
			goto close_socket;
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the IPC socket */
	(void) close(ipc_socket);

close_log_file:
	/* close the log file */
	(void) close(log_file);

end:
	return exit_code;
}
