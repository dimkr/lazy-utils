#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* a command-line option */
	int option;

	/* the operating system details */
	struct utsname details;

	/* make sure the number of command-line arguments is valid */
	if (1 == argc)
		goto end;

	/* get the operating system details */
	if (-1 == uname(&details))
		goto end;

	/* parse the command-line */
	do {
		option = getopt(argc, argv, "asrvm");
		if (-1 == option)
			break;

		switch (option) {
			case 'a':
				if (0 > printf("%s %s %s %s\n",
				               (char *) &details.sysname,
				               (char *) &details.release,
				               (char *) &details.version,
				               (char *) &details.machine))
					goto end;
				break;

			case 's':
				if (0 > printf("%s\n", (char *) &details.sysname))
					goto end;
				break;

			case 'r':
				if (0 > printf("%s\n", (char *) &details.release))
					goto end;
				break;

			case 'v':
				if (0 > printf("%s\n", (char *) &details.version))
					goto end;
				break;

			case 'm':
				if (0 > printf("%s\n", (char *) &details.machine))
					goto end;
				break;

			default:
				goto end;
		}
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

end:
	return exit_code;
}
