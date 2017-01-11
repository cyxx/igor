
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "debug.h"

int g_debugMask = 0;

void debug(int mask, const char *msg, ...) {
	char buf[1024];
	if (mask & g_debugMask) {
		va_list va;
		va_start(va, msg);
		vsprintf(buf, msg, va);
		va_end(va);
		fprintf(stdout, "%s\n", buf);
	}
}

void info(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	fprintf(stdout, "INFO: %s!\n", buf);
}

void error(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	fprintf(stderr, "ERROR: %s!\n", buf);
	snprintf(g_err, sizeof__g_err, "%s", buf);
	exit(-1);
}

void warning(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	fprintf(stderr, "WARNING: %s!\n", buf);
}

