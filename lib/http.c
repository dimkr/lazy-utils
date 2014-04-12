#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <liblazy/common.h>
#include <liblazy/http.h>

static const http_request_identifier_t g_request_types[] = {
	{ "GET ", STRLEN("GET "), HTTP_REQUEST_TYPE_GET },
	{ "POST ", STRLEN("POST "), HTTP_REQUEST_TYPE_POST }
};

static const http_response_identifier_t g_response_types[] = {
	{ 200, "200 OK" },
	{ 400, "400 Bad Request" },
	{ 403, "403 Forbidden" },
	{ 404, "404 Not Found" },
	{ 405, "405 Method Not Allowed" },
	{ 413, "413 Request Entity Too Large" },
	{ 500, "500 Internal Server Error" }
};

bool http_header_add(http_header_t **headers,
                     unsigned int *count,
                     const char *name,
                     const char *value) {
	/* the return value */
	bool is_success = false;

	/* the enlarged array of headers */
	http_header_t *more_headers;

	/* enlarge the headers array */
	more_headers = realloc(*headers,
	                       sizeof(http_request_t) * (1 + (*count)));
	if (NULL == more_headers)
		goto end;
		
	/* add the new header to the array */
	more_headers[*count].name = name;
	more_headers[*count].value = value;
	*headers = more_headers;
	++(*count);

	/* report success */
	is_success = true;
	
end:
	return is_success;
}

http_response_type_t http_headers_parse(http_request_t *request,
                                        char *raw_request) {
	/* the return value */
	http_response_type_t response;
	
	/* the current position inside the raw request */
	char *position;
	
	/* a single, raw header */
	const char *header;
	
	/* a header value */
	char *value;

	/* a loop index */
	unsigned int i;

	/* locate the URL, according to the request type */
	for (i = 0; ARRAY_SIZE(g_request_types) > i; ++i) {
		if (0 == strncmp(raw_request,
		                 g_request_types[i].text,
		                 g_request_types[i].length)) {
			request->url = (const char *) raw_request;
			request->url += g_request_types[i].length;
			request->type = g_request_types[i].type;
			goto parse_headers;
		}
	}
	response = HTTP_RESPONSE_METHOD_NOT_ALLOWED;
	goto end;

parse_headers:
	/* start splitting the raw request; skip the first line */
	header = strtok_r(raw_request, "\r\n", &position);
	if (NULL == header) {
		response = HTTP_RESPONSE_BAD_REQUEST;
		goto end;
	}

	/* initialize the headers array */
	request->headers = NULL;
	request->headers_count = 0;

	do {
		/* continue to the next header */
		header = strtok_r(NULL, "\r\n", &position);
		if (NULL == header)
			break;

		/* skip empty lines */
		if (0 == strlen(header))
			continue;

		/* locate the separator between the header name and its value */
		value = strstr(header, ": ");
		if (NULL == value) {
			response = HTTP_RESPONSE_BAD_REQUEST;
			goto free_memory;
		}
		
		/* replace the separator with a null byte and skip it */
		value[0] = '\0';
		value += STRLEN(": ");
		
		/* add the header to the headers array */
		if (false == http_header_add(&request->headers,
		                             &request->headers_count,
		                             header,
		                             value)) {
			response = HTTP_RESPONSE_INTERNAL_ERROR;
			goto free_memory;
		}
	} while (1);

	/* locate the space after the URL and replace it with a null byte */
	position = strchr(request->url, ' ');
	if (NULL == position) {
		response = HTTP_RESPONSE_INTERNAL_ERROR;
		goto free_memory;
	}
	position[0] = '\0';

	/* report success */
	response = HTTP_RESPONSE_OK;
	goto end;
	
free_memory:
	/* free all allocated memory */
	if (NULL != request->headers)
		free(request->headers);
	
end:
	return response;
}

bool http_response_send(FILE *stream,
                        http_response_t *response,
                        const int fd,
                        const size_t size) {
	/* the return value */
	bool is_success = false;
	
	/* a loop index */
	unsigned int i;

	/* a file offset */
	off_t offset;
	
	/* the output file descriptor */
	int output_fd;
	
	/* a socket option value */
	int value;
	
	/* a flag which indicates whether the output stream is a socket */
	bool is_socket;
	
	/* get the output stream file descriptor */
	output_fd = fileno(stream);
	
	/* enable the TCP_CORK socket option, to make send() more efficient */
	value = 1;
	is_socket = true;
	if (-1 == setsockopt(output_fd, IPPROTO_TCP, TCP_CORK, &value, sizeof(value))) {
		if (ENOTSOCK != errno)
			goto end;
		is_socket = false;
	}
		
	/* send the status code and the date */
	if (0 > fprintf(stream,
	                "HTTP/1.1 %s\r\n",
	                g_response_types[response->type].text))
		goto end;
		
	/* send all headers */
	for (i = 0; response->headers_count > i; ++i) {
		if (0 > fprintf(stream,
		                "%s: %s\r\n",
		                response->headers[i].name,
		                response->headers[i].value))
			goto end;
	}

	/* flush the stream */
	if (0 != fflush(stream))
		goto end;

	/* if there is content to send, add an additional line break */
	if (-1 != fd) {
		if (STRLEN("\r\n") != write(output_fd,
		                            "\r\n",
		                            sizeof(char) * STRLEN("\r\n")))
			goto end;
			
		/* then, send the content */
		offset = 0;
		if (size != sendfile(output_fd, fd, &offset, size))
			goto end;
	}

	/* disable the TCP_CORK socket option */
	if (true == is_socket) {
		value = 0;
		if (-1 == setsockopt(output_fd,
		                     IPPROTO_TCP,
		                     TCP_CORK,
		                     &value,
		                     sizeof(value)))
			goto end;
	}
	
	/* report success */
	is_success = true;

end:
	return is_success;
}

const char *http_request_get_header(const http_request_t *request,
                                    const char *name) {
	/* the return value */
	const char *value = NULL;
	
	/* a loop index */
	unsigned int i;
	
	for (i = 0; request->headers_count > i; ++i) {
		if (0 == strcmp(name, request->headers[i].name)) {
			value = request->headers[i].value;
			break;
		}
	}
	
	return value;
}
