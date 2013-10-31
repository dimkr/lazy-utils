#ifndef _CACHE_H_INCLUDED
#	define _CACHE_H_INCLUDED

#	include <stdbool.h>
#	include <string.h>
#	include <stdint.h>
#	include <liblazy/io.h>
#	include <liblazy/crc32.h>

typedef struct __attribute__((packed)) {
	uint8_t type;
	crc32_t hash;
	uint16_t size;
} cache_entry_header_t;

typedef struct {
	file_t file;
} cache_file_t;

#define HASH_STRING(string) \
	crc32_hash((const unsigned char *) string, strlen(string))

bool cache_open(cache_file_t *cache, const char *path);
void cache_close(cache_file_t *cache);

bool cache_file_get_by_hash(cache_file_t *cache,
                            const uint8_t type,
                            const crc32_t hash,
                            unsigned char **value,
                            size_t *size);
#define cache_file_get_by_string(cache, \
                                 type, \
                                 string, \
                                 value, \
                                 value_size) \
	cache_file_get_by_hash(cache, \
	                       type, \
	                       HASH_STRING(string), \
	                       value, \
	                       value_size)

bool cache_file_add(const int fd,
                    const uint8_t type,
                    const unsigned char *key,
                    const size_t key_size,
                    const unsigned char *value,
                    const size_t value_size);

typedef bool (*cache_callback_t)(const cache_entry_header_t *entry,
                                 const unsigned char *value,
                                 void *parameter);

bool cache_search(cache_file_t *cache,
                  const uint8_t type,
                  const cache_callback_t callback,
                  void *parameter);

#endif
