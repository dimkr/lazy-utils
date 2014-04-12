#include <string.h>
#include <liblazy/common.h>
#include <liblazy/mime.h>

static const mime_type_t g_mime_types[] = {
	{ "html", STRLEN("html"), "text/html" },
	{ "png", STRLEN("png"), "image/png" },
	{ "css", STRLEN("css"), "text/css" }
};

const char *mime_type_guess(const char *path) {
	/* the return value */
	const char *type = "application/octet-stream";

	/* the file name extension */
	const char *extension;

	/* a loop index */
	unsigned int i;

	/* locate the file name extension */
	extension = strrchr(path, '.');
	if (NULL == extension)
		goto end;
	++extension;

	/* compare the extension against known ones */
	for (i = 0; ARRAY_SIZE(g_mime_types) > i; ++i) {
		if (0 == strncmp(extension,
		                 g_mime_types[i].extension,
		                 sizeof(char) * g_mime_types[i].length)) {
			type = g_mime_types[i].mime_type;
			break;
		}
	}

end:
	return type;
}
