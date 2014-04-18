#ifndef _KMODULE_H_INCLUDED
#	define _KMODULE_H_INCLUDED

#	include <stdio.h>
#	include <stdbool.h>
#	include <limits.h>
#	include <liblazy/common.h>
#	include <liblazy/io.h>
#	include <liblazy/cache.h>

#	define KERNEL_MODULE_DIRECTORY "/lib/modules"

#	define KERNEL_MODULE_CACHE_PATH "/var/run/kmodule.cache"

#	define KERNEL_MODULE_FILE_NAME_EXTENSION "ko"

#	define LOADED_KERNEL_MODULES_LIST_PATH "/proc/modules"

#	define LOADED_KERNEL_MODULES_LIST_MAX_ENTRY_SIZE (1024)

#	define KERNEL_MODULE_BLACKLIST_PATH "/etc/kmodule.blacklist"

enum kernel_module_cache_types {
	CACHE_TYPE_KERNEL_MODULE_PATH,
	CACHE_TYPE_KERNEL_MODULE_ALIASES,
	CACHE_TYPE_KERNEL_MODULE_BLACKLIST
};

typedef struct {
	FILE *loaded_modules;
	cache_file_t cache;
} kmodule_loader_t;

#	define KERNEL_MODULE_DEPENDENCIES_FIELD "depends"
#	define KERNEL_MODULE_ALIAS_FIELD "alias"

const static char *g_kmodule_fields[] = {
	KERNEL_MODULE_DEPENDENCIES_FIELD,
	KERNEL_MODULE_ALIAS_FIELD,
	"author",
	"description",
	"filename",
	"firmware",
	"intree",
	"license",
	"srcversion",
	"vermagic",
	"version",
	"parm"
};

typedef struct {
	char **values;
	unsigned int count;
} kmodule_field_t;

typedef struct {
	cache_entry_header_t *cache_entry;
	char name[NAME_MAX];
	char path[PATH_MAX];
	crc32_t hash;
	file_t file;
	kmodule_field_t _fields[ARRAY_SIZE(g_kmodule_fields)];
} kmodule_t;

#	define mod_depends	_fields[0]
#	define mod_alias	_fields[1]

bool kmodule_loader_init(kmodule_loader_t *loader);
void kmodule_loader_destroy(kmodule_loader_t *loader);

bool kmodule_open_module_by_path(kmodule_t *module, const char *path);
bool kmodule_open_module_by_name(kmodule_loader_t *loader,
                                 kmodule_t *module,
                                 const char *name);

void kmodule_close(kmodule_t *module);

bool kmodule_get_info(kmodule_t *module);

bool kmodule_load_by_name(kmodule_loader_t *loader,
                          const char *name,
                          const char *parameters,
                          bool with_dependencies);
bool kmodule_load_by_alias(kmodule_loader_t *loader,
                           const char *alias,
                           const char *parameters,
                           bool with_dependencies);

#endif
