
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef UTIL_H__
#define UTIL_H__

#include "intern.h"

enum {
	DBG_RES    = 1 << 0,
	DBG_MENU   = 1 << 1,
	DBG_UNPACK = 1 << 2,
	DBG_PGE    = 1 << 3,
	DBG_VIDEO  = 1 << 4,
	DBG_GAME   = 1 << 5,
	DBG_COL    = 1 << 6,
	DBG_SND    = 1 << 7,
	DBG_CUT    = 1 << 8,
	DBG_MOD    = 1 << 9,
	DBG_SFX    = 1 << 10,
	DBG_FILE   = 1 << 11,
	DBG_DEMO   = 1 << 12,
	DBG_PRF    = 1 << 13,
	DBG_MIDI   = 1 << 14,
	DBG_PAQ    = 1 << 15
};

extern uint32_t g_debugMask;

extern void debug(uint32_t cm, const char *msg, ...);
extern void error(const char *msg, ...);
extern void warning(const char *msg, ...);
extern void info(const char *msg, ...);

#ifdef NDEBUG
#define debug(x, ...)
#endif

#endif // UTIL_H__
