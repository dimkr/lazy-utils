#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <liblazy/common.h>
#include <liblazy/http.h>

/* the server banner */
#define SERVER_BANNER "httpclient/1.0"

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;
	
	/* the request */
	http_request_t request;
	
	/* the raw request */
	unsigned char raw_request[HTTP_MAX_REQUEST_SIZE];

	/* the raw request size */
	ssize_t request_size;

	/* the request host */
	const char *host;
	
	/* the response */
	http_response_t response;
	
	/* the current time */
	time_t now;

	/* the local time */
	struct tm local_time;
	
	/* the local time, in textual form */
	char now_textual[26];

	/* the sent file */
	int fd = -1;
	
	/* the file attributes */
	struct stat attributes;
	
	/* the content size, in textual form */
	char content_size[STRLEN("18446744073709551615")];

	/* initialize the response headers */
	response.headers = NULL;
	response.headers_count = 0;
	
	/* make sure the right number of command-line arguments was passed */
	if (3 != argc) {
		response.type = HTTP_RESPONSE_INTERNAL_ERROR;
		goto end;
	}

	/* receive the request */
	request_size = read(STDIN_FILENO, (void *) &raw_request, sizeof(raw_request));
	switch (request_size) {
		case 0:
		case (-1):
			goto end;

		case sizeof(request_size):
			response.type = HTTP_RESPONSE_REQUEST_TOO_LARGE;
			goto send_response;
	}
	
	/* terminate the request */
	raw_request[request_size] = '\0';

	/* parse the request headers */
	response.type = http_headers_parse(&request, (char *) &raw_request);
	if (HTTP_RESPONSE_OK != response.type)
		goto send_response;

	/* ignore anything but GET requests */
	if (HTTP_REQUEST_TYPE_GET != request.type) {
		response.type = HTTP_RESPONSE_METHOD_NOT_ALLOWED;
		goto send_response;
	}

	/* make sure the request host is correct */
	host = http_request_get_header(&request, "Host");
	if (NULL != host) {
		if (0 != strcmp(argv[2], host)) {
			response.type = HTTP_RESPONSE_NOT_FOUND;
			goto send_response;
		}
	}

	/* change the server root directory */
	if (-1 == chroot(argv[1])) {
		response.type = HTTP_RESPONSE_FORBIDDEN;
		goto end;
	}
 
	/* get the file attributes */
	if (-1 == stat(request.url, &attributes)) {
		response.type = HTTP_RESPONSE_NOT_FOUND;
		goto send_response;
	}

	/* convert the file size to textual form */
	if (sizeof(content_size) <= snprintf((char *) &content_size,
	                                     sizeof(content_size),
	                                     "%zu",
	                                     attributes.st_size)) {
		response.type = HTTP_RESPONSE_INTERNAL_ERROR;
		goto send_response;
	}

	/* open the file for reading */
	fd = open(request.url, O_RDONLY);
	if (-1 == fd) {
		response.type = HTTP_RESPONSE_NOT_FOUND;
		goto send_response;
	}

	/* add the response headers */
	if (false == http_header_add(&response.headers,
	                             &response.headers_count,
	                             "Content-Length",
	                             (const char *) &content_size))
		goto close_file;

send_response:
	/* get the local time */
	(void) time(&now);
	if (NULL == gmtime_r(&now, &local_time)) {
		response.type = HTTP_RESPONSE_INTERNAL_ERROR;
		goto send_response;
	}
	if (NULL == asctime_r(&local_time, (char *) &now_textual)) {
		response.type = HTTP_RESPONSE_INTERNAL_ERROR;
		goto send_response;
	}
	now_textual[strlen((const char *) &now_textual) - 1] = '\0';

	if (false == http_header_add(&response.headers,
	                             &response.headers_count,
	                             "Date",
	                             (const char *) &now_textual))
		goto close_file;
	if (false == http_header_add(&response.headers,
	                             &response.headers_count,
	                             "Server",
	                             SERVER_BANNER))
		goto close_file;
	if (false == http_header_add(&response.headers,
	                             &response.headers_count,
	                             "Connection",
	                             "close"))
		goto close_file;
		
	/* send the response */
	if (false == http_response_send(stdout, &response, fd, attributes.st_size))
		goto close_file;

	/* report success */
	exit_code = EXIT_SUCCESS;

close_file:
	/* close the file */
	if (-1 != fd)
		(void) close(fd);
		
end:
	return exit_code;
}
