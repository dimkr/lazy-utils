#ifndef _KMODULE_H_INCLUDED
#	define _KMODULE_H_INCLUDED

#	include <stdbool.h>
#	include <sys/types.h>
#	include <liblazy/common.h>

#	define KMODULE_LIST_PATH "/lib/modules/modules.order"

#	define KMODULE_FILE_NAME_EXTENSION "ko"

typedef struct {
	char *contents;
	size_t size;
	const char *name;
	const char *path;
} kmodule_t;

const static char *g_kmodule_fields[] = {
	"depends",
	"author",
	"alias",
	"description",
	"filename",
	"firmware",
	"intree",
	"license",
	"srcversion",
	"vermagic",
	"version"
};

#	define KMODULE_INFO_DEPENDENCIES_INDEX (0)

typedef struct {
	char *_fields[ARRAY_SIZE(g_kmodule_fields)];
} kmodule_info_t;

#	define mod_depends		_fields[KMODULE_INFO_DEPENDENCIES_INDEX]
#	define mod_author		_fields[1]
#	define mod_alias		_fields[2]
#	define mod_description	_fields[3]
#	define mod_filename		_fields[4]
#	define mod_firmware		_fields[5]
#	define mod_intree		_fields[6]
#	define mod_license		_fields[7]
#	define mod_srcversion	_fields[8]
#	define mod_vermagic		_fields[9]
#	define mod_version		_fields[10]

bool kmodule_open(kmodule_t *module, const char *name, const char *path);
void kmodule_get_info(kmodule_t *module, kmodule_info_t *info);
char *kmodule_get_dependencies(kmodule_t *module);
void kmodule_close(kmodule_t *module);

bool kmodule_load(const char *name);

#endif
