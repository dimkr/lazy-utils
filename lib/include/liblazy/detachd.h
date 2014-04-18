#ifndef _DETACHD_H_INCLUDED
#	define _DETACHD_H_INCLUDED

#	include <liblazy/io.h>

enum packet_types {
	PACKET_TYPE_OUTPUT_CHUNK = 0,
	PACKET_TYPE_CHILD_EXIT   = 1
};

typedef struct __attribute__((packed)) {
	unsigned int size;
	unsigned char type;
} packet_header_t;

typedef struct __attribute__((packed)) {
	packet_header_t header;
	unsigned char buffer[FILE_READING_BUFFER_SIZE];
} output_packet_t;

#endif
