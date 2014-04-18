#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <shadow.h>
#include <crypt.h>
#include <string.h>
#include <unistd.h>
#include "greeter.h"

bool greeter_init(greeter_t *greeter) {
	/* the return value */
	bool is_success = false;

	/* disable echo, so the password does not get printed */
	greeter->terminal.input = stdin;
	greeter->terminal.output = stdout;
	greeter->terminal.error_output = stderr;
	greeter->terminal.should_echo = false;
	if (false == terminal_init(&greeter->terminal))
		goto end;

	/* get the maximum reading buffer size; if there's no limit, use a sane
	 * fallback value */
	greeter->buffer_size = (size_t) sysconf(_SC_GETPW_R_SIZE_MAX);
	if (-1 == greeter->buffer_size)
		greeter->buffer_size = SHADOW_READING_BUFFER_SIZE;

	/* allocate memory for the reading buffer */
	greeter->buffer = malloc((size_t) greeter->buffer_size);
	if (NULL == greeter->buffer)
		goto end;

	/* open the system log */
	openlog(LOGIN_LOG_IDENTITY, LOG_NDELAY, LOG_AUTH);

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool _get_correct_password_hash(const char *user,
                                char *buffer,
                                size_t buffer_size,
                                struct spwd **hash) {
	/* the return value */
	bool is_success = false;

	/* the passed pointer */
	struct spwd *original_pointer;

	/* save the passed pointer */
	original_pointer = *hash;

	/* get the correct user password hash */
	if (0 != getspnam_r(user, original_pointer, buffer, buffer_size, hash))
		goto end;
	if (original_pointer != *hash)
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool _is_password_correct(const char *correct_hash, const char *password) {
	/* the return value */
	bool is_correct = false;

	/* password hashing data */
	struct crypt_data encryption_data;

	/* the password hash */
	char *hash;

	/* hash the password */
	encryption_data.initialized = 0;
	hash = crypt_r(password, correct_hash, &encryption_data);
	if (NULL == hash)
		goto end;

	/* compare the password hashes */
	if (0 != strcmp(hash, correct_hash))
		goto end;

	/* report the password is correct */
	is_correct = true;

end:
	return is_correct;
}

bool _authenticate_once(terminal_t *terminal,
                        const char *user,
                        const char *hash) {
	/* the return value */
	bool is_authenticated = false;

	/* the plain-text password provided by the user */
	char password[1 + MAX_PASSWORD_LENGTH];

	/* show a password prompt */
	if (false == terminal_read(terminal,
	                           (char *) &password,
	                           sizeof(password),
	                           "Enter the password for %s: ",
	                           user))
		goto end;

	/* check whether the password is correct */
	is_authenticated = _is_password_correct(hash, (char *) &password);

	/* zero the raw password */
	(void) memset((char *) &password, 0, sizeof(password));

end:
	return is_authenticated;
}

bool _authenticate(greeter_t *greeter, const char *user) {
	/* the return value */
	bool is_authenticated = false;

	/* the number of failed authentication attempts */
	unsigned int attempts;

	/* the correct password hash */
	struct spwd hash;

	/* a pointer to the hash structure */
	struct spwd *hash_pointer;

	/* the user details */
	struct passwd *user_details_pointer;

	/* get the correct password hash */
	hash_pointer = &hash;
	if (false == _get_correct_password_hash(user,
	                                        greeter->buffer,
	                                        greeter->buffer_size,
	                                        &hash_pointer))
		goto end;

	for (attempts = 0; MAX_LOGIN_ATTEMPTS > attempts; ++attempts) {
		/* show a password entry prompt and check whether the entered password
		 * is correct */
		if (true == _authenticate_once(&greeter->terminal,
		                               user,
		                               hash_pointer->sp_pwdp))
			goto correct_password;

		/* log the failure in the system log */
		syslog(LOG_WARNING, "failed authentication attempt for %s", user);

		/* upon failure to authenticate, wait for a while before the next
		 * attempt */
		(void) sleep(LOGIN_FAILURE_SLEEP_INTERVAL);
	}
	goto end;

correct_password:
	/* log the successful authentication */
	syslog(LOG_INFO, "successful authentication attempt for %s", user);

	/* get the user details */
	if (0 != getpwnam_r(user,
	                    &greeter->user_details,
	                    greeter->buffer,
	                    greeter->buffer_size,
	                    &user_details_pointer))
		goto end;
	if (&greeter->user_details != user_details_pointer)
		goto end;

	/* report the user is authenticated */
	is_authenticated = true;

end:
	return is_authenticated;
}

bool greeter_login(greeter_t *greeter, const char *user) {
	/* the return value */
	bool is_success = false;

	/* alternate user name */
	char user_fallback[1 + MAX_USER_NAME_LENGTH];

	/* print the contents of /etc/issue */
	if (true == greeter->is_interactive)
		(void) terminal_print_file(&greeter->terminal, "/etc/issue");

	/* if no user name was specified, show a user name prompt */
	if (NULL == user) {
		if (false == terminal_read(&greeter->terminal,
		                           (char *) &user_fallback,
		                           sizeof(user_fallback),
		                           "Log in as: "))
			goto end;
		user = (char *) &user_fallback;
	}

	/* authenticate the user */
	if (false == _authenticate(greeter, user))
		goto end;

	/* set the user ID */
	if (-1 == setuid(greeter->user_details.pw_uid))
		goto end;

	/* change the working directory to the user home directory */
	if (-1 == chdir(greeter->user_details.pw_dir))
		goto end;

	/* print the contents of /etc/motd */
	if (true == greeter->is_interactive)
		(void) terminal_print_file(&greeter->terminal, "/etc/motd");

	/* report success */
	is_success = true;

end:
	return is_success;
}

bool greeter_end(greeter_t *greeter) {
	/* the return value */
	bool is_success = false;

	/* close the system log */
	closelog();

	/* free the buffer */
	free(greeter->buffer);

	/* restore the original terminal state */
	if (false == terminal_end(&greeter->terminal))
		goto end;

	/* report success */
	is_success = true;

end:
	return is_success;
}
