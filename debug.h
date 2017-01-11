
#ifndef DEBUG_H__
#define DEBUG_H__

enum {
	DBG_SCRIPT = 1 << 0,
	DBG_TRAPS  = 1 << 1,
	DBG_GAME   = 1 << 2,
	DBG_MEMORY = 1 << 3,
	DBG_TEXT   = 1 << 4,
	DBG_SEGEXE = 1 << 5,
};

extern int g_debugMask;

extern void debug(int mask, const char *msg, ...);
extern void info(const char *msg, ...);
extern void error(const char *msg, ...);
extern void warning(const char *msg, ...);

extern char *g_err;
#define sizeof__g_err 512

#endif // DEBUG_H__
