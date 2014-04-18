#ifndef _TERMINAL_H_INCLUDED
#	define _TERMINAL_H_INCLUDED

#	include <stdio.h>
#	include <stdbool.h>
#	include <termios.h>

typedef struct {
	struct termios original_attributes;
	struct termios attributes;
	FILE *input;
	FILE *output;
	int output_fd;
	FILE *error_output;
	bool should_echo;
} terminal_t;

bool terminal_init(terminal_t *terminal);

bool terminal_read(terminal_t *terminal,
                   char *buffer,
                   size_t len,
                   const char *prompt_format,
                   ...);

bool terminal_print_file(terminal_t *terminal, const char *path);

bool terminal_end(terminal_t *terminal);

#endif
