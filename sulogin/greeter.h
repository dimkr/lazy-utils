#ifndef _LOGIN_H_INCLUDED
#	define _LOGIN_H_INCLUDED

#	include <stdbool.h>
#	include <sys/types.h>
#	include <pwd.h>
#	include <liblazy/terminal.h>

#	define MAX_USER_NAME_LENGTH (31)

typedef struct {
	terminal_t terminal;
	char *buffer;
	size_t buffer_size;
	struct passwd user_details;
	bool is_interactive;
} greeter_t;

#define LOGIN_LOG_IDENTITY "login"

#define SHADOW_READING_BUFFER_SIZE (512)

#define MAX_PASSWORD_LENGTH (127)

#define MAX_LOGIN_ATTEMPTS (3)

#define LOGIN_FAILURE_SLEEP_INTERVAL (5)

bool greeter_init(greeter_t *greeter);
bool greeter_login(greeter_t *greeter, const char *user);
bool greeter_end(greeter_t *greeter);

#endif
