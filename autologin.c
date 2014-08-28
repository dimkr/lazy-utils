#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/sendfile.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"

/* the usage message */
#define USAGE "Usage: autologin [USER]\n"

/* the default user */
#define DEFAULT_USER "root"

/* the issue file path */
#define ISSUE_PATH CONF_DIR"/issue"

/* the motd file path */
#define MOTD_PATH CONF_DIR"/motd"

int main(int argc, char *argv[]) {
	/* issue's attributes */
	struct stat issue_attributes = {0};

	/* motd's attributes */
	struct stat motd_attributes = {0};

	/* a file offset */
	off_t offset = 0;

	/* the issue file descriptor */
	int issue = (-1);

	/* the motd file descriptor */
	int motd = (-1);

	/* a flag set upon success */
	bool success = false;

	/* the user */
	const char *user = NULL;

	/* the user details */
	struct passwd *details = NULL;

	/* parse the command-line */
	switch (argc) {
		case 1:
			user = DEFAULT_USER;
			break;

		case 2:
			user = argv[1];
			break;

		default:
			PRINT(USAGE);
			goto end;
	}

	/* get the root user ID */
	details = getpwnam("root");
	if (NULL == details) {
		goto end;
	}

	/* make sure the process runs as root */
	if (geteuid() != details->pw_uid) {
		goto end;
	}

	/* get the size of issue and motd */
	if (-1 == stat(ISSUE_PATH, &issue_attributes)) {
		goto end;
	}
	if (-1 == stat(MOTD_PATH, &motd_attributes)) {
		goto end;
	}

	/* open issue and motd */
	issue = open(ISSUE_PATH, O_RDONLY);
	if (-1 == issue) {
		goto end;
	}
	motd = open(MOTD_PATH, O_RDONLY);
	if (-1 == motd) {
		goto close_issue;
	}

	/* print the contents of /etc/issue */
	if (0 < issue_attributes.st_size) {
		if ((ssize_t) issue_attributes.st_size != sendfile(
		                                   STDOUT_FILENO,
		                                   issue,
		                                   &offset,
		                                   (size_t) issue_attributes.st_size)) {
			goto close_motd;
		}
	}

	/* get the user details */
	details = getpwnam(user);
	if (NULL == details) {
		goto close_motd;
	}

	/* set important environment variables */
	if (-1 == setenv("USER", details->pw_name, 1)) {
		goto close_motd;
	}
	if (-1 == setenv("HOME", details->pw_dir, 1)) {
		goto close_motd;
	}
	if (-1 == setenv("SHELL", details->pw_shell, 1)) {
		goto close_motd;
	}

	/* change the process owner */
	if (-1 == setgid(details->pw_uid)) {
		goto close_motd;
	}
	if (-1 == setegid(details->pw_uid)) {
		goto close_motd;
	}
	if (-1 == setuid(details->pw_uid)) {
		goto close_motd;
	}
	if (-1 == seteuid(details->pw_uid)) {
		goto close_motd;
	}

	/* open the system log */
	openlog("autologin", LOG_NDELAY | LOG_CONS | LOG_PID, LOG_AUTH);

	/* change the working directory */
	if (-1 == chdir(details->pw_dir)) {
		goto close_log;
	}

	/* log the user name and shell */
	syslog(LOG_INFO,
	       "automatically logging %s with %s",
	       details->pw_name,
	       details->pw_shell);

	/* print the contents of /etc/motd */
	if (0 == motd_attributes.st_size) {
		goto success;
	}
	offset = 0;
	if ((ssize_t) motd_attributes.st_size != sendfile(
	                                        STDOUT_FILENO,
	                                        motd,
	                                        &offset,
	                                        (size_t) motd_attributes.st_size)) {
		goto close_log;
	}

success:
	success = true;

close_log:
	/* close the system log */
	closelog();

close_motd:
	/* close motd */
	(void) close(motd);

close_issue:
	/* close issue */
	(void) close(issue);

end:
	/* execute the shell */
	if (true == success) {
		(void) execlp(details->pw_shell,
		              details->pw_shell,
		              "-l",
		              (char *) NULL);
	}

	return EXIT_FAILURE;
}
