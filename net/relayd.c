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
#include <syslog.h>
#include <errno.h>
#include <liblazy/io.h>
#include <liblazy/daemon.h>

/* the maximum number of clients */
#define CLIENTS_LIMIT (32)

/* the client handler */
#define CLIENT_HANDLER "client"

/* the application name in the system log */
#define LOG_IDENTITY "relayd"

/* a semaphore used to limit the number of clients served at once */
sem_t g_client_semaphore;

/* the maximum interval to wait once the client limit is reached, in seconds */
#define QUEUE_WAITING_TIMEOUT (30)

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

	/* waiting timeout */
	struct timespec timeout;

	/* make sure the number of command-line arguments is valid */
	if (4 != argc)
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

	/* add SIGIO, SIGTERM and SIGCHLD to the signal mask */
	if (-1 == sigemptyset(&signal_mask))
		goto close_system_log;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto close_system_log;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto close_system_log;
	if (-1 == sigaddset(&signal_mask, SIGCHLD))
		goto close_system_log;

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
		syslog(LOG_INFO, "waiting for a client");
		if (-1 == sigwaitinfo(&signal_mask, &received_signal))
			goto close_socket;

		switch (received_signal.si_signo) {
			case SIGCHLD:
				/* destroy the zombie process */
				if (received_signal.si_pid != waitpid(received_signal.si_pid,
				                                      NULL,
				                                      WNOHANG))
					goto close_socket;

				/* unlock the semaphore */
				if (-1 == sem_post(&g_client_semaphore))
					goto close_socket;

				/* wait for the next signal */
				continue;

			/* if the received signal is a termination one, stop */
			case SIGTERM:
				goto success;
		}

		/* lock the semaphore; signals are blocked and therefore not handled,
		 * but it's impossible to use sigprocmask() to unblock them, so we use
		 * sem_timedwait() instead of sem_wait() - i.e of SIGTERM was sent, it
		 * will be handled once the timeout expires */
		timeout.tv_sec = QUEUE_WAITING_TIMEOUT;
		timeout.tv_nsec = 0;
		if (-1 == sem_timedwait(&g_client_semaphore, &timeout)) {
			if (ETIMEDOUT == errno)
				continue;
			else
				goto close_socket;
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

				/* destroy the semaphore, for the same reason */
				(void) sem_destroy(&g_client_semaphore);

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
				(void) execlp(CLIENT_HANDLER,
				              CLIENT_HANDLER,
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

destroy_semaphore:
	/* destroy the semaphore */
	(void) sem_destroy(&g_client_semaphore);

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
