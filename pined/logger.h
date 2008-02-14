#ifndef __INC_LOGGER_H__
#define __INC_LOGGER_H__

#define PINE_LOG_NONE	0
#define PINE_LOG_ERROR	1
#define PINE_LOG_WARN	2
#define PINE_LOG_INFO	3
#define PINE_LOG_DEBUG	4
#define PINE_LOG_MAX	5


int log_init(char *log_file);
void log_setlevel(int level);
void logger(int level, char format[], ...);

#endif /* __INC_LOGGER_H__ */
