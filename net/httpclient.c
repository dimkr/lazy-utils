#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <liblazy/common.h>
#include <liblazy/http.h>

/* the server name */
#define SERVER_NAME "httpclient"

/* the server banner */
#define SERVER_BANNER SERVER_NAME"/1.0"

/* the index page */
#define INDEX_PAGE "/index.html"

extern const http_request_identifier_t g_request_types[];

extern const http_response_identifier_t g_response_types[];

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

	/* the client user agent */
	const char *user_agent;

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

	/* open the system log */
	openlog(SERVER_NAME, LOG_NDELAY | LOG_PID, LOG_USER);

	/* change the server root directory */
	if (-1 == chroot(argv[1])) {
		response.type = HTTP_RESPONSE_FORBIDDEN;
		goto send_response;
	}

	/* if no file was requested, return the index page */
	if (0 == strcmp("/", request.url))
		request.url = INDEX_PAGE;

	/* get the file attributes */
	if (-1 == stat(request.url, &attributes)) {
		response.type = HTTP_RESPONSE_NOT_FOUND;
		goto send_response;
	}

	/* make sure the file is a regular file */
	if (!(S_ISREG(attributes.st_mode))) {
		response.type = HTTP_RESPONSE_NOT_FOUND;
		goto send_response;
	}

	/* convert the file size to textual form */
	if (sizeof(content_size) <= snprintf((char *) &content_size,
	                                     sizeof(content_size),
	                                     "%u",
	                                     (unsigned int) attributes.st_size)) {
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
	if (true == http_response_send(stdout, &response, fd, attributes.st_size))
		exit_code = EXIT_SUCCESS;

	/* get the client name and version */
	user_agent = http_request_get_header(&request, "User-Agent");
	if (NULL == user_agent)
		user_agent = "unknown";

	/* log the request to the system log */
	syslog(LOG_INFO,
	       "%s%s (%s) -> %s",
	       g_request_types[request.type].text,
	       request.url,
	       user_agent,
	       g_response_types[request.type].text);

close_file:
	/* close the file */
	if (-1 != fd)
		(void) close(fd);

	/* close the system log */
	closelog();

	/* free the response headers */
	if (NULL != response.headers)
		free(response.headers);

	/* free the request headers */
	free(request.headers);

end:
	return exit_code;
}
