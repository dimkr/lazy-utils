#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>

/* the maximum number of clients */
#define CLIENTS_LIMIT (32)

/* the default client handler */
#define DEFAULT_CLIENT_HANDLER "client"

/* the application name in the system log */
#define LOG_IDENTITY "relayd"

void _signal_handler(int type, siginfo_t *info, void *context) {
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
	siginfo_t received_signal;

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

	/* the signal which indicates there is a pending client */
	int io_signal;

	/* a signal action */
	struct sigaction signal_action;

	/* the handler */
	const char *handler;

	/* parse the command-line */
	switch (argc) {
		case 4:
			handler = DEFAULT_CLIENT_HANDLER;
			break;

		case 5:
			handler = argv[4];
			break;

		default:
			goto end;
	}

	/* assign a signal handler for SIGCHLD, which destroys zombie processes */
	signal_action.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_NOCLDSTOP | SA_NODEFER;
	signal_action.sa_sigaction = _signal_handler;
	if (-1 == sigemptyset(&signal_action.sa_mask))
		goto end;
	if (-1 == sigaction(SIGCHLD, &signal_action, NULL))
		goto end;

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_USER);

	/* write a log message which indicates when the daemon started running */
	syslog(LOG_INFO,
	       "relayd has started (%s -> %s:%s)",
	       argv[1],
	       argv[2],
	       argv[3]);

	/* resolve the local address */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (0 != getaddrinfo(NULL, argv[1], &hints, &local_address))
		goto close_system_log;

	/* pick the minimum real-time signal */
	io_signal = SIGRTMIN;

	/* add io_signal, SIGTERM and SIGCHLD to the signal mask */
	if (-1 == sigemptyset(&signal_mask))
		goto free_local_address;
	if (-1 == sigaddset(&signal_mask, io_signal))
		goto free_local_address;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto free_local_address;
	if (-1 == sigaddset(&signal_mask, SIGCHLD))
		goto free_local_address;

	/* create a socket */
	listening_socket = socket(local_address->ai_family,
	                      local_address->ai_socktype,
	                      local_address->ai_protocol);
	if (-1 == listening_socket)
		goto free_local_address;

	/* block io_signal and SIGTERM signals */
	if (-1 == sigprocmask(SIG_SETMASK, &signal_mask, NULL))
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
	if (false == file_enable_async_io(listening_socket, io_signal))
		goto close_socket;

	do {
		/* wait until a signal is received */
		syslog(LOG_INFO, "waiting for a client");
		if (-1 == sigwaitinfo(&signal_mask, &received_signal))
			goto close_socket;

		/* if the received signal is a termination one, stop */
		switch (received_signal.si_signo) {
			case SIGCHLD:
				continue;

			case SIGTERM:
				goto success;
		}

		/* accept a client */
		client_address_size = sizeof(client_address);
		client_socket = accept(listening_socket,
		                       (struct sockaddr *) &client_address,
		                       &client_address_size);
		if (-1 == client_socket)
			goto close_socket;

		syslog(LOG_INFO, "accepted a client");

		/* create a child process */
		pid = fork();
		switch (pid) {
			case (-1):
				goto close_socket;

			case (0):
				/* close the listening socket, since the handler process doesn't
				 * need it */
				(void) close(listening_socket);

				/* free the listening address */
				freeaddrinfo(local_address);

				/* close the system log */
				closelog();

				/* close the standard input file descriptor */
				if (-1 == close(STDIN_FILENO))
					goto disconnect_client;

				/* attach the socket to the standard input pipe file
				 * descriptor */
				if (STDIN_FILENO != dup(client_socket))
					goto disconnect_client;

				/* get rid of the redundant file descriptor */
				(void) close(client_socket);

				/* close the standard output file descriptor */
				if (-1 == close(STDOUT_FILENO))
					goto terminate_child;

				/* attach the socket to the standard output pipe file
				 * descriptor, too */
				if (STDOUT_FILENO != dup(STDIN_FILENO))
					goto terminate_child;

				/* serve the client, by relaying its traffic */
				(void) execlp(handler,
				              handler,
				              argv[2],
				              argv[3],
				              (char *) NULL);
				goto terminate_child;

disconnect_client:
				/* disconnect the client */
				(void) close(client_socket);

terminate_child:
				/* terminate the process */
				exit(EXIT_SUCCESS);

				break;

			default:
				/* close the client socket - the child process owns a copy */
				(void) close(client_socket);

				break;
		}
	} while (1);

success:
	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the listening socket */
	(void) close(listening_socket);

free_local_address:
	/* free the local address */
	freeaddrinfo(local_address);

close_system_log:
	/* write another log message, which indicates when the daemon stopped
	 * running */
	syslog(LOG_INFO, "relayd has terminated");

	/* close the system log */
	closelog();

end:
	return exit_code;
}
