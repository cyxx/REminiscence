
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SCALER_H__
#define SCALER_H__

#include <stdint.h>

typedef void (*ScaleProc32)(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h);

enum ScalerType {
	kScalerTypePoint,
	kScalerTypeLinear,
	kScalerTypeInternal,
	kScalerTypeExternal,
};

#define SCALER_TAG 1

struct Scaler {
	uint32_t tag;
	const char *name;
	int factorMin, factorMax;
	ScaleProc32 scale;
};

extern const Scaler _internalScaler;

const Scaler *findScaler(const char *name);

#endif // SCALER_H__
