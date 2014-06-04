#ifndef _MODULE_H_INCLUDED
#	define _MODULE_H_INCLUDED

#	include <stdbool.h>
#	include <sys/stat.h>

typedef struct {
	struct stat attributes;
	int fd;
	unsigned char *contents;
	char *path;
	char *name;
} module_t;

typedef bool (*dependency_callback_t)(const char *name, void *arg);

typedef bool (*alias_callback_t)(const char *module,
                                 const char *alias,
                                 void *arg);

bool module_open(module_t *module, const char *path);
void module_close(module_t *module);

bool module_load(module_t *module);

bool module_for_each_dependency(module_t *module,
                                const dependency_callback_t callback,
                                void *arg);

bool module_for_each_alias(module_t *module,
                           const alias_callback_t callback,
                           void *arg);

#endif
