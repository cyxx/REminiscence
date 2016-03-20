
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SCALER_H__
#define SCALER_H__

#include "intern.h"

typedef void (*ScaleProc)(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h);

enum {
	SCALER_POINT_1X = 0,
	SCALER_POINT_2X,
	SCALER_SCALE_2X,
	SCALER_POINT_3X,
	SCALER_SCALE_3X,
	SCALER_POINT_4X,
	SCALER_SCALE_4X,
	NUM_SCALERS = 7
};

struct Scaler {
	const char *name;
	ScaleProc proc;
	uint8_t factor;
};

extern const Scaler _scalers[];

#endif // SCALER_H__
