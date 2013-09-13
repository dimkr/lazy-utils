#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

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

	/* a signal mask */
	sigset_t signal_mask;

	/* file descriptor flags */
	int flags;

	/* a received signal */
	int received_signal;

	/* open the log file */
	log_file = open(LOG_FILE_PATH,
	                O_CREAT | O_APPEND | O_WRONLY,
	                S_IWUSR | S_IRUSR);
	if (-1 == log_file)
		goto end;

	/* create the IPC socket; if it exists already, do nothing, since this
	 * indicates the daemon is already running or a previous one terminated
	 * brutally */
	ipc_socket_address.sun_family = AF_UNIX;
	(void) strcpy(ipc_socket_address.sun_path, IPC_SOCKET_PATH);
	ipc_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == ipc_socket)
		goto close_log_file;

	/* start blocking SIGIO, SIGINT and SIGTERM */
	if (-1 == sigemptyset(&signal_mask))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGINT))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto close_socket;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto close_socket;

	/* get the IPC socket flags */
	flags = fcntl(ipc_socket, F_GETFL);
	if (-1 == flags)
		goto close_socket;

	/* enable non-blocking, asynchronous I/O */
	if (-1 == fcntl(ipc_socket, F_SETFL, flags | O_NONBLOCK | O_ASYNC))
		goto close_socket;

	/* bind the socket */
	if (-1 == bind(ipc_socket,
	               (struct sockaddr *) &ipc_socket_address,
	               sizeof(ipc_socket_address)))
		goto close_socket;

	/* daemonize */
	if (-1 == daemon(0, 0))
		goto end;

	/* change the IPC socket ownership; the process ID changed during the
	 * daemonization; from now and on, SIGIO will be received once a message is
	 * received */
	if (-1 == fcntl(ipc_socket, F_SETOWN, getpid()))
		goto close_socket;

	do {
		/* receive a message; if a message was received during the
		 * initialization stage, no signal will be sent, so this must happen
		 * before the daemon starts waiting for signals; otherwise, recvfrom()
		 * will fail with EAGAIN in errno */
		message_size = recvfrom(ipc_socket,
		                        (char *) &message,
		                        sizeof(message),
		                        0,
		                        NULL,
		                        NULL);
		switch (message_size) {
			case (-1):
				if (EAGAIN != errno)
					goto close_socket;
				break;

			/* if the received message is empty, do nothing */
			case (0):
				break;

			default:
				/* otherwise, write the message to the log file */
				if (0 != message_size) {
					if (message_size != write(log_file,
					                          (char *) &message,
					                          (size_t) message_size))
					goto close_socket;
				}
				break;
		}

		/* wait for the next message to arrive */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto close_socket;

		/* if the received signal is a termination signal, stop */
		if ((SIGTERM == received_signal) || (SIGINT == received_signal))
			break;
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the IPC socket; upon success, delete the file */
	if (0 == close(ipc_socket))
		(void) unlink(IPC_SOCKET_PATH);

close_log_file:
	/* close the log file */
	(void) close(log_file);

end:
	return exit_code;
}
