#ifndef _KMODULE_H_INCLUDED
#	define _KMODULE_H_INCLUDED

#	include <stdbool.h>
#	include <sys/types.h>
#	include <limits.h>
#	include <liblazy/common.h>
#	include <liblazy/io.h>

#	define KMODULE_LIST_PATH "/lib/modules/modules.order"
#	define KMODULE_ALIAS_LIST_PATH "/lib/modules/modules.alias"
#	define KMODULE_DEPENDENCIES_LIST_PATH "/lib/modules/modules.dep"
#	define KMODULE_DIRECTORY "/lib/modules"

#	define KMODULE_FILE_NAME_EXTENSION "ko"

#	define KMODULE_LIST_DELIMETER '#'

typedef struct {
	file_t file;
	const char *name;
	const char *path;
	char _path[PATH_MAX];
} kmodule_t;

typedef struct {
	const char **values;
	unsigned int count;
} kmodule_var_t;

const static char *g_kmodule_fields[] = {
	"depends",
	"alias",
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
	kmodule_var_t _fields[ARRAY_SIZE(g_kmodule_fields)];
} kmodule_info_t;

#	define mod_contents		file.contents
#	define mod_size			file.size
#	define mod_depends		_fields[0]
#	define mod_alias		_fields[1]

bool kmodule_open(kmodule_t *module, const char *name, const char *path);
void kmodule_close(kmodule_t *module);
bool kmodule_info_get(kmodule_t *module, kmodule_info_t *info);
void kmodule_info_free(kmodule_info_t *info);

bool kmodule_load(const char *name, const char *path, bool with_dependencies);
bool kmodule_load_by_alias(const char *alias);

#endif
