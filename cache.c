#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fnmatch.h>
#include <sys/utsname.h>
#include <stdio.h>

#include "find.h"
#include "module.h"
#include "cache.h"

static bool _append_alias(const char *module,
                          const char *alias,
                          cache_entry_t *entry) {
	/* the enlarged entry */
	cache_entry_t new_entry = {{0}};

	/* the return value */
	bool result = false;

	assert(NULL != module);
	assert(NULL != alias);
	assert(NULL != entry);

	/* enlarge the aliases array */
	new_entry.count = 1 + entry->count;
	new_entry.aliases = realloc(entry->aliases,
	                            sizeof(char *) * new_entry.count);
	if (NULL == new_entry.aliases) {
		free(entry->aliases);
		entry->count = 0;
		goto end;
	}

	/* append the alias to the array */
	new_entry.aliases[entry->count] = strdup(alias);
	if (NULL != new_entry.aliases[entry->count]) {
		result = true;
	}
	entry->aliases = new_entry.aliases;
	entry->count = new_entry.count;

end:
	return result;
}

static bool _append_module(const char *path, cache_t *cache) {
	/* the module */
	module_t module = {{0}};

	/* the enlarged cache */
	cache_t new_cache = {0};

	/* the return value */
	bool result = false;

	assert(NULL != path);
	assert(NULL != cache);

	/* open the module */
	if (false == module_open(&module, path)) {
		goto end;
	}

	/* enlarge the cache */
	new_cache.count = 1 + cache->count;
	new_cache.entries = realloc(cache->entries,
	                            sizeof(cache_entry_t) * new_cache.count);
	if (NULL == new_cache.entries) {
		goto close_module;
	}

	/* cache the module */
	new_cache.entries[cache->count].aliases = NULL;
	new_cache.entries[cache->count].count = 0;
	result = module_for_each_alias(&module,
                                   (alias_callback_t) _append_alias,
                                   &new_cache.entries[cache->count]);

	/* cache the module name and path */
	(void) strcpy(new_cache.entries[cache->count].path, path);
	(void) strcpy(new_cache.entries[cache->count].name, module.name);

	/* copy over the new entries array */
	(void) memcpy(cache, &new_cache, sizeof(new_cache));

close_module:
	/* close the module */
	module_close(&module);

end:
	return result;
}

bool cache_generate(cache_t *cache) {
	/* the kernel modules directory path */
	char path[PATH_MAX] = {'\0'};

	/* kernel information */
	struct utsname kernel = {{0}};

	/* obtain the kernel release */
	if (-1 == uname(&kernel)) {
		return true;
	}

	/* obtain the kernel modules directory path */
	if (sizeof(path) <= snprintf(path,
	                             sizeof(path),
	                             KERNEL_MODULES_DIRECTORY"/%s",
	                             kernel.release)) {
		return false;
	}

	/* cache all modules metadata */
	if (false == find_all(path,
	                      "*.ko",
	                      (file_callback_t) _append_module,
	                      cache)) {
		if (NULL != cache->entries) {
			cache_free(cache);
		}
		return false;
	}

	return true;
}

void cache_free(cache_t *cache) {
	/* loop indices */
	unsigned int i = 0;
	unsigned int j = 0;

	assert(NULL != cache);
	assert(NULL != cache->entries);
	assert(0 < cache->count);

	/* free all aliases */
	for ( ; cache->count > i; ++i) {
		if (0 == cache->entries[i].count) {
			continue;
		}
		for (j = 0; cache->entries[i].count > j; ++j) {
			free(cache->entries[i].aliases[j]);
		}
		free(cache->entries[i].aliases);
	}

	/* free the entries array */
	free(cache->entries);
}

static bool _module_cmp(const char *a, const char *b) {
	/* a loop index */
	size_t i = 0;

	/* the first name length */
	size_t length = 0;

	assert(NULL != a);
	assert(NULL != b);
	assert('\0' != a[0]);

	/* make sure the both names are of the same length */
	length = strlen(a);
	if (strlen(b) != length) {
		return false;
	}

	/* compare the module names */
	for ( ; length > i; ++i) {
		switch (a[i]) {

			/* allow both hyphens and underscores */
			case '-':
			case '_':
				if (('-' == b[i]) || ('_' == b[i])) {
					break;
				}
				return false;

			default:
				if (a[i] != b[i]) {
					return false;
				}
		}
	}

	return true;
}

cache_entry_t *cache_find_module(cache_t *cache, const char *name) {
	/* a loop index */
	unsigned int i = 0;

	/* find the hash in the cache */
	for ( ; cache->count > i; ++i) {
		if (true == _module_cmp(name, cache->entries[i].name)) {
			return &cache->entries[i];
		}
	}

	return NULL;
}

cache_entry_t *cache_find_alias(cache_t *cache, const char *alias) {
	/* loop indices */
	unsigned int i = 0;
	unsigned int j = 0;

	/* module name */
	const char *name = NULL;

	for ( ; cache->count > i; ++i) {
		for (j = 0; cache->entries[i].count > j; ++j) {
			if (0 == fnmatch(cache->entries[i].aliases[j], alias, 0)) {
				return &cache->entries[i];
			}
		}
	}

	/* upon failure, try to load the module by name */
	name = strchr(alias, ':');
	if (NULL != name) {
		++name;
		if ('\0' != name[0]) {
			return cache_find_module(cache, name);
		}
	}

	return NULL;
}
