#ifndef _PROC_H_INCLUDED
#	define _PROC_H_INCLUDED

#	include <stdbool.h>

/* the proc mount point */
#define PROC_MOUNT_POINT "/proc"

/* the maximum size of the process command-line */
#define MAX_COMMAND_LINE_SIZE (4095)

/* the process command-line file path template */
#define PROCESS_COMMAND_LINE_PATH_TEMPLATE PROC_MOUNT_POINT"/%s/cmdline"

/* the maximum size of the process information file */
#define MAX_PROCESS_INFO_SIZE (1023)

/* the process information file path template */
#define PROCESS_INFO_PATH_TEMPLATE PROC_MOUNT_POINT"/%s/status"

typedef bool (*process_callback_t)(const char *pid, void *parameter);

typedef struct {
	char _buffer[MAX_PROCESS_INFO_SIZE];
	char *name;
	char *pid;
	char *ppid;
	char *state;
} process_info_t;

bool for_each_process(process_callback_t callback, void *parameter);

bool get_process_command_line(const char *pid, char *command_line, ssize_t *size);

bool get_process_info(const char *pid, process_info_t *info);

int signal_option_to_int(const char *option);

#endif
