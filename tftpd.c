/* written using http://www.faqs.org/rfcs/rfc1350.html and
 * https://en.wikipedia.org/wiki/Trivial_File_Transfer_Protocol */

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <netinet/in.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <assert.h>

#include "common.h"
#include "daemon.h"

/* the usage message */
#define USAGE "Usage: tftpd\n"

/* the listening port */
#define LISTENING_PORT "69"

/* the server root */
#define SERVER_ROOT "/srv/tftp"

/* the minimum size of a request */
#define MIN_REQUEST_SIZE (sizeof(uint16_t) + \
                          sizeof("a") + \
                          sizeof("octet"))

/* the maximum size of a request */
#define MAX_REQUEST_SIZE (sizeof(uint16_t) + \
                          (1 + NAME_MAX) + \
                          sizeof("netascii"))

/* socket timeout, in seconds */
#define SOCKET_TIMEOUT (5)

/* the lowest possible TID */
#define MIN_TID (6996)

/* the maximum number of clients connected at once */
#define MAX_CLIENTS (4)

/* a random port */
#define RANDOM_PORT htons(MIN_TID + (rand() % MAX_CLIENTS))

/* the maximum number of random ports checked for each client */
#define MAX_TID_ATTEMPTS (2)

/* packet types (opcodes) */
enum packet_types {
	PACKET_TYPE_RRQ   = 1,
	PACKET_TYPE_WRQ   = 2,
	PACKET_TYPE_DATA  = 3,
	PACKET_TYPE_ACK   = 4,
	PACKET_TYPE_ERROR = 5
};

/* a request packet header */
typedef struct __attribute__((packed)) {
	uint16_t opcode;
	char file_name[1];
} request_header_t;

/* a request packet */
typedef union {
	request_header_t header;
	unsigned char data[MAX_REQUEST_SIZE];
} request_t;

/* a data packet header */
typedef struct __attribute__((packed)) {
	uint16_t opcode;
	uint16_t id;
} data_header_t;

/* a data packet */
typedef struct __attribute__((packed)) {
	data_header_t header;
	unsigned char data[512];
} data_t;

/* an acknowledgment packet */
typedef struct __attribute__((packed)) {
	uint16_t opcode;
	uint16_t id;
} ack_t;

/* a pre-generated error packet */
typedef struct {
	const unsigned char *packet;
	const unsigned int size;
} error_packet_t;

/* error types */
enum errors {
	ERROR_FILE_NOT_FOUND    = 1,
	ERROR_ACCESS_VIOLATION  = 2,
	ERROR_DISK_FULL         = 3,
	ERROR_ILLEGAL_OPERATION = 4,
	ERROR_UNKNOWN_TID       = 5,
	ERROR_FILE_EXISTS       = 6
};

static const unsigned char g_file_not_found[] = {
	0, PACKET_TYPE_ERROR, 0, ERROR_FILE_NOT_FOUND, 'F', 'i', 'l', 'e', ' ', 'n',
	'o', 't',' ' , 'f', 'o', 'u', 'n', 'd', '\0'
};

static const unsigned char g_access_violation[] = {
	 0, PACKET_TYPE_ERROR, 0, ERROR_ACCESS_VIOLATION, 'A', 'c', 'c', 'e', 's',
	 's', ' ', 'V', 'i', 'o', 'l', 'a', 't', 'i', 'o', 'n', '\0'
};

static const unsigned char g_disk_full[] = {
	0, PACKET_TYPE_ERROR, 0, ERROR_DISK_FULL, 'D', 'i', 's', 'k', ' ', 'f', 'u',
	'l', 'l', ' ', 'o', 'r', ' ', 'a', 'l', 'l', 'o', 'c', 'a', 't', 'i', 'o',
	'n', ' ', 'e', 'x', 'c', 'e', 'e', 'd', 'e', 'd', '\0'
};

static const unsigned char g_illegal_operation[] = {
	0, PACKET_TYPE_ERROR, 0, ERROR_ILLEGAL_OPERATION, 'I', 'l', 'l', 'e', 'g',
	'a', 'l', ' ', 'T', 'F', 'T', 'P', ' ', 'o', 'p', 'e', 'r', 'a', 't', 'i',
	'o', 'n', '\0'
};

static const unsigned char g_unknown_tid[] = {
	0, PACKET_TYPE_ERROR, 0, ERROR_UNKNOWN_TID, 'U', 'n', 'k', 'n', 'o', 'w',
	'n', ' ', 't', 'r', 'a', 'n', 's', 'f', 'e', 'r', ' ', 'I', 'D', '\0'
};

static const unsigned char g_file_exists[] = {
	0, PACKET_TYPE_ERROR, 0, ERROR_FILE_EXISTS, 'F', 'i', 'l', 'e', ' ', 'a',
	'l', 'r', 'e', 'a', 'd', 'y', ' ', 'e', 'x', 'i', 's', 't', 's', '\0'
};

static const error_packet_t g_errors[] = {
	{ g_file_not_found, sizeof(g_file_not_found) },
	{ g_access_violation, sizeof(g_access_violation) },
	{ g_disk_full, sizeof(g_disk_full) },
	{ g_illegal_operation, sizeof(g_illegal_operation) },
	{ g_unknown_tid, sizeof(g_unknown_tid) },
	{ g_file_exists, sizeof(g_file_exists) }
};

static bool _set_timeout(const int s) {
	static const struct timeval timeout = { SOCKET_TIMEOUT, 0 };

	if (-1 == setsockopt(s,
	                     SOL_SOCKET,
	                     SO_RCVTIMEO,
	                     (const void *) &timeout,
	                     (socklen_t) sizeof(timeout))) {
		return false;
	}

	if (-1 == setsockopt(s,
	                     SOL_SOCKET,
	                     SO_SNDTIMEO,
	                     (const void *) &timeout,
	                     (socklen_t) sizeof(timeout))) {
		return false;
	}

	return true;
}

static void _send_error(const int s,
                        const unsigned int type,
                        const struct sockaddr *client_address,
                        const socklen_t address_size) {
	unsigned int index = (type - 1);

	assert(STDERR_FILENO < s);
	assert(NULL != client_address);
	assert(0 < address_size);

	(void) sendto(s,
	              (const void *) g_errors[index].packet,
	              g_errors[index].size,
	              0,
	              client_address,
	              address_size);
}

static void _log_request(const char *type,
                         const char *path,
                         const struct sockaddr *address,
                         const socklen_t size) {
	/* the client address, in textual form */
	char textual[1 + INET6_ADDRSTRLEN] = {'\0'};

	assert(NULL != type);
	assert(NULL != path);
	assert(NULL != address);
	assert(0 < size);

	/* convert the address to textual form */
	switch (address->sa_family) {
		case AF_INET:
			if (NULL == inet_ntop(
			       AF_INET,
			       (const void *) &(((struct sockaddr_in *) address)->sin_addr),
			       textual,
			       size)) {
				return;
			}
			break;

		case AF_INET6:
			if (NULL == inet_ntop(
			     AF_INET6,
			     (const void *) &(((struct sockaddr_in6 *) address)->sin6_addr),
			     textual,
			     size)) {
				return;
			}
			break;

		default:
			return;
	}

	/* log the request */
	syslog(LOG_INFO, "%s %s (%s)", type, path, textual);
}

static bool _handle_wrq(const int s,
                        const char *path,
                        const struct sockaddr *address,
                        const socklen_t address_size,
                        const short tid) {
	/* a data chunk */
	data_t chunk = {{0}};

	/* the address the chunk was received from */
	struct sockaddr chunk_source = {0};

	/* the chunk size */
	ssize_t size = 0;

	/* the source address size */
	socklen_t source_address_size = 0;

	/* the output file */
	int file = (-1);

	/* an acknowledgment packet */
	ack_t ack = {0};

	/* the chunk transaction ID */
	short chunk_tid = 0;

	/* the return value */
	bool result = false;

	assert(STDERR_FILENO < s);
	assert(NULL != path);
	assert(NULL != address);
	assert(0 < address_size);
	assert(0 != tid);

	/* create the file */
	file = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (-1 == file) {
		switch (errno) {
			case EEXIST:
				_send_error(s, ERROR_FILE_EXISTS, address, address_size);
				break;

			case EPERM:
			case EACCES:
				_send_error(s, ERROR_ACCESS_VIOLATION, address, address_size);
		}

		goto end;
	}

	/* send an acknowledgment message, to indicate the request is valid */
	ack.opcode = htons(PACKET_TYPE_ACK);
	if (sizeof(ack) != sendto(s,
	                          (const void *) &ack,
	                          sizeof(ack),
	                          0,
	                          address,
	                          address_size)) {
		goto end;
	}

	do {
		/* receive a data chunk */
		source_address_size = sizeof(chunk_source);
		size = recvfrom(s,
		                (void *) &chunk,
		                sizeof(chunk),
		                0,
		                &chunk_source,
		                &source_address_size);
		if (-1 == size) {
			goto close_file;
		}

		/* make sure the chunk is a data packet */
		if (htons(PACKET_TYPE_DATA) != chunk.header.opcode) {
			goto close_file;
		}

		/* make sure the chunk was sent with the right transaction ID */
		switch (chunk_source.sa_family) {
			case AF_INET:
				chunk_tid = ((struct sockaddr_in *) &chunk_source)->sin_port;
				break;

			case AF_INET6:
				chunk_tid = ((struct sockaddr_in6 *) &chunk_source)->sin6_port;
				break;
		}
		if (tid != chunk_tid) {
			_send_error(s, ERROR_UNKNOWN_TID, address, address_size);
			goto close_file;
		}

		/* write the chunk to the file */
		size -= sizeof(chunk.header);
		if (size != write(file, (const void *) &chunk.data, (size_t) size)) {
			if (ENOSPC == errno) {
				_send_error(s, ERROR_DISK_FULL, address, address_size);
			}
			goto close_file;
		}

		/* send an acknowledgment message, to indicate the chunk was received */
		ack.id = chunk.header.id;
		if (sizeof(ack) != sendto(s,
		                          (const void *) &ack,
		                          sizeof(ack),
		                          0,
		                          address,
		                          address_size)) {
			goto close_file;
		}

	/* stop once a small or empty chunk is received */
	} while (sizeof(chunk.data) == size);

	/* report success */
	result = true;

close_file:
	/* close the file */
	(void) close(file);

	/* upon failure, also delete it */
	if (false == result) {
		(void) unlink(path);
	}

end:
	return result;
}

static bool _handle_rrq(const int s,
                        const char *path,
                        const struct sockaddr *address,
                        const socklen_t address_size,
                        const short tid) {
	/* a data chunk */
	data_t chunk = {{0}};

	/* the address the acknowledgement was received from */
	struct sockaddr ack_source = {0};

	/* the chunk size */
	ssize_t size = 0;

	/* the source address size */
	socklen_t source_address_size = 0;

	/* the input file */
	int file = (-1);

	/* an acknowledment message */
	ack_t ack = {0};

	/* chunk ID */
	uint16_t id = 0;

	/* the acknowledgement transaction ID */
	short ack_tid = 0;

	/* the return value */
	bool result = false;

	assert(STDERR_FILENO < s);
	assert(NULL != path);
	assert(NULL != address);
	assert(0 < address_size);
	assert(0 != tid);

	/* create the file */
	file = open(path, O_RDONLY);
	if (-1 == file) {
		switch (errno) {
			case ENOENT:
				_send_error(s, ERROR_FILE_NOT_FOUND, address, address_size);
				break;

			case EPERM:
			case EACCES:
			case EROFS:
				_send_error(s, ERROR_ACCESS_VIOLATION, address, address_size);
		}

		goto end;
	}

	chunk.header.opcode = htons(PACKET_TYPE_DATA);
	do {
		/* read a data chunk */
		size = read(file, (void *) &chunk.data, sizeof(chunk.data));
		if (-1 == size) {
			goto close_file;
		}

		/* send the chunk */
		++id;
		chunk.header.id = htons(id);
		size += sizeof(chunk.header);
		if (size != sendto(s,
		                   (const void *) &chunk,
		                   size,
		                   0,
		                   address,
		                   address_size)) {
			goto end;
		}

		/* wait for an acknowledgment message */
		source_address_size = sizeof(ack_source);
		if (sizeof(ack) != recvfrom(s,
		                            (void *) &ack,
		                            sizeof(ack),
		                            0,
		                            &ack_source,
		                            &source_address_size)) {
			goto close_file;
		}

		/* make sure the acknowledgment is an acknowledgment packet */
		if (htons(PACKET_TYPE_ACK) != ack.opcode) {
			goto close_file;
		}

		/* make sure the acknowledgment message has the right chunk number */
		if (chunk.header.id != ack.id) {
			goto close_file;
		}

		/* make sure the acknowledgment message has the right transaction ID */
		switch (ack_source.sa_family) {
			case AF_INET:
				ack_tid = ((struct sockaddr_in *) &ack_source)->sin_port;
				break;

			case AF_INET6:
				ack_tid = ((struct sockaddr_in6 *) &ack_source)->sin6_port;
				break;
		}
		if (tid != ack_tid) {
			_send_error(s, ERROR_UNKNOWN_TID, address, address_size);
			goto close_file;
		}

	/* stop once a small or empty chunk is sent */
	} while (sizeof(chunk) == size);

	/* report success */
	result = true;

close_file:
	/* close the file */
	(void) close(file);

end:
	return result;
}

static bool _pick_tid(const int s,
                      const struct sockaddr *local_address,
                      const socklen_t address_size) {
	/* the number of attempts so far*/
	unsigned int attempts = 0;

	assert(STDERR_FILENO < s);
	assert(NULL != local_address);
	assert(0 < address_size);

	/* initialize the random number seed - this should be done for each client,
	 * to improve the TID randomness */
	srand((unsigned int) time(NULL));

	/* try to find a free port, with a limit of MAX_TID_ATTEMPTS attempts */
	switch (local_address->sa_family) {
		case AF_INET:
			for ( ; MAX_TID_ATTEMPTS > attempts; ++attempts) {
				((struct sockaddr_in *) local_address)->sin_port = RANDOM_PORT;
				if (0 == bind(s, local_address, address_size)) {
					return true;
				} else {
					if (EADDRINUSE != errno) {
						return false;
					}
				}
			}
			break;

		case AF_INET6:
			for ( ; MAX_TID_ATTEMPTS > attempts; ++attempts) {
				((struct sockaddr_in6 *) local_address)->sin6_port = \
				                                                    RANDOM_PORT;
				if (0 == bind(s, local_address, address_size)) {
					return true;
				} else {
					if (EADDRINUSE != errno) {
						return false;
					}
				}
			}
			break;
	}

	return false;
}

static bool _handle_request(const char *path,
                            const uint16_t opcode,
                            const struct addrinfo *local_address,
                            const struct sockaddr *client_address,
                            const socklen_t address_size) {
	/* the socket */
	int s = (-1);

	/* the source address */
	struct sockaddr source = {0};

	/* the return value */
	bool result = false;

	/* the transaction ID */
	short tid = 0;

	assert(NULL != path);
	assert((PACKET_TYPE_RRQ == opcode) || (PACKET_TYPE_WRQ == opcode));
	assert(0 < address_size);
	assert(NULL != local_address);
	assert(NULL != client_address);
	assert(0 < address_size);

	/* get the transaction ID */
	switch (client_address->sa_family) {
		case AF_INET:
			tid = ((struct sockaddr_in *) client_address)->sin_port;
			break;

		case AF_INET6:
			tid = ((struct sockaddr_in6 *) client_address)->sin6_port;
			break;

		default:
			goto end;
	}

	/* create a socket */
	s = socket(local_address->ai_family,
	           local_address->ai_socktype,
	           local_address->ai_protocol);
	if (-1 == s) {
		goto end;
	}

	/* set the socket timeout */
	if (false == _set_timeout(s)) {
		goto end;
	}

	/* pick a unique source port and bind the socket on it */
	(void) memcpy(&source, local_address->ai_addr, local_address->ai_addrlen);
	if (false == _pick_tid(s, &source, local_address->ai_addrlen)) {
		goto close_socket;
	}

	/* handle the request */
	switch (opcode) {
		case PACKET_TYPE_RRQ:
			_log_request("GET", path, client_address, address_size);
			result = _handle_rrq(s, path, client_address, address_size, tid);
			break;

		case PACKET_TYPE_WRQ:
			_log_request("PUT", path, client_address, address_size);
			result = _handle_wrq(s, path, client_address, address_size, tid);
			break;

		default:
			_send_error(s,
			            ERROR_ILLEGAL_OPERATION,
			            client_address,
			            address_size);
			goto close_socket;
	}

close_socket:
	/* close the socket */
	(void) close(s);

end:
	return result;
}

int main(int argc, char *argv[]) {
	/* a request */
	request_t request = {{0}};

	/* resolving hints */
	struct addrinfo hints = {0};

	/* the address of a client */
	struct sockaddr client_address = {0};

	/* the daemon data */
	daemon_t daemon_data = {{{0}}};

	/* the request size */
	ssize_t size = 0;

	/* the client address size */
	socklen_t address_size = 0;

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a received signal */
	int received_signal = 0;

	/* child process ID */
	pid_t pid = 0;

	/* the local address */
	struct addrinfo *address = NULL;

	/* the file path */
	const char *path = NULL;

	/* the transfer mode */
	const char *mode = NULL;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* resolve the local address */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_V4MAPPED;
	if (0 != getaddrinfo(NULL, LISTENING_PORT, &hints, &address)) {
		goto end;
	}

	/* create a socket */
	daemon_data.fd = socket(address->ai_family,
	                        address->ai_socktype,
	                        address->ai_protocol);
	if (-1 == daemon_data.fd) {
		goto free_address;
	}

	/* set the socket timeout */
	if (false == _set_timeout(daemon_data.fd)) {
		goto end;
	}

	/* bind the socket */
	if (-1 == bind(daemon_data.fd, address->ai_addr, address->ai_addrlen)) {
		goto close_socket;
	}

	/* open the system log*/
	openlog("tftpd", LOG_NDELAY, LOG_DAEMON);

	/* initialize the daemon */
	if (false == daemon_init(&daemon_data, SERVER_ROOT)) {
		goto close_log;
	}

	do {
		/* wait for a request */
		if (false == daemon_wait(&daemon_data, &received_signal)) {
			break;
		}

		/* if the received signal is a termination one, report success */
		if (SIGTERM == received_signal) {
			break;
		}

		/* receive the request */
		address_size = sizeof(client_address);
		size = recvfrom(daemon_data.fd,
		                (void *) request.data,
		                sizeof(request.data),
		                0,
		                &client_address,
		                &address_size);
		switch (size) {
			case 0:
				/* if it was nothing but a scan, ignore it */
				continue;

			case (-1):
				goto close_log;
		}

		/* make sure the request isn't too small */
		if (MIN_REQUEST_SIZE > size) {
			continue;
		}

		/* make sure the request is valid */
		request.header.opcode = ntohs(request.header.opcode);
		switch (request.header.opcode) {
			case PACKET_TYPE_RRQ:
			case PACKET_TYPE_WRQ:
				break;

			default:
				continue;
		}

		/* locate the path */
		path = (const char *) &request.data[sizeof(uint16_t)];

		/* make sure the path is null-terminated */
		mode = memchr((const void *) path, '\0', 1 + NAME_MAX);
		if (NULL == mode) {
			continue;
		}

		/* skip the null byte */
		++mode;

		/* make sure the transfer mode is "octet" */
		if (0 != strcmp("octet", mode)) {
			_send_error(daemon_data.fd,
			            ERROR_ILLEGAL_OPERATION,
			            &client_address,
			            address_size);
			continue;
		}

		/* spawn a child process */
		pid = fork();
		switch (pid) {
			case (-1):
				goto close_log;

			case 0:
				if (false == _handle_request(path,
				                             request.header.opcode,
				                             address,
				                             &client_address,
				                             address_size)) {
					goto close_log;
				}
				goto success;
		}

	} while (1);

success:
	/* report success */
	exit_code = EXIT_SUCCESS;

close_log:
	/* close the system log */
	closelog();

close_socket:
	/* close the socket */
	(void) close(daemon_data.fd);

free_address:
	/* free the local address */
	freeaddrinfo(address);

end:
	return exit_code;
}
