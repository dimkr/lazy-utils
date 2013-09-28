#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <liblazy/common.h>
#include <liblazy/proc.h>

bool _is_pid(const char *string) {
	size_t i;

	for (i = 0; strlen(string) > i; ++i) {
		if (0 == isdigit(string[i]))
			return false;
	}

	return true;
}

bool for_each_process(process_callback_t callback, void *parameter) {
	/* the return value */
	bool is_success = false;

	/* the proc file system mount point */
	DIR *proc;

	/* a file under proc */
	struct dirent entry;
	struct dirent *entry_pointer;

	/* open the proc mount point */
	proc = opendir(PROC_MOUNT_POINT);
	if (NULL == proc)
		goto end;

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

		/* run the callback */
		if (false == callback((char *) &entry_pointer->d_name, parameter))
			goto close_proc;
	} while (1);

	/* report success */
	is_success = true;

close_proc:
	/* close the directory */
	(void) closedir(proc);

end:
	return is_success;
}

bool get_process_command_line(const char *pid,
                              char *command_line,
                              ssize_t *size) {
	/* the return value */
	bool is_success = false;

	/* the command-line file's file descriptor */
	int fd;

	/* a loop index */
	ssize_t i;

	/* the process command-line file path */
	char command_line_file_path[PATH_MAX];

	/* format the process command-line file path */
	(void) sprintf((char *) &command_line_file_path,
				   PROCESS_COMMAND_LINE_PATH_TEMPLATE,
				   pid);

	/* open the command-line file */
	fd = open((char *) &command_line_file_path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* read the file */
	*size = read(fd, command_line, (size_t) *size);
	if (0 >= *size)
		goto close_file;

	/* replace NULL characters in the command-line (which is a raw dump of
	 * argv[]) with spaces */
	for (i = 0; (*size - sizeof(char)) > i; ++i) {
		if ('\0' == command_line[i])
			command_line[i] = ' ';
	}

	/* report success */
	is_success = true;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

bool get_process_info(const char *pid, process_info_t *info) {
	/* the return value */
	bool is_success = false;

	/* the process information file's file descriptor */
	int fd;

	/* the process information file path */
	char info_file_path[PATH_MAX];

	/* the process information size */
	ssize_t information_size;

	/* strtok_r()'s position within the process information */
	char *position;

	/* format the process information file path */
	(void) sprintf((char *) &info_file_path, PROCESS_INFO_PATH_TEMPLATE, pid);

	/* open the information file */
	fd = open((char *) &info_file_path, O_RDONLY);
	if (-1 == fd)
		goto end;

	/* read the file */
	information_size = read(fd, (char *) &info->_buffer, sizeof(info->_buffer));
	if (0 >= information_size)
		goto close_file;

	/* terminate the process information string */
	info->_buffer[information_size - 1] = '\0';

	/* terminate the process name */
	info->name = strtok_r((char *) &info->_buffer, "\n", &position);
	if (NULL == info->name)
		goto close_file;
	info->name += STRLEN("Name:\t");

	/* terminate the process state */
	info->state = strtok_r(NULL, "\n", &position);
	if (NULL == info->state)
		goto close_file;
	info->state += STRLEN("State:\t");

	/* keep only the letter (i.e 'S') and strip its meaning, by replacing
	 * the following space with a NULL byte */
	info->state[1] = '\0';

	/* skip the process thread group ID */
	if (NULL == strtok_r(NULL, "\n", &position))
		goto close_file;

	/* skip the process ID */
	if (NULL == strtok_r(NULL, "\n", &position))
		goto close_file;

	/* terminate the parent process ID */
	info->ppid = strtok_r(NULL, "\n", &position);
	if (NULL == info->ppid)
		goto close_file;
	info->ppid += STRLEN("PPid:\t");

	/* report success */
	is_success = true;

close_file:
	/* close the file */
	(void) close(fd);

end:
	return is_success;
}

int signal_name_to_int(const char *name) {
	/* the return value */
	int numeric_value = (-1);

	if (0 == strcmp("KILL", name))
		numeric_value = SIGKILL;
	else {
		if (0 == strcmp("TERM", name))
			numeric_value = SIGTERM;
		else {
			if (0 == strcmp("INT", name))
				numeric_value = SIGINT;
			else {
				if (0 == strcmp("STOP", name))
					numeric_value = SIGSTOP;
				else {
					if (0 == strcmp("CONT", name))
						numeric_value = SIGCONT;
				}
			}
		}
	}

	return numeric_value;
}
