#ifndef __INC_LOGGER_H__
#define __INC_LOGGER_H__

#define RSRV_LOG_NONE	0
#define RSRV_LOG_ERROR	1
#define RSRV_LOG_WARN	2
#define RSRV_LOG_INFO	3
#define RSRV_LOG_DEBUG	4
#define RSRV_LOG_MAX	5


int log_init(char *log_file);
void log_setlevel(int level);
void logger(int level, char format[], ...);

#endif /* __INC_LOGGER_H__ */
