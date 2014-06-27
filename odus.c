#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <pwd.h>
#include <grp.h>

#include "common.h"

/* the usage message */
#define USAGE "Usage: odus COMMAND...\n"

/* the user */
#define USER "someone"

/* the group */
#define GROUP "someone"

/* the flags argument of unshare() */
#define NAMESPACES (CLONE_NEWIPC | CLONE_NEWNS | CLONE_NEWUTS)

int main(int argc, char *argv[]) {
	/* the user details */
	struct passwd *user = NULL;

	/* the group details */
	struct group *group = NULL;

	/* make sure the number of command-line arguments is valid */
	if (2 > argc) {
		PRINT(USAGE);
		goto end;
	}

	/* get the user details */
	user = getpwnam(USER);
	if (NULL == user) {
		goto end;
	}

	/* get the group details */
	group = getgrnam(GROUP);
	if (NULL == group) {
		goto end;
	}

	/* change the user details in the environment */
	if (-1 == setenv("USER", user->pw_name, 1)) {
		goto end;
	}
	if (-1 == setenv("LOGNAME", user->pw_name, 1)) {
		goto end;
	}
	if (-1 == setenv("HOME", user->pw_dir, 1)) {
		goto end;
	}
	if (-1 == setenv("SHELL", user->pw_shell, 1)) {
		goto end;
	}

	/* change the working directory to the user's home directory */
	if (-1 == chdir(user->pw_dir)) {
		goto end;
	}

	/* isolate the processes */
	if (-1 == unshare(NAMESPACES)) {
		goto end;
	}

	/* change the process owner */
	if (-1 == setgid(group->gr_gid)) {
		goto end;
	}
	if (-1 == setegid(group->gr_gid)) {
		goto end;
	}
	if (-1 == setuid(user->pw_uid)) {
		goto end;
	}
	if (-1 == seteuid(user->pw_uid)) {
		goto end;
	}

	/* run the specified process */
	(void) execvp(argv[1], &argv[1]);

end:
	return EXIT_FAILURE;
}
