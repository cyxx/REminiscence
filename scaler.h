/* REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
