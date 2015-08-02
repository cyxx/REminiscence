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

#ifndef GRAPHICS_H__
#define GRAPHICS_H__

#include "intern.h"

struct Graphics {
	uint8_t *_layer;
	int16_t _areaPoints[0x200];
	int16_t _crx, _cry, _crw, _crh;

	void setClippingRect(int16_t vx, int16_t vy, int16_t vw, int16_t vh);
	void drawPoint(uint8_t color, const Point *pt);
	void drawLine(uint8_t color, const Point *pt1, const Point *pt2);
	void addEllipseRadius(int16_t y, int16_t x1, int16_t x2);
	void drawEllipse(uint8_t color, bool hasAlpha, const Point *pt, int16_t rx, int16_t ry);
	void fillArea(uint8_t color, bool hasAlpha);
	void drawSegment(uint8_t color, bool hasAlpha, int16_t ys, const Point *pts, uint8_t numPts);
	void drawPolygonOutline(uint8_t color, const Point *pts, uint8_t numPts);
	void drawPolygon(uint8_t color, bool hasAlpha, const Point *pts, uint8_t numPts);
};

#endif // GRAPHICS_H__
