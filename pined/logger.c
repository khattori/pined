#include <stdarg.h>
#include <stdio.h>
#include <langinfo.h>
#include <time.h>

#include "logger.h"

static int s_log_level = PINE_LOG_INFO;
static FILE *s_log_out_p;

static char *s_log_level_table[] = {
	"NONE",
	"ERROR",
	"WARN",
	"INFO",
	"DEBUG",
	"MAX"
};

int log_init(char *log_file) {
	if (log_file == NULL) {
		s_log_out_p = stderr;
		return 0;
	}
	s_log_out_p = fopen(log_file, "a");
	if (s_log_out_p == NULL) {
		perror("log_init: fopen()");
		return -1;
	}
	return 0;
}

void log_setlevel(int level) {
	s_log_level = level;
}

void logger(int level, char format[], ...) {
	va_list ap;
	char buf[BUFSIZ];
	time_t cur_time;
	struct tm cur_tm;

	if (s_log_level < level) {
		return;
	}
	if ((cur_time = time(NULL)) < 0) {
		perror("logger: time()");
		return;
	}
	if ((localtime_r(&cur_time, &cur_tm)) == NULL) {
		perror("logger: localtime_r()");
		return;
	}
	strftime(buf, sizeof buf, nl_langinfo(D_T_FMT), &cur_tm);
	va_start(ap, format);
	fprintf(s_log_out_p, "%s [%s]: ", buf, s_log_level_table[level]);
	vfprintf(s_log_out_p, format, ap);
	fprintf(s_log_out_p, "\n");
	fflush(s_log_out_p);
	va_end(ap);
}
