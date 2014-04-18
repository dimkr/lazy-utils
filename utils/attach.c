#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <liblazy/io.h>
#include <liblazy/detachd.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a signal mask */
	sigset_t signal_mask;

	/* the signal which indicates there is output to read */
	int output_signal;

	/* a received signal */
	int received_signal;

	/* the socket used to receive the child process output */
	int output_socket;

	/* a received packet */
	output_packet_t packet;

	/* the packet size */
	ssize_t packet_size;

	/* the socket address */
	struct sockaddr_un address;

	/* make sure the number of command-line arguments is valid */
	if (2 != argc)
		goto end;

	/* pick the minimum real-time signal */
	output_signal = SIGRTMIN;

	/* block output_signal, SIGTERM and SIGINT signals */
	if (-1 == sigemptyset(&signal_mask))
		goto end;
	if (-1 == sigaddset(&signal_mask, output_signal))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGINT))
		goto end;
	if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL))
		goto end;

	/* create a socket */
	output_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (-1 == output_socket)
		goto end;

	/* bind the socket */
	address.sun_family = AF_UNIX;
	(void) strcpy((char *) &address.sun_path, argv[1]);
	if (-1 == bind(output_socket,
	               (const struct sockaddr *) &address,
	               sizeof(address)))
		goto close_socket;

	/* enable asynchronous I/O */
	if (false == file_enable_async_io(output_socket, output_signal))
		goto delete_socket;

	do {
		/* wait for a signal */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto delete_socket;

		switch (received_signal) {
			case SIGTERM:
			case SIGINT:
				goto success;
		}

		/* receive a packet */
		packet_size = recv(output_socket, (void *) &packet, sizeof(packet), 0);
		switch (packet_size) {
			case (-1):
			case 0:
				goto delete_socket;

			default:
				switch (packet.header.type) {
					/* if the child process has terminated, report success */
					case PACKET_TYPE_CHILD_EXIT:
						goto success;

					/* otherwise, print the received output chunk */
					case PACKET_TYPE_OUTPUT_CHUNK:
						if ((ssize_t) packet.header.size != write(
						                           STDOUT_FILENO,
						                           (void *) &packet.buffer,
						                           (size_t) packet.header.size))
							goto delete_socket;
						break;
				}
				break;
		}
	} while (1);

success:
	/* report success */
	exit_code = EXIT_SUCCESS;

delete_socket:
	/* delete the socket */
	(void) unlink(argv[1]);

close_socket:
	/* close the socket */
	(void) close(output_socket);

end:
	return exit_code;
}
