#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <liblazy/common.h>

/* the proc mount point */
#define PROC_MOUNT_POINT "/proc"

/* the process information file path template */
#define PROCESS_INFO_PATH_TEMPLATE "/proc/%s/status"

/* the maximum size of the process command-line */
#define MAX_COMMAND_LINE_SIZE (4095)

/* the process command-line file path template */
#define PROCESS_COMMAND_LINE_PATH_TEMPLATE "/proc/%s/cmdline"

/* the maximum size of the process information file */
#define MAX_PROCESS_INFO_SIZE (1023)

/* the format of a printed entry */
#define OUTPUT_FORMAT "%-5s %-5s %-5s %s\n"

bool _is_pid(const char *string) {
	size_t i;

	for (i = 0; strlen(string) > i; ++i) {
		if (0 == isdigit(string[i]))
			return false;
	}

	return true;
}

bool _read_file(const char *path, char *buffer, ssize_t *size) {
	/* the return value */
	bool is_success = false;

	/* the file descriptor */
	int fd;

	/* open the file */
	fd = open(path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* read the file */
	*size = read(fd, buffer, (size_t) *size);
	if (0 >= *size)
		goto close_file;

	/* report success */
	is_success = true;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

int main(int argc, char *argv[]) {
	/* the exit code */
	int exit_code = EXIT_FAILURE;

	/* the proc mount point */
	DIR *proc;

	/* a file under proc */
	struct dirent entry;
	struct dirent *entry_pointer;

	/* the process command-line */
	char command_line[MAX_COMMAND_LINE_SIZE];

	/* the process command-line size, in bytes */
	ssize_t command_line_size;

	/* the process command-line file path */
	char command_line_file_path[PATH_MAX];

	/* the process information */
	char information[MAX_PROCESS_INFO_SIZE];

	/* the process information size, in bytes */
	ssize_t information_size;

	/* the process information file path */
	char information_file_path[PATH_MAX];

	/* strtok_r()'s position within the process information */
	char *position;

	/* the process parent */
	char *ppid;

	/* the process state */
	char *state;

	/* a loop index */
	ssize_t i;

	/* make sure the number of command-line arguments is valid */
	if (1 != argc)
		goto end;

	/* open the proc mount point */
	proc = opendir(PROC_MOUNT_POINT);
	if (NULL == proc)
		goto end;

	/* print the column headers */
	if (0 > printf(OUTPUT_FORMAT, "PID", "PPID", "STATE", "NAME"))
		goto close_proc;

	do {
		/* read an entry */
		if (0 != readdir_r(proc, &entry, &entry_pointer))
			goto close_proc;

		/* if the last entry was reached, stop */
		if (NULL == entry_pointer)
			break;

		/* if the directory is does not contain process information, skip it */
		if (false == _is_pid((char *) &entry_pointer->d_name))
			continue;

		/* format the process command-line file path */
		(void) sprintf((char *) &command_line_file_path,
		               PROCESS_COMMAND_LINE_PATH_TEMPLATE,
		               (char *) &entry_pointer->d_name);

		/* read the process information */
		command_line_size = STRLEN(command_line);
		if (false == _read_file((char *) &command_line_file_path,
		                        (char *) &command_line,
		                        &command_line_size))
			continue;

		/* format the process information file path */
		(void) sprintf((char *) &information_file_path,
		               PROCESS_INFO_PATH_TEMPLATE,
		               (char *) &entry_pointer->d_name);

		/* read the process information */
		information_size = STRLEN(information);
		if (false == _read_file((char *) &information_file_path,
		                        (char *) &information,
		                        &information_size))
			continue;

		/* terminate the process information string */
		information[information_size - 1] = '\0';

		/* skip the process name */
		if (NULL == strtok_r((char *) &information, "\n", &position))
			continue;

		/* terminate the process state */
		state = strtok_r(NULL, "\n", &position);
		if (NULL == state)
			continue;
		state += STRLEN("State:\t");

		/* keep only the letter (i.e 'S') and strip its meaning, by replacing
		 * the following space with a NULL byte */
		state[1] = '\0';

		/* skip the process thread group ID */
		if (NULL == strtok_r(NULL, "\n", &position))
			continue;

		/* skip the process ID */
		if (NULL == strtok_r(NULL, "\n", &position))
			continue;

		/* terminate the parent process ID */
		ppid = strtok_r(NULL, "\n", &position);
		if (NULL == ppid)
			continue;
		ppid += STRLEN("PPid:\t");

		/* replace NULL characters in the command-line (which is a raw dump of
		 * argv[]) with spaces */
		for (i = 0; (command_line_size - 1) > i; ++i) {
			if ('\0' == command_line[i])
				command_line[i] = ' ';
		}

		/* print the process details */
		if (0 > printf(OUTPUT_FORMAT,
		               (char *) &entry_pointer->d_name,
		               ppid,
		               state,
		               (char *) &command_line))
			goto close_proc;
	} while (1);

	/* report success */
	exit_code = EXIT_SUCCESS;

close_proc:
	/* close the directory */
	(void) closedir(proc);

end:
	return exit_code;
}
