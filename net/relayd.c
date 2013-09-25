#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>

/* the maximum number of clients */
#define CLIENTS_LIMIT (2)

/* the client handler */
#define CLIENT_HANDLER "client"

/* a semaphore used to limit the number of clients served at once */
sem_t g_client_semaphore;

void _destroy_zombie_process(int type, siginfo_t *info, void *context) {
	/* destroy the zombie process */
	(void) waitpid(info->si_pid, NULL, WNOHANG);

	/* unlock the semaphore */
	if (-1 == sem_post(&g_client_semaphore))
		(void) kill(getpid(), SIGTERM);
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a socket */
	int listening_socket;

	/* resolving hints */
	struct addrinfo hints;

	/* the local address */
	struct addrinfo *local_address;

	/* a signal mask */
	sigset_t signal_mask;

	/* a received signal */
	int received_signal;

	/* a socket connected to a client */
	int client_socket;

	/* client address */
	struct sockaddr_storage client_address;

	/* the client address size */
	socklen_t client_address_size;

	/* the process ID */
	pid_t pid;

	/* one */
	int one;

	/* a signal action */
	struct sigaction signal_action;

	/* make sure the number of command-line arguments is valid */
	if (4 != argc)
		goto end;

	/* resolve the local address */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (0 != getaddrinfo(NULL, argv[1], &hints, &local_address))
		goto end;

	/* assign a signal handler for SIGCHLD, which destroys zombie child
	 * processes */
	signal_action.sa_flags = SA_SIGINFO | SA_RESTART;
	signal_action.sa_sigaction = _destroy_zombie_process;
	if (-1 == sigemptyset(&signal_action.sa_mask))
		goto free_local_address;
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL))
		goto free_local_address;

	/* initialize the client semaphore */
	if (-1 == sem_init(&g_client_semaphore, 0, CLIENTS_LIMIT))
		goto free_local_address;

	/* create a socket */
	listening_socket = socket(local_address->ai_family,
	                      local_address->ai_socktype,
	                      local_address->ai_protocol);
	if (-1 == listening_socket)
		goto destroy_semaphore;

	/* block SIGIO and SIGTERM signals */
	if (-1 == sigemptyset(&signal_mask))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto close_socket;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto close_socket;

	/* make it possible to listen on the same address again */
	one = 1;
	if (-1 == setsockopt(listening_socket,
	                     SOL_SOCKET,
	                     SO_REUSEADDR,
	                     &one,
	                     sizeof(one)))
		goto close_socket;

	/* bind the socket */
	if (-1 == bind(listening_socket,
	               local_address->ai_addr,
	               local_address->ai_addrlen))
		goto close_socket;

	/* start listening */
	if (-1 == listen(listening_socket, CLIENTS_LIMIT))
		goto close_socket;

	/* daemonize */
	if (false == daemonize())
		goto close_socket;

	/* enable asynchronous I/O */
	if (false == file_enable_async_io(listening_socket))
		goto close_socket;

	do {
		/* wait until a signal is received */
		if (0 != sigwait(&signal_mask, &received_signal))
			goto close_socket;

		/* if the received signal is a termination one, stop */
		if (SIGTERM == received_signal)
			break;

		/* lock the semaphore */
		if (-1 == sem_wait(&g_client_semaphore))
			goto close_socket;

		/* accept a client */
		client_address_size = sizeof(client_address);
		client_socket = accept(listening_socket,
		                       (struct sockaddr *) &client_address,
		                       &client_address_size);
		if (-1 == client_socket)
			goto close_socket;

		/* create a child process */
		pid = fork();
		switch (pid) {
			case (-1):
				goto close_socket;

			case (0):
				/* close the standard input file descriptor */
				if (-1 == close(STDIN_FILENO))
					goto disconnect_client;

				/* attach the socket to the standard input pipe file
				 * descriptor */
				if (STDIN_FILENO != dup(client_socket))
					goto disconnect_client;

				/* close the standard output file descriptor */
				if (-1 == close(STDOUT_FILENO))
					goto disconnect_client;

				/* attach the socket to the standard output pipe file
				 * descriptor, too */
				if (STDOUT_FILENO != dup(client_socket))
					goto disconnect_client;

				/* serve the client, by relaying its traffic */
				(void) execlp(CLIENT_HANDLER,
				              CLIENT_HANDLER,
				              argv[2],
				              argv[3],
				              (char *) NULL);

disconnect_client:
				/* disconnect the client */
				(void) close(client_socket);

				/* terminate the process */
				exit(EXIT_SUCCESS);

				break;

			default:
				/* close the client socket */
				(void) close(client_socket);

				break;
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the listening socket */
	(void) close(listening_socket);

destroy_semaphore:
	/* destroy the semaphore */
	(void) sem_destroy(&g_client_semaphore);

free_local_address:
	/* free the local address */
	freeaddrinfo(local_address);

end:
	return exit_code;
}
