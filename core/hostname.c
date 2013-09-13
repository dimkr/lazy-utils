#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the hostname */
	char hostname[MAXHOSTNAMELEN];

	/* the hostname length */
	int hostname_length;

	switch (argc) {
		case 2:
			/* set the hostname */
			if (-1 == sethostname(argv[1], strlen(argv[1])))
				goto end;
			break;

		case 1:
			/* obtain the hostname */
			if (-1 == gethostname((char *) &hostname, sizeof(hostname)))
				goto end;
			hostname_length = strlen((char *) &hostname);

			/* write the hostname to standard output */
			if ((ssize_t) hostname_length != write(STDOUT_FILENO,
			                                      (char *) &hostname,
			                                      (size_t) hostname_length))
				goto end;
			break;

		default:
			goto end;
	}

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
