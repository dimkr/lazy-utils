#include <stdio.h>
#include <string.h>

char *fgetln(FILE *fp, size_t *lenp) {
	char buffer[1024];
	char *result;

	result = fgets((char *) &buffer, sizeof(buffer), fp);
	if (NULL != result)
		*lenp = strlen(result);

	return result;
}
