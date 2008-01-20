#include <stdarg.h>
#include <stdio.h>

#include "logger.h"

static int log_level = RSRV_LOG_INFO;

void log_setlevel(int level) {
	log_level = level;
}

void logger(int level, char format[], ...) {
	va_list ap;

	if (log_level < level) {
		return;
	}

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
