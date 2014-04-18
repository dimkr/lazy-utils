#ifndef _HTTP_H_INCLUDED
#	define _HTTP_H_INCLUDED

#	include <stdbool.h>
#	include <stdint.h>
#	include <stdio.h>

#	define HTTP_MAX_REQUEST_SIZE (2048)

enum http_request_types {
	HTTP_REQUEST_TYPE_GET,
	HTTP_REQUEST_TYPE_POST
} ;

typedef unsigned int http_request_type_t;

typedef enum {
	HTTP_RESPONSE_OK,
	HTTP_RESPONSE_BAD_REQUEST,
	HTTP_RESPONSE_FORBIDDEN,
	HTTP_RESPONSE_NOT_FOUND,
	HTTP_RESPONSE_METHOD_NOT_ALLOWED,
	HTTP_RESPONSE_REQUEST_TOO_LARGE,
	HTTP_RESPONSE_INTERNAL_ERROR
} http_response_type_t;

typedef struct {
	uint16_t code;
	const char *text;
} http_response_identifier_t;

typedef struct {
	const char *text;
	unsigned int length;
} http_request_identifier_t;

typedef struct {
	const char *name;
	const char *value;
} http_header_t;

typedef struct {
	http_header_t *headers;
	unsigned int headers_count;
	const char *url;
	http_request_type_t type;
} http_request_t;

typedef struct {
	http_header_t *headers;
	unsigned int headers_count;
	http_response_type_t type;
} http_response_t;

bool http_header_add(http_header_t **headers,
                     unsigned int *count,
                     const char *name,
                     const char *value);

const char *http_request_get_header(const http_request_t *request,
                                    const char *name);

http_response_type_t http_headers_parse(http_request_t *request,
                                        char *raw_request);

bool http_response_send(FILE *stream,
                        http_response_t *response,
                        const int fd,
                        const size_t size);

void http_request_log(const char *identity,
                      const http_request_t *request,
                      const http_response_t *response);

#endif