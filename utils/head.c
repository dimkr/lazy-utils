#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

/* the maximum size of a read line */
#define MAX_LINE_SIZE (sizeof(char) * 1024)

typedef struct {
	char *buffer;
	size_t size;
} _line_t;

typedef bool (*_reader_t)(size_t *count, _line_t **lines);

bool _read_lines(size_t *count, _line_t **lines) {
	/* the return value */
	bool is_success = false;

	/* a loop index */
	size_t i;

	/* allocate memory for all lines */
	*lines = malloc(sizeof(_line_t) * (*count));
	if (NULL == *lines)
		goto end;

	for (i = 0; *count > i; ++i) {
		/* allocate memory for the line */
		(*lines)[i].size = MAX_LINE_SIZE;
		(*lines)[i].buffer = malloc((*lines)[i].size);
		if (NULL == (*lines)[i].buffer)
			goto end;

		/* read one line */
		(*lines)[i].size = (size_t) getline(&(*lines)[i].buffer,
		                                    &(*lines)[i].size,
		                                    stdin);
		if (-1 == (*lines)[i].size)
			goto end;
	}

	/* report success */
	is_success = true;

end:
	return is_success;
}


bool _read_bytes(size_t *count, _line_t **lines) {
	/* the return value */
	bool is_success = false;

	/* the number of bytes to read */
	size_t bytes;

	/* allocate memory for one line */
	*lines = malloc(sizeof(_line_t));
	if (NULL == *lines)
		goto end;
	bytes = *count;
	*count = 1;

	/* allocate a buffer */
	(*lines)[0].buffer = malloc(*count);
	if (NULL == (*lines)[0].buffer)
		goto end;

	/* read the requested number of bytes into the buffer */
	(*lines)[0].size = (size_t) read(STDIN_FILENO, (*lines)[0].buffer, bytes);
	if (-1 == (*lines)[0].size)
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the number of units to read */
	size_t count;

	/* the number, in textual form */
	const char *count_textual;

	/* the read lines */
	_line_t *lines;

	/* a loop index */
	size_t i;

	/* the reading function */
	_reader_t reader;

	/* the required number of command-line arguments */
	int required_argc;

	/* make sure the number of command-line arguments is valid */
	if (2 > argc)
		goto end;

	/* decide which reading function should be used */
	reader = _read_lines;
	count_textual = argv[2];
	required_argc = 3;
	if (0 != strcmp("-n", argv[1])) {
		if (0 == strcmp("-c", argv[1]))
			reader = _read_bytes;
		else {
			if ('-' != argv[1][0])
				goto end;
			required_argc = 2;
			count_textual = &argv[1][1];
		}
	}
	if (required_argc != argc)
		goto end;

	/* conver the amount of units to read to an integer */
	count = (size_t) atoi(count_textual);
	if (0 == count)
		goto end;

	/* read the requied amount of data */
	if (false == reader(&count, &lines)) {
		if (NULL == lines)
			goto end;
		goto free_lines;
	}

	/* print the filtered data */
	for (i = 0; count > i; ++i) {
		if (lines[i].size != (size_t) write(STDOUT_FILENO,
		                                    lines[i].buffer,
		                                    lines[i].size))
			goto free_lines;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

free_lines:
	/* free each line; when NULL is reached (i.e malloc() or realloc() failure),
	 * break the loop, since the next pointers are were not used and therefore
	 * uninitialized */
	for (i = count - 1; 0 <= i; --i) {
		if (NULL == lines[i].buffer)
			break;
		free(lines[i].buffer);
	}

	/* free the lines array */
	free(lines);

end:
	return exit_code;
}
