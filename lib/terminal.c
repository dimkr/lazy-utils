#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <liblazy/io.h>
#include <liblazy/terminal.h>

bool terminal_init(terminal_t *terminal) {
	/* the return value */
	bool is_success = false;

	/* get the output stream file descriptor */
	terminal->output_fd = fileno(terminal->output);

	/* if echo should not be disabled, do nothing */
	if (true == terminal->should_echo)
		goto success;

	/* get the terminal attributes */
	if (-1 == tcgetattr(terminal->output_fd, &terminal->original_attributes))
		goto end;

	/* disable echo */
	(void) memcpy(&terminal->attributes,
	              &terminal->original_attributes,
	              sizeof(terminal->attributes));
	terminal->attributes.c_lflag &= (~ECHO);
	if (-1 == tcsetattr(terminal->output_fd, TCSAFLUSH, &terminal->attributes))
		goto end;

success:
	/* report success */
	is_success = true;

end:
	return is_success;
}

bool terminal_read(terminal_t *terminal,
                   char *buffer,
                   size_t len,
                   const char *prompt_format,
                   ...) {
	/* the return value */
	bool is_success = false;

	/* the prompt tokens */
	va_list prompt_tokens;

	/* a line break */
	char line_break;

	/* initialize the list of prompt tokens */
	va_start(prompt_tokens, prompt_format);

	/* show a password prompt */
	if (0 > vfprintf(terminal->output, prompt_format, prompt_tokens))
		goto free_arguments;
	if (0 != fflush(terminal->output))
		goto free_arguments;

	/* read the user input */
	if (NULL == fgets(buffer, len, terminal->input))
		goto free_arguments;

	/* strip the trailing line break */
	buffer[strnlen((char *) buffer, len) - sizeof(char)] = '\0';

	/* write a line break */
	line_break = '\n';
	if (sizeof(char) != write(terminal->output_fd, &line_break, sizeof(char)))
		goto end;

	/* report success */
	is_success = true;

free_arguments:
	/* free the list of prompt tokens */
	va_end(prompt_tokens);

end:
	return is_success;
}

bool terminal_print_file(terminal_t *terminal, const char *path) {
	/* the return value */
	bool is_success = false;

	/* the file descriptor */
	int fd;

	/* the reading buffer */
	char buffer[FILE_READING_BUFFER_SIZE];

	/* read chunk size */
	ssize_t chunk_size;

	/* open the file */
	fd = open(path, O_RDONLY);
	if (-1 == fd)
		goto end;

	do {
		/* read a chunk */
		chunk_size = read(fd, (char *) &buffer, sizeof(buffer));
		switch (chunk_size) {
			case (-1):
				goto close_file;

			case (0):
				/* once the file end was reached, report success */
				is_success = true;
				goto close_file;

			default:
				/* print the chunk */
				if (chunk_size != write(terminal->output_fd,
				                        (char *) &buffer,
				                        (size_t) chunk_size))
					goto close_file;
				break;
		}
	} while (1);

close_file:
	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

bool terminal_end(terminal_t *terminal) {
	/* the return value */
	bool is_success = false;

	/* if echo was not disabled, do nothing */
	if (true == terminal->should_echo)
		goto success;

	/* restore the original terminal attributes */
	if (-1 == tcsetattr(terminal->output_fd,
	                    TCSAFLUSH,
	                    &terminal->original_attributes))
		goto end;

success:
	/* report success */
	is_success = true;

end:
	return is_success;
}
