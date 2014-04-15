#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <dirent.h>
#include <limits.h>
#include <liblazy/common.h>
#include <liblazy/mime.h>
#include <liblazy/http.h>

/* the server name */
#define SERVER_NAME "httpclient"

/* the server banner */
#define SERVER_BANNER SERVER_NAME"/1.0"

extern const http_request_identifier_t g_http_request_types[];

extern const http_response_identifier_t g_http_response_types[];

http_response_type_t _receive_request(unsigned char *raw_request,
                                      const size_t size,
                                      http_request_t *request) {
	/* the return value */
	http_response_type_t result;

	/* the raw request size */
	ssize_t request_size;

	/* receive the request */
	request_size = read(STDIN_FILENO, raw_request, size);
	switch (request_size) {
		case (-1):
		case 0:
			result = HTTP_RESPONSE_BAD_REQUEST;
			goto end;

		default:
			if (size == (size_t) request_size) {
				result = HTTP_RESPONSE_REQUEST_TOO_LARGE;
				goto end;
			}
	}

	/* terminate the request */
	raw_request[request_size] = '\0';

	/* parse the request headers */
	result = http_headers_parse(request, (char *) raw_request);
	if (HTTP_RESPONSE_OK != result)
		goto end;

	/* ignore anything but GET requests */
	if (HTTP_REQUEST_TYPE_GET != request->type) {
		result = HTTP_RESPONSE_METHOD_NOT_ALLOWED;
		goto end;
	}

	/* report success */
	result = HTTP_RESPONSE_OK;

end:
	return result;
}

http_response_type_t _enter_root(const char *root,
                                 const http_request_t *request,
                                 const char *default_host,
                                 const char **host) {
	/* the return value */
	http_response_type_t result = HTTP_RESPONSE_REQUEST_TOO_LARGE;

	/* the host root */
	char host_root[PATH_MAX];

	/* the host root absolute path */
	char absolute_root[PATH_MAX];

	/* get the requested host */
	*host = http_request_get_header(request, "Host");
	if (NULL == *host)
		*host = default_host;

	/* obtain the host root directory */
	if (sizeof(host_root) <= snprintf((char *) &host_root,
	                                  sizeof(host_root),
	                                  "%s/%s",
	                                  root,
	                                  *host))
		goto end;

	/* get host root directory's absolute path */
	if (NULL == realpath((char *) &host_root, (char *) &absolute_root)) {
		result = HTTP_RESPONSE_NOT_FOUND;
		goto end;
	}

	/* make sure the host root directory path is indeed an absolute path -
	 * otherwise, it could be a path traversal attempt*/
	if (0 != strcmp((const char *) &absolute_root, (const char *) &host_root)) {
		result = HTTP_RESPONSE_FORBIDDEN;
		goto end;
	}

	/* change the server root directory */
	if (-1 == chroot((const char *) &host_root)) {
		result = HTTP_RESPONSE_INTERNAL_ERROR;
		goto end;
	}
	if (-1 == chdir("/")) {
		result = HTTP_RESPONSE_INTERNAL_ERROR;
		goto end;
	}

	/* report success */
	result = HTTP_RESPONSE_OK;

end:
	return result;
}

http_response_type_t _open_file(const http_request_t *request,
                                off_t *size,
                                int *fd,
                                const char **mime_type) {
	/* the return value */
	http_response_type_t result = HTTP_RESPONSE_INTERNAL_ERROR;

	/* the host root directory */
	DIR *root;

	/* a file under the diretcory */
	struct dirent _entry;
	struct dirent *entry;

	/* the requested file path */
	const char *path;

	/* the requested file attributes */
	struct stat attributes;

	/* if no file was requested, locate the index page */
	if (0 != strcmp("/", request->url))
		path = request->url;
	else {
		/* until the index is found, unset the URL */
		path = NULL;

		/* open the root directory */
		root = opendir(".");
		if (NULL == root)
			goto end;

		do {
			/* read the name of one file */
			if (0 != readdir_r(root, &_entry, &entry))
				goto close_root;
			if (NULL == entry)
				break;

			/* if the file name begins with "index", use it */
			if (0 == strncmp((const char *) &entry->d_name,
			                 "index",
			                 sizeof("index") - sizeof(char))) {
				path = (const char *) &entry->d_name;
				break;
			}
		} while (1);

close_root:
		/* close the root directory */
		(void) closedir(root);

		/* if the index file was not found, report failure */
		if (NULL == path)
			goto end;
	}

	/* get the file attributes */
	if (-1 == stat(path, &attributes)) {
		result = HTTP_RESPONSE_NOT_FOUND;
		goto end;
	}

	/* make sure the file is a regular file */
	if (!(S_ISREG(attributes.st_mode))) {
		result = HTTP_RESPONSE_NOT_FOUND;
		goto end;
	}

	/* open the file for reading; upon failure, to not disclose why it could not
	 * be opened */
	*fd = open(path, O_RDONLY);
	if (-1 == *fd) {
		result = HTTP_RESPONSE_NOT_FOUND;
		goto end;
	}

	/* return the file size and type */
	*size = attributes.st_size;
	*mime_type = mime_type_guess(path);

	/* report success */
	result = HTTP_RESPONSE_OK;

end:
	return result;
}

http_response_type_t _add_common_headers(http_response_t *response,
                                         char *now_textual,
                                         const char *mime_type) {
	/* the return value */
	http_response_type_t result = HTTP_RESPONSE_INTERNAL_ERROR;

	/* the current time */
	time_t now;

	/* the local time */
	struct tm local_time;

	/* get the local time */
	(void) time(&now);
	if (NULL == gmtime_r(&now, &local_time))
		goto end;
	if (NULL == asctime_r(&local_time, now_textual))
		goto end;
	now_textual[strlen(now_textual) - 1] = '\0';

	/* add all common headers */
	if (false == http_header_add(&response->headers,
	                             &response->headers_count,
	                             "Date",
	                             now_textual))
		goto end;
	if (false == http_header_add(&response->headers,
	                             &response->headers_count,
	                             "Server",
	                             SERVER_BANNER))
		goto end;
	if (false == http_header_add(&response->headers,
	                             &response->headers_count,
	                             "Content-Type",
	                             mime_type))
		goto end;
	if (false == http_header_add(&response->headers,
	                             &response->headers_count,
	                             "Connection",
	                             "close"))
		goto end;

	/* report success */
	result = HTTP_RESPONSE_OK;

end:
	return result;
}

void _log_request(http_request_t *request,
                  http_response_t *response,
                  const char *host) {
	/* the client user agent */
	const char *user_agent;

	/* the referer */
	const char *referer;

	/* get the client name and version */
	user_agent = http_request_get_header(request, "User-Agent");
	if (NULL == user_agent)
		user_agent = "";

	/* get the referer URL */
	referer = http_request_get_header(request, "Referer");
	if (NULL == referer)
		referer = "";

	/* log the request to the system log */
	syslog(LOG_INFO,
	       "%s%s%s [%s] (%s) -> %s",
	       g_http_request_types[request->type].text,
	       host,
	       request->url,
	       referer,
	       user_agent,
	       g_http_response_types[response->type].text);
}


int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the raw request */
	unsigned char raw_request[HTTP_MAX_REQUEST_SIZE];

	/* the request */
	http_request_t request;

	/* the response */
	http_response_t response;

	/* the host */
	const char *host;

	/* the sent file */
	int fd = -1;

	/* the file size */
	off_t content_size;

	/* the file type */
	const char *mime_type;

	/* the file  size, in textual form */
	char content_size_textual[STRLEN("18446744073709551615")];

	/* the current time, in textual form */
	char now[26];

	/* make sure the right number of command-line arguments was passed */
	if (3 != argc)
		goto end;

	/* open the system log */
	openlog(SERVER_NAME, LOG_NDELAY | LOG_PID, LOG_USER);

	/* initialize the response headers */
	response.headers = NULL;
	response.headers_count = 0;

	/* initialize the host */
	host = "";

	/* receive the request */
	request.headers = NULL;
	request.headers_count = 0;
	response.type = _receive_request((unsigned char *) &raw_request,
	                                 sizeof(raw_request),
	                                 &request);
	if (HTTP_RESPONSE_OK != response.type)
		goto log_request;

	/* change the server root directory */
	response.type = _enter_root(argv[1], &request, argv[2], &host);
	if (HTTP_RESPONSE_OK != response.type)
		goto send_response;

	/* open the sent file */
	response.type = _open_file(&request, &content_size, &fd, &mime_type);
	if (HTTP_RESPONSE_OK != response.type)
		goto send_response;

	/* convert the file size to textual form */
	if (sizeof(content_size_textual) <= snprintf((char *) &content_size_textual,
	                                             sizeof(content_size_textual),
	                                             "%u",
	                                             (unsigned int) content_size)) {
		response.type = HTTP_RESPONSE_INTERNAL_ERROR;
		goto send_response;
	}

	/* add the content size header */
	if (false == http_header_add(&response.headers,
	                             &response.headers_count,
	                             "Content-Length",
	                             (const char *) &content_size_textual))
		goto close_file;

send_response:
	/* add all common headers */
	if (HTTP_RESPONSE_OK != _add_common_headers(&response,
	                                            (char *) &now,
	                                            mime_type)) {
		response.type = HTTP_RESPONSE_INTERNAL_ERROR;
		goto close_file;
	}

	/* send the response */
	if (true == http_response_send(stdout, &response, fd, content_size))
		exit_code = EXIT_SUCCESS;

close_file:
	/* close the file */
	if (-1 != fd)
		(void) close(fd);

log_request:
	/* if the headers were successfully parsed, log the request */
	if (0 < request.headers_count)
		_log_request(&request, &response, host);

	/* free the response headers */
	if (NULL != response.headers)
		free(response.headers);

	/* free the request headers */
	free(request.headers);

	/* close the system log */
	closelog();

end:
	return exit_code;
}
