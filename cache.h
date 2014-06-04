#ifndef _CACHE_H_INCLUDED
#	define _CACHE_H_INCLUDED

#	include <stdbool.h>
#	include <limits.h>

#	define KERNEL_MODULES_DIRECTORY "/lib/modules"

typedef struct {
	char path[PATH_MAX];
	char name[NAME_MAX];
	char **aliases;
	unsigned int count;
} cache_entry_t;

typedef struct {
	cache_entry_t *entries;
	unsigned int count;
} cache_t;

bool cache_generate(cache_t *cache);
void cache_free(cache_t *cache);

cache_entry_t *cache_find_module(cache_t *cache, const char *name);
cache_entry_t *cache_find_alias(cache_t *cache, const char *alias);

#endif
