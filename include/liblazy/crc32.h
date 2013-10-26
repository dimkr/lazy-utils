#ifndef _CRC32_H_INCLUDED
#	define _CRC32_H_INCLUDED

typedef unsigned long crc32_t;

crc32_t crc32_hash(const unsigned char *buffer, size_t size);

#endif