#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>
#include <liblazy/detachd.h>

bool _send_packet(const int fd,
                  const struct sockaddr_un *address,
                  const output_packet_t *packet) {
	/* the return value */
	bool is_success = false;

	/* the number of bytes sent */
	ssize_t bytes_sent;

	/* the packet size */
	size_t size;

	/* send the output to the client */
	size = sizeof(packet->header) + (size_t) packet->header.size;
	bytes_sent = sendto(fd,
	                    packet,
	                    size,
	                    0,
	                    (const struct sockaddr *) address,
	                    sizeof(*address));
	switch (bytes_sent) {
		case (-1):
			if (ENOENT != errno)
				goto end;
			break;

		default:
			if ((ssize_t) size != bytes_sent)
				goto end;
			break;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a named pipe */
	int fifo;

	/* the named pipe path */
	char fifo_path[] = "/tmp/detachdXXXXXX";

	/* the child process ID */
	pid_t pid;

	/* a signal mask */
	sigset_t signal_mask;

	/* the signal which indicates there is output to read */
	int output_signal;

	/* a received signal */
	int received_signal;

	/* a packet sent to the client */
	output_packet_t packet;

	/* the socket used to send the child process output */
	int output_socket;

	/* the client address */
	struct sockaddr_un address;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* pick the minimum real-time signal */
	output_signal = SIGRTMIN;

	/* block output_signal, SIGTERM and SIGCHLD signals */
	if (-1 == sigemptyset(&signal_mask))
		goto end;
	if (-1 == sigaddset(&signal_mask, output_signal))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGCHLD))
		goto end;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto end;

	/* daemonize */
	if (false == daemonize())
		goto end;

	/* get a random, temporary file path */
	(void) mktemp((char *) &fifo_path);
	if ('\0' == fifo_path[0])
		goto end;

	/* create a named pipe under the temporary file path */
	if (-1 == mkfifo((char *) &fifo_path, O_RDWR))
		goto end;

	/* open the pipe */
	fifo = open((char *) &fifo_path, O_RDONLY | O_NONBLOCK);
	if (-1 == fifo)
		goto delete_fifo;

	/* create a socket */
	output_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == output_socket)
		goto close_fifo;

	/* create a child process */
	pid = fork();
	switch (pid) {
		case (-1):
			goto close_socket;

		case 0:
			/* redirect the child process output to the pipe */
			if (-1 == close(STDIN_FILENO))
				goto terminate_child;
			if (STDIN_FILENO != open("/dev/null", O_RDONLY))
				goto terminate_child;
			if (-1 == close(STDOUT_FILENO))
				goto terminate_child;
			if (STDOUT_FILENO != open((char *) &fifo_path, O_WRONLY))
				goto terminate_child;
			if (-1 == close(STDERR_FILENO))
				goto terminate_child;
			if (STDERR_FILENO != dup(STDOUT_FILENO))
				goto terminate_child;

			/* run the child process */
			(void) execl("/bin/sh", "sh", "-c", argv[2], (char *) NULL);

terminate_child:
			/* terminate the child process */
			exit(EXIT_FAILURE);
	}

	/* enable asynchronous I/O */
	if (false == file_enable_async_io(fifo, output_signal))
		goto close_socket;

	/* create the socket address structure */
	address.sun_family = AF_UNIX;
	(void) strcpy((char *) &address.sun_path, argv[1]);

	do {
wait_for_a_signal:

		/* wait for a signal */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto close_socket;

		switch (received_signal) {
			case SIGTERM:
				/* terminate the child process */
				if (-1 == kill(pid, SIGTERM))
					goto close_socket;

				/* wait for the child process to terminate */

			case SIGCHLD:
				/* destroy the zombie child process */
				if (pid != waitpid(pid, NULL, 0))
					goto close_socket;

				/* notify the client about the child process termination */
				packet.header.type = PACKET_TYPE_CHILD_EXIT;
				packet.header.size = 0;
				if (false == _send_packet(output_socket, &address, &packet))
					goto close_socket;

				goto success;
		}

		do {
			/* read an output chunk */
			packet.header.size = (unsigned int) read(fifo,
			                                         (void *) &packet.buffer,
			                                         sizeof(packet.buffer));
			switch (packet.header.size) {
				case (-1):
					if (EAGAIN != errno)
						goto close_socket;
					break;

				case 0:
					goto wait_for_a_signal;

				default:
					/* send the output chunk */
					packet.header.type = PACKET_TYPE_OUTPUT_CHUNK;
					if (false == _send_packet(output_socket, &address, &packet))
						goto close_socket;
					break;
			}
		} while (1);
	} while (1);

success:
	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the socket */
	(void) close(output_socket);

close_fifo:
	/* close the named pipe */
	(void) close(fifo);

delete_fifo:
	/* delete the named pipe */
	(void) unlink((char *) &fifo_path);

end:
	return exit_code;
}
