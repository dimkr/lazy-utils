#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>

/* the IPC socket path */
#define IPC_SOCKET_PATH "/dev/log"

/* the log file path */
#define LOG_FILE_PATH "/var/log/messages"

/* the pipe path */
#define PIPE_PATH "/run/messages"

int main() {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a Unix socket */
	int ipc_socket;

	/* the Unix socket address */
	struct sockaddr_un ipc_socket_address;

	/* a named pipe */
	int fifo;

	/* the log file */
	int log_file;

	/* create a named pipe */
	if (-1 == mkfifo(PIPE_PATH, S_IWUSR | S_IRUSR))
		goto end;

	/* open the log file */
	log_file = open(LOG_FILE_PATH,
	                O_CREAT | O_APPEND | O_WRONLY,
	                S_IWUSR | S_IRUSR);
	if (-1 == log_file)
		goto delete_pipe;

	/* create the IPC socket; if it exists already, do nothing, since this
	 * indicates the daemon is already running or a previous one terminated
	 * brutally */
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
	if (false == daemonize())
		goto close_socket;

	/* open the pipe for writing */
	fifo = open(PIPE_PATH, O_WRONLY);
	if (-1 == fifo)
		goto close_socket;

	/* receive messages over the IPC socket and write them to the pipe */
	if (false == file_log_from_dgram_socket(ipc_socket, fifo))
		goto close_pipe;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_pipe:
	/* close the pipe */
	(void) close(fifo);

close_socket:
	/* close the IPC socket; upon success, delete the file */
	if (0 == close(ipc_socket))
		(void) unlink(IPC_SOCKET_PATH);

close_log_file:
	/* close the log file */
	(void) close(log_file);

delete_pipe:
	/* delete the pipe */
	(void) unlink(PIPE_PATH);

end:
	return exit_code;
}
