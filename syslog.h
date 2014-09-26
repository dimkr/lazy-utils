#ifndef _SYSLOG_H_INCLUDED
#	define _SYSLOG_H_INCLUDED

#	include "common.h"

/* the log file path */
#	define LOG_FILE_PATH "/var/log/messages"

/* the maximum length of a log message */
#	define MAX_MESSAGE_LENGTH (MAX_LENGTH)

/* the log XOR key */
#	define XOR_KEY (0xF1)

#endif