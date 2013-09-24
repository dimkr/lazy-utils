#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>

/* the maximum number of clients waiting to be handled */
#define CLIENTS_QUEUE_SIZE (32)

/* the relaying timeout, in seconds */
#define RELAYING_TIMEOUT (5 * 60)

/* the log message source */
#define LOG_IDENTITY "relayd"

/* the relaying buffer size */
#define RELAYING_BUFFER_SIZE (FILE_READING_BUFFER_SIZE)

bool _relay_tick(unsigned char *buffer,
                 size_t buffer_size,
                 const int source,
                 const int destination,
                 ssize_t *counter) {
	/* the return value */
	bool is_success = false;

	/* received chunk size */
	ssize_t chunk_size;

	do {
		/* receive a chunk from the source socket */
		chunk_size = recv(source, buffer, buffer_size, 0);
		switch (chunk_size) {
			case (-1):
				if (EAGAIN == errno)
					goto success;
				goto end;

			/* if the client disconnected, stop */
			case (0):
				goto end;

			default:
				/* send the chunk to the destination socket */
				if (chunk_size != send(destination,
				                       buffer,
				                       (size_t) chunk_size,
				                       0))
					goto end;

				/* increment the relayed traffic counter */
				(*counter) += chunk_size;

				/* if this is the last chunk, stop */
				if (chunk_size < (ssize_t) buffer_size)
					goto success;

				break;
		}
	} while (1);

success:
	/* report success */
	is_success = true;

end:
	return is_success;
}

ssize_t _relay(const int first, const int second, const long timeout) {
	/* the return value */
	ssize_t counter = 0;

	/* a signal mask */
	sigset_t signal_mask;

	/* file descriptor flags */
	int flags;

	/* a received signal */
	siginfo_t received_signal;

	/* a buffer */
	unsigned char *buffer;

	/* relaying timeout */
	struct timespec timeout_structure;

	/* start blocking SIGIO and SIGTERM */
	if (-1 == sigemptyset(&signal_mask))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto end;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto end;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto end;

	/* allocate a buffer */
	buffer = malloc(RELAYING_BUFFER_SIZE);
	if (NULL == buffer)
		goto end;

	/* get the first socket flags */
	flags = fcntl(first, F_GETFL);
	if (-1 == flags)
		goto free_buffer;

	/* enable non-blocking, asynchronous I/O, for the first socket */
	if (false == file_enable_async_io(first))
		goto free_buffer;

	/* enable non-blocking, asynchronous I/O, for the second socket */
	if (false == file_enable_async_io(second))
		goto free_buffer;

	/* relay all data sent through the first socket */
	if (false == _relay_tick(buffer,
	                         RELAYING_BUFFER_SIZE,
	                         first,
	                         second,
	                         &counter))
		goto free_buffer;

	/* relay all data sent through the second socket */
	if (false == _relay_tick(buffer,
	                         RELAYING_BUFFER_SIZE,
	                         second,
	                         first,
	                         &counter))
		goto free_buffer;

	do {
		/* wait for more data to arrive, by waiting for a signal */
		timeout_structure.tv_sec = timeout;
		timeout_structure.tv_nsec = 0;
		if (-1 == sigtimedwait(&signal_mask,
		                       &received_signal,
		                       &timeout_structure))
			break;

		/* if the received signal is a termination signal, stop */
		if (SIGTERM == received_signal.si_signo)
			break;

		/* relay the received data; since it's impossible to know which socket
		 * cause the SIGIO signal to be sent, try to relay in both directions */
		if (false == _relay_tick(buffer,
		                         RELAYING_BUFFER_SIZE,
		                         first,
		                         second,
		                         &counter))
			break;
		if (false == _relay_tick(buffer,
		                         RELAYING_BUFFER_SIZE,
		                         second,
		                         first,
		                         &counter))
			break;
	} while (1);

free_buffer:
	/* free the allocated memory */
	free(buffer);

end:
	return counter;
}

void _handle_client(const int client_socket,
                    struct sockaddr_storage *client_address,
                    char *client_address_textual,
                    struct addrinfo *server_address,
                    ssize_t *bytes_relayed) {
	/* a socket connected to the server */
	int server_socket;

	/* initialize the relayed traffic counter */
	*bytes_relayed = 0;

	/* convert the client address to textual form */
	switch (client_address->ss_family) {
		case AF_INET6:
			if (NULL == inet_ntop(
			             AF_INET6,
			             &(((struct sockaddr_in6 *) client_address)->sin6_addr),
			             client_address_textual,
			             INET6_ADDRSTRLEN))
				goto end;
			break;

		case AF_INET:
			if (NULL == inet_ntop(
			               AF_INET,
			               &(((struct sockaddr_in *) client_address)->sin_addr),
			               client_address_textual,
			               INET6_ADDRSTRLEN))
				goto end;
			break;

		default:
			(void) strcpy(client_address_textual, "(invalid)");
			goto end;
	}

	/* write the client address to the log */
	syslog(LOG_INFO,
	       "accepted a connection from %s",
	       client_address_textual);

	/* create a socket */
	server_socket = socket(server_address->ai_family,
	                       server_address->ai_socktype,
	                       server_address->ai_protocol);
	if (-1 == server_socket)
		goto end;

	/* connect to the server */
	if (-1 == connect(server_socket,
					  server_address->ai_addr,
					  server_address->ai_addrlen))
		goto close_server_socket;

	/* relay the entire session */
	*bytes_relayed = _relay(client_socket, server_socket, RELAYING_TIMEOUT);

close_server_socket:
	/* close the socket */
	(void) close(server_socket);

end:
	;
}

void _clean_after_child(int type, siginfo_t *info, void *context) {
	(void) waitpid(info->si_pid, NULL, WNOHANG);
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* resolving hints */
	struct addrinfo hints;

	/* the local address */
	struct addrinfo *local_address;

	/* the server address */
	struct addrinfo *server_address;

	/* a listening socket */
	int listening_socket;

	/* a client socket */
	int client_socket;

	/* the client address */
	struct sockaddr_storage client_address;

	/* the client address size */
	socklen_t client_address_size;

	/* the client address, in textual form */
	char client_address_textual[INET6_ADDRSTRLEN];

	/* the number of bytes relayed from the client */
	ssize_t bytes_relayed = 0;

	/* a process ID */
	pid_t pid;

	/* the parent process ID */
	pid_t parent_pid;

	/* one */
	int one;

	/* a signal action */
	struct sigaction signal_action;

	/* a signal mask */
	sigset_t signal_mask;

	/* a received signal */
	int received_signal;

	/* make sure the number of command-line arguments is valid */
	if (4 != argc)
		goto end;

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_KERN);

	/* write a log message which indicates when the daemon started running */
	syslog(LOG_INFO, "relayd has started");

	/* get the process ID, so child processes can compare their own PID to it
	 * later */
	parent_pid = getpid();

	/* assign a signal handler for SIGCHLD, which destroys child processes */
	signal_action.sa_flags = SA_SIGINFO;
	signal_action.sa_sigaction = _clean_after_child;
	if (-1 == sigemptyset(&signal_action.sa_mask))
		goto close_system_log;
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL))
		goto close_system_log;

	/* resolve the local address */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (0 != getaddrinfo(NULL, argv[1], &hints, &local_address))
		goto close_system_log;

	/* resolve the server address */
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (0 != getaddrinfo(argv[2], argv[3], &hints, &server_address))
		goto free_local_address;

	/* create a socket */
	listening_socket = socket(local_address->ai_family,
	                          local_address->ai_socktype,
	                          local_address->ai_protocol);
	if (-1 == listening_socket)
		goto free_server_address;

	/* make it possible to listen on the same address again */
	one = 1;
	if (-1 == setsockopt(listening_socket,
	                     SOL_SOCKET,
	                     SO_REUSEADDR,
	                     &one,
	                     sizeof(one)))
		goto close_listening_socket;

	/* bind the socket */
	if (-1 == bind(listening_socket,
	               local_address->ai_addr,
	               local_address->ai_addrlen))
		goto close_listening_socket;

	/* start listening */
	if (-1 == listen(listening_socket, CLIENTS_QUEUE_SIZE))
		goto close_listening_socket;

	/* daemonize */
	if (false == daemonize())
		goto close_listening_socket;

	/* block SIGIO and SIGTERM signals */
	if (-1 == sigemptyset(&signal_mask))
		goto close_listening_socket;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto close_listening_socket;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto close_listening_socket;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto close_listening_socket;

	/* enable asynchronous I/O */
	if (false == file_enable_async_io(listening_socket))
		goto close_system_log;

	/* get the new process ID */
	parent_pid = getpid();

	do {
		/* wait until a signal is received */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto close_listening_socket;

		/* if the received signal is a termination one, stop */
		if (SIGTERM == received_signal)
			break;

		/* accept a client; upon failure, try again */
		client_address_size = sizeof(client_address);
		client_socket = accept(listening_socket,
		                       (struct sockaddr *) &client_address,
		                       &client_address_size);
		if (-1 == client_socket)
			continue;

		/* create a child process; the signal handler will take care of zombie
		 * process cleanup */
		pid = fork();
		switch (pid) {
			case (-1):
				goto close_listening_socket;

			case (0):
				/* serve the client */
				_handle_client(client_socket,
				               &client_address,
				               (char *) &client_address_textual,
				               server_address,
				               &bytes_relayed);

				/* write a summary to the system log */
				syslog(LOG_INFO,
				       "relayed %d bytes from %s to %s:%s",
				       (int) bytes_relayed,
				       (char *) &client_address_textual,
				       argv[2],
				       argv[3]);

				/* disconnect the client */
				(void) close(client_socket);

				/* report success, always */
				exit_code = EXIT_SUCCESS;

				goto close_listening_socket;

			default:
				/* close the client socket */
				(void) close(client_socket);
				break;
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_listening_socket:
	/* close the listening socket */
	(void) close(listening_socket);

free_server_address:
	/* free the server address */
	freeaddrinfo(server_address);

free_local_address:
	/* free the local address */
	freeaddrinfo(local_address);

close_system_log:
	/* in the parent process, write a log message which indicates when the
	 * daemon terminated */
	if (parent_pid == getpid())
		syslog(LOG_INFO, "relayd has terminated");

	/* close the system log */
	closelog();

end:
	return exit_code;
}
