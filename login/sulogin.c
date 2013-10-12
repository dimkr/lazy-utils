#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <liblazy/login.h>

/* the fallback value of TERM */
#define FALLBACK_TERM "linux"

#define MAX_ENVIRONMENT_VARIABLE_SIZE (1024)

int main(int argc, char *argv[]) {
	/* the greeter */
	greeter_t greeter;

	/* the user name */
	char *user;

	/* the shell command-line arguments */
	char shell[PATH_MAX];
	char *shell_argv[3];

	/* the shell environment */
	char shell_environment_home[MAX_ENVIRONMENT_VARIABLE_SIZE];
	char *term;
	char shell_environment_term[MAX_ENVIRONMENT_VARIABLE_SIZE];
	char shell_environment_user[MAX_ENVIRONMENT_VARIABLE_SIZE];
	char *shell_environment[4];

	/* parse the command-line */
	switch (argc) {
		case (1):
			user = NULL;
			greeter.is_interactive = true;
			break;

		case (2):
			user = argv[1];
			greeter.is_interactive = false;
			break;

		default:
			goto end;
	}

	/* initialize the greeter */
	if (false == greeter_init(&greeter))
		goto end;

	/* authenticate the user */
	if (false == greeter_login(&greeter, user))
		goto close_greeter;

	/* set the value of TERM */
	term = getenv("TERM");
	if (NULL == term)
		term = FALLBACK_TERM;
	if (sizeof(shell_environment_term) <= snprintf(
	                                           (char *) &shell_environment_term,
	                                           sizeof(shell_environment_term),
	                                           "TERM=%s",
	                                           term))
		goto close_greeter;

	/* same, with HOME */
	if (sizeof(shell_environment_home) <= snprintf(
	                                           (char *) &shell_environment_home,
	                                           sizeof(shell_environment_home),
	                                           "HOME=%s",
	                                           greeter.user_details.pw_dir))
		goto close_greeter;

	/* same, with USER */
	if (sizeof(shell_environment_user) <= snprintf(
	                                           (char *) &shell_environment_user,
	                                           sizeof(shell_environment_user),
	                                           "USER=%s",
	                                           greeter.user_details.pw_name))
		goto close_greeter;

	/* copy the shell path from the reading buffer to a statically-allocated
	 * buffer */
	(void) strcpy((char *) &shell, greeter.user_details.pw_shell);

	/* close the greeter */
	if (false == greeter_end(&greeter))
		goto end;

	/* if the user name was specified interactively, pass "-l" to the shell, so
	 * it acts as a login shell */
	if (NULL == user) {
		shell_argv[0] = (char *) &shell;
		shell_argv[1] = "-l";
		shell_argv[2] = NULL;
	} else {
		shell_argv[0] = (char *) &shell;
		shell_argv[1] = NULL;
	}

	/* execute the shell */
	shell_environment[0] = (char *) &shell_environment_term;
	shell_environment[1] = (char *) &shell_environment_home;
	shell_environment[2] = (char *) &shell_environment_user;
	shell_environment[3] = NULL;
	(void) execve(shell_argv[0],
	              (char **) &shell_argv,
	              (char **) &shell_environment);

	goto end;

close_greeter:
	/* close the greeter */
	(void) greeter_end(&greeter);

end:
	return EXIT_FAILURE;
}
