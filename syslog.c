#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"
#include "syslog.h"

/* the usage message */
#define USAGE "Usage: syslog\n"

int main(int argc, char *argv[]) {
	/* a log chunk */
	char chunk[1 + MAX_MESSAGE_LENGTH] = {'\0'};

	/* the chunk size */
	ssize_t size = 0;

	/* a loop index */
	ssize_t i = 0;

	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the log file */
	int log_file = 0;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc) {
		PRINT(USAGE);
		goto end;
	}

	/* open the log file */
	log_file = open(LOG_FILE_PATH, O_RDONLY);
	if (-1 == log_file) {
		goto end;
	}

	do {
		/* read a log chunk */
		size = read(log_file, (void *) chunk, sizeof(chunk));
		switch (size) {
			case 0:
				exit_code = EXIT_SUCCESS;

			case (-1):
				goto close_log;

			default:
				/* de-XOR the chunk */
				for (i = 0; size > i; ++i) {
					chunk[i] ^= XOR_KEY;
				}

				/* write it to standard output */
				if (size != write(STDOUT_FILENO,
				                  (void *) chunk,
				                  (size_t) size)) {
					goto close_log;
				}
		}
	} while (1);

close_log:
	/* close the log file */
	(void) close(log_file);

end:
	return exit_code;
}
