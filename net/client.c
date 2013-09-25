#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <regex.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <liblazy/io.h>

/* the relaying timeout, in seconds */
#define RELAYING_TIMEOUT (60)

/* the relaying buffer size, in bytes */
#define RELAYING_BUFFER_SIZE (64 * 1024)

/* the environment variable containing the blacklist */
#define BLACKLIST_ENVIRONMENT_VARIABLE "TRAFFIC_BLACKLIST"

/* the environment variable containing the minimum size of a relayd chunk, in
 * bytes */
#define MINIMUM_CHUNK_SIZE_ENVIRONMENT_VARIABLE ("MIN_CHUNK_SIZE")

/* the default minimum chunk size, in bytes */
#define FALLBACK_MINIMUM_CHUNK_SIZE (0)

/* the application name in the system log */
#define LOG_IDENTITY "client"

/* a blacklist expression */
typedef struct {
	char *pattern;
	regex_t compiled;
	bool is_compiled;
} blacklist_expression_t;

/* a blacklist of expressions which must not be part of the session traffic */
typedef struct {
	blacklist_expression_t *items;
	unsigned int count;
	char *environment_variable;
	ssize_t minimum_chunk_size;
} blacklist_t;

/* a peer in the session */
typedef struct {
	int input;
	int output;
} peer_t;

bool _should_drop_client(unsigned char *buffer,
                         ssize_t buffer_size,
                         const blacklist_t *blacklist) {
	/* the return value */
	bool should_drop = false;

	/* the last byte in the buffer*/
	unsigned char last_byte;

	/* a loop index */
	unsigned int i;

	/* if the chunk is too small, drop the connection - it could be a tiny
	 * fragmentation attack */
	if (blacklist->minimum_chunk_size > buffer_size) {
		should_drop = true;
		goto end;
	}

	/* if the blacklist is empty, do nothing */
	if (0 == blacklist->count)
		goto end;

	/* if the buffer contains a NULL byte, the traffic is binary, so it cannot
	 * be analyzed */
	if (NULL != memchr(buffer, '\0', (size_t) buffer_size))
		goto end;

	/* otherwise, treat the traffic as textual - replace the last byte with a
	 * NULL one, to terminate the buffer */
	last_byte = buffer[buffer_size - 1];
	buffer[buffer_size - 1] = '\0';

	/* match the buffer against all expressions */
	for (i = 0; blacklist->count > i; ++i) {
		if (0 == regexec(&blacklist->items[i].compiled,
		                 (const char *) buffer,
		                 (size_t) buffer_size,
		                 NULL,
		                 0)) {
			should_drop = true;
			break;
		}
	}

	/* revert the last byte back to its original value */
	buffer[buffer_size - 1] = last_byte;

end:
	return should_drop;
}

void _log_offending_client(const peer_t *peer) {
	/* the client address */
	struct sockaddr_storage client_address;

	/* the client address size */
	socklen_t client_address_size;

	/* the client address, in textual form */
	char client_address_textual[INET6_ADDRSTRLEN];

	/* get the client address */
	client_address_size = sizeof(client_address);
	if (-1 == getpeername(peer->input,
	                      (struct sockaddr *) &client_address,
	                      &client_address_size))
		goto end;

	/* convert the client address to textual form */
	switch (client_address.ss_family) {
		case AF_INET6:
			if (NULL == inet_ntop(
			            AF_INET6,
			            &(((struct sockaddr_in6 *) &client_address)->sin6_addr),
			            (char *) &client_address_textual,
			            sizeof(client_address_textual)))
				goto end;
			break;

		case AF_INET:
			if (NULL == inet_ntop(
			              AF_INET,
			              &(((struct sockaddr_in *) &client_address)->sin_addr),
			              (char *) &client_address_textual,
			              sizeof(client_address_textual)))
				goto end;
			break;

		default:
			goto end;
	}

	/* open the system log */
	openlog(LOG_IDENTITY, LOG_NDELAY | LOG_PID, LOG_USER);

	/* log the client address */
	syslog(LOG_WARNING,
	       "dropped a connection from %s",
	       (char *) &client_address_textual);

	/* close the system log */
	closelog();

end:
	;
}

bool _relay(unsigned char *buffer,
            size_t buffer_size,
            const peer_t *source,
            const peer_t *destination,
            const blacklist_t *blacklist) {
	/* the return value */
	bool is_success = false;

	/* received chunk size */
	ssize_t chunk_size;

	do {
		/* receive a chunk */
		chunk_size = read(source->output, buffer, buffer_size);
		switch (chunk_size) {
			case (-1):
				if (EAGAIN == errno)
					goto success;
				goto end;

			/* if the peer has disconnected, stop */
			case (0):
				goto end;

			default:
				/* if the buffer contains a blacklisted expression, log the
				 * client details and stop */
				if (true == _should_drop_client(buffer,
				                                chunk_size,
				                                blacklist)) {
					_log_offending_client(source);
					goto end;
				}

				/* send the chunk to the other socket */
				if (chunk_size != write(destination->input,
				                        buffer,
				                        (size_t) chunk_size))
					goto end;
				break;
		}
	} while (1);

success:
	/* report success */
	is_success = true;

end:
	return is_success;
}

bool _compile_blacklist(blacklist_t *blacklist) {
	/* the return value */
	bool is_success = false;

	/* a loop index */
	unsigned int i;

	/* compile all regular expressions */
	for (i = 0; blacklist->count > i; ++i) {
		if (0 == regcomp(&blacklist->items[i].compiled,
		                 blacklist->items[i].pattern,
		                 REG_EXTENDED | REG_NEWLINE | REG_NOSUB))
			blacklist->items[i].is_compiled = true;
		else
			goto end;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool _get_blacklist(blacklist_t *blacklist) {
	/* the return value */
	bool is_success = false;

	/* the allocated array */
	blacklist_expression_t *array;

	/* strtok_r()'s position within the environment variable value */
	char *position;

	/* a blacklist expression */
	char *expression;

	/* the minimum chunk size */
	char *minimum_chunk_size;

	/* initialize the blacklist size */
	blacklist->count = 0;

	/* get the environment variable value */
	blacklist->environment_variable = getenv(BLACKLIST_ENVIRONMENT_VARIABLE);
	if (NULL == blacklist->environment_variable)
		goto get_minimum_size;

	/* allocate a writable string */
	blacklist->environment_variable = strdup(blacklist->environment_variable);
	if (NULL == blacklist->environment_variable)
		goto end;

	/* split the string */
	expression = strtok_r(blacklist->environment_variable, ",", &position);
	if (NULL == expression)
		goto free_environment_variable;

	/* allocate an array which holds the split elements */
	blacklist->items = NULL;
	do {
		++blacklist->count;
		array = realloc(blacklist->items,
		                sizeof(blacklist_expression_t) * blacklist->count);
		if (NULL == array)
			goto free_blacklist;
		blacklist->items = array;
		blacklist->items[blacklist->count - 1].pattern = expression;
		blacklist->items[blacklist->count - 1].is_compiled = false;
		expression = strtok_r(NULL, ",", &position);
	} while (NULL != expression);

get_minimum_size:
	/* get the minimum chunk size and convert it to an integer; if it's
	 * non-numeric, atoi() should return 0 */
	minimum_chunk_size = getenv(MINIMUM_CHUNK_SIZE_ENVIRONMENT_VARIABLE);
	if (NULL == minimum_chunk_size)
		blacklist->minimum_chunk_size = FALLBACK_MINIMUM_CHUNK_SIZE;
	else
		blacklist->minimum_chunk_size = atoi(minimum_chunk_size);

	/* report success */
	is_success = true;
	goto end;

free_blacklist:
	/* free the allocated array */
	free(blacklist->items);

free_environment_variable:
	/* free the allocated string */
	free(blacklist->environment_variable);

end:
	return is_success;
}

void _free_blacklist(blacklist_t *blacklist) {
	/* a loop index */
	unsigned int i;

	if (0 != blacklist->count) {
		/* free all compiled regular expressions */
		for (i = 0; blacklist->count > i; ++i) {
			if (true == blacklist->items[i].is_compiled)
				regfree(&blacklist->items[i].compiled);
		}

		/* free the expressions array */
		free(blacklist->items);
	}

	/* free the environment variable value */
	if (NULL != blacklist->environment_variable)
		free(blacklist->environment_variable);
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a socket */
	peer_t relay_socket;

	/* the connected peer */
	peer_t peer;

	/* resolving hints */
	struct addrinfo hints;

	/* the server address */
	struct addrinfo *server_address;

	/* a signal mask */
	sigset_t signal_mask;

	/* a received signal */
	siginfo_t received_signal;

	/* the relaying timeout */
	struct timespec timeout;

	/* a buffer */
	unsigned char *buffer;

	/* the blacklist */
	blacklist_t blacklist;

	/* make sure the number of command-line arguments is valid */
	if (3 != argc)
		goto end;

	/* resolve the server address */
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (0 != getaddrinfo(argv[1], argv[2], &hints, &server_address))
		goto end;

	/* allocate a buffer */
	buffer = malloc(RELAYING_BUFFER_SIZE);
	if (NULL == buffer)
		goto free_server_address;

	/* parse the blacklist */
	if (false == _get_blacklist(&blacklist))
		goto free_buffer;

	/* compile the blacklist expressions */
	if (false == _compile_blacklist(&blacklist))
		goto free_blacklist;

	/* create a socket */
	relay_socket.input = socket(server_address->ai_family,
	                            server_address->ai_socktype,
	                            server_address->ai_protocol);
	if (-1 == relay_socket.input)
		goto free_blacklist;
	relay_socket.output = relay_socket.input;

	/* block SIGIO and SIGTERM signals */
	if (-1 == sigemptyset(&signal_mask))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGIO))
		goto close_socket;
	if (-1 == sigaddset(&signal_mask, SIGTERM))
		goto close_socket;
	if (-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL))
		goto close_socket;

	/* connect to the server */
	if (-1 == connect(relay_socket.input,
	                  server_address->ai_addr,
	                  server_address->ai_addrlen))
		goto close_socket;

	/* enable asynchronous I/O */
	if (false == file_enable_async_io(STDIN_FILENO))
		goto close_socket;
	if (false == file_enable_async_io(relay_socket.input))
		goto close_socket;

	peer.input = STDIN_FILENO;
	peer.output = STDOUT_FILENO;
	do {
		/* relay the received data; since it's impossible to know which socket
		 * cause the SIGIO signal to be sent, try to relay in both directions */
		if (false == _relay(buffer,
		                    RELAYING_BUFFER_SIZE,
		                    &peer,
		                    &relay_socket,
		                    &blacklist))
			break;
		if (false == _relay(buffer,
		                    RELAYING_BUFFER_SIZE,
		                    &relay_socket,
		                    &peer,
		                    &blacklist))
			break;

		/* wait until a signal is received */
		timeout.tv_sec = RELAYING_TIMEOUT;
		timeout.tv_nsec = 0;
		if (-1 == sigtimedwait(&signal_mask, &received_signal, &timeout)) {
			if (EAGAIN == errno)
				break;
			else
				goto close_socket;
		}

		/* if the received signal is a termination one, stop */
		if (SIGTERM == received_signal.si_signo)
			break;
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_socket:
	/* close the relay socket */
	(void) close(relay_socket.input);

free_blacklist:
	/* free the blacklist */
	_free_blacklist(&blacklist);

free_buffer:
	/* free the buffer */
	free(buffer);

free_server_address:
	/* free the server address */
	freeaddrinfo(server_address);

end:
	return exit_code;
}
