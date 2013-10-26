#include <unistd.h>
#include <liblazy/cache.h>

bool cache_open(cache_file_t *cache, const char *path) {
	/* the return value */
	bool is_success = false;

	/* map the cache file to memory */
	if (false == file_read(&cache->file, path))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool cache_search(cache_file_t *cache,
                  const uint8_t type,
                  const cache_callback_t callback,
                  void *parameter) {
	/* the return value */
	bool is_found = false;

	/* the corrent position within the cache file */
	unsigned char *position;

	/* a cache entry header */
	cache_entry_header_t *header;

	position = cache->file.contents;
	do {
		header = (cache_entry_header_t *) position;
		position += sizeof(cache_entry_header_t);
		if (header->type == type) {
			if (true == callback(header, position, parameter)) {
				is_found = true;
				break;
			}
		}
		 position += header->size;
	} while (cache->file.size > (position - cache->file.contents));

	return is_found;
}

typedef struct {
	crc32_t hash;
	unsigned char **value;
	size_t *size;
} _cache_fetch_params_t;

bool _get_first_match(const cache_entry_header_t *entry,
                      const unsigned char *value,
                      _cache_fetch_params_t *parameters) {
	if (parameters->hash == entry->hash) {
		*(parameters->value) = (unsigned char *) entry + sizeof(*entry);
		if (NULL != parameters->size)
			*(parameters->size) = entry->size;
		return true;
	}

	return false;
}

bool cache_file_get_by_hash(cache_file_t *cache,
                            const uint8_t type,
                            const crc32_t hash,
                            unsigned char **value,
                            size_t *size) {
	_cache_fetch_params_t parameters;

	parameters.value = value;
	parameters.size = size;
	parameters.hash = hash;
	return cache_search(cache,
	                    type,
	                    (cache_callback_t) _get_first_match,
	                    &parameters);

#if 0
	/* the return value */
	bool is_found = false;

	/* the corrent position within the cache file */
	unsigned char *position;

	/* a cache entry header */
	cache_entry_header_t *header;

	/* check each entry's hash until a match is found */
	position = cache->file.contents;
	do {
		header = (cache_entry_header_t *) position;
		position += sizeof(cache_entry_header_t);
		if (type == header->type) {
			if (hash == header->hash) {
				*value = position;
				if (NULL != size)
					*size = header->size;
				is_found = true;
				break;
			}
		}
		position += header->size;
	} while (cache->file.size > (position - cache->file.contents));

	return is_found;
#endif
}

bool cache_file_add(const int fd,
                    const uint8_t type,
                    const unsigned char *key,
                    const size_t key_size,
                    const unsigned char *value,
                    const size_t value_size) {
	/* the return value */
	bool is_success = false;

	/* the cache entry header */
	cache_entry_header_t header;

	/* write the cache entry header */
	header.type = type;
	header.hash = crc32_hash(key, key_size);
	header.size = value_size;
	if (sizeof(header) != write(fd, &header, sizeof(header)))
		goto end;

	/* write the value */
	if ((ssize_t) value_size != write(fd, value, value_size))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

void cache_close(cache_file_t *cache) {
	/* close the cache file */
	file_close(&cache->file);
}
