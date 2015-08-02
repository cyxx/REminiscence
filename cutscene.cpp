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

#include "resource.h"
#include "systemstub.h"
#include "video.h"
#include "cutscene.h"


Cutscene::Cutscene(Resource *res, SystemStub *stub, Video *vid)
	: _res(res), _stub(stub), _vid(vid) {
	memset(_palBuf, 0, sizeof(_palBuf));
}

void Cutscene::sync() {
	// XXX input handling
	if (!(_stub->_pi.dbgMask & PlayerInput::DF_FASTMODE)) {
		int32_t delay = _stub->getTimeStamp() - _tstamp;
		int32_t pause = _frameDelay * TIMER_SLICE - delay;
		if (pause > 0) {
			_stub->sleep(pause);
		}
	}
	_tstamp = _stub->getTimeStamp();
}

void Cutscene::copyPalette(const uint8_t *pal, uint16_t num) {
	uint8_t *dst = (uint8_t *)_palBuf;
	if (num != 0) {
		dst += 0x20;
	}
	memcpy(dst, pal, 0x20);
	_newPal = true;
}

void Cutscene::updatePalette() {
	if (_newPal) {
		const uint8_t *p = _palBuf;
		for (int i = 0; i < 32; ++i) {
			uint16_t color = READ_BE_UINT16(p); p += 2;
			uint8_t t = (color == 0) ? 0 : 3;
			Color c;
			c.r = ((color & 0xF00) >> 6) | t;
			c.g = ((color & 0x0F0) >> 2) | t;
			c.b = ((color & 0x00F) << 2) | t;
			_stub->setPaletteEntry(0xC0 + i, &c);
		}
		_newPal = false;
	}
}

void Cutscene::setPalette() {
	sync();
	updatePalette();
	SWAP(_page0, _page1);
	_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _page0, 256);
	_stub->updateScreen(0);
}

void Cutscene::initRotationData(uint16_t a, uint16_t b, uint16_t c) {
	int16_t n1 = _sinTable[a];
	int16_t n2 = _cosTable[a];
	int16_t n3 = _sinTable[c];
	int16_t n4 = _cosTable[c];
	int16_t n5 = _sinTable[b];
	int16_t n6 = _cosTable[b];
	_rotData[0] = ((n2 * n6) >> 8) - ((((n4 * n1) >> 8) * n5) >> 8);
	_rotData[1] = ((n1 * n6) >> 8) + ((((n4 * n2) >> 8) * n5) >> 8);
	_rotData[2] = ( n3 * n1) >> 8;
	_rotData[3] = (-n3 * n2) >> 8;
}

uint16_t Cutscene::findTextSeparators(const uint8_t *p) {
	uint8_t *q = _textSep;
	uint16_t ret = 0;
	uint16_t pos = 0;
	for (; *p != 0xA; ++p) {
		if (*p == 0x7C) {
			*q++ = pos;
			if (pos > ret) {
				ret = pos;
			}
			pos = 0;
		} else {
			++pos;
		}
	}
	*q++ = pos;
	if (pos > ret) {
		ret = pos;
	}
	*q++ = 0;
	return ret;
}

void Cutscene::drawText(int16_t x, int16_t y, const uint8_t *p, uint16_t color, uint8_t *page, uint8_t n) {
	debug(DBG_CUT, "Cutscene::drawText(x=%d, y=%d, c=%d)", x, y, color);
	uint16_t last_sep = 0;
	if (n != 0) {
		last_sep = findTextSeparators(p);
		if (n != 2) {
			last_sep = 30;
		}
	}
	const uint8_t *sep = _textSep;
	y += 50;
	x += 8;
	int16_t yy = y;
	int16_t xx = x;
	if (n != 0) {
		xx += ((last_sep - *sep++) & 0xFE) * 4;
	}
	for (; *p != 0xA; ++p) {
		if (*p == 0x7C) {
			yy += 8;
			xx = x;
			if (n != 0) {
				xx += ((last_sep - *sep++) & 0xFE) * 4;
			}
		} else if (*p == 0x20) {
			xx += 8;
		} else {
			uint8_t *dst_char = page + 256 * yy + xx;
			const uint8_t *src = _res->_fnt + (*p - 32) * 32;
			for (int h = 0; h < 8; ++h) {
				for (int w = 0; w < 4; ++w) {
					uint8_t c1 = (*src & 0xF0) >> 4;
					uint8_t c2 = (*src & 0x0F) >> 0;
					++src;
					if (c1 != 0) {
						*dst_char = (c1 == 0xF) ? color : (0xE0 + c1);
					}
					++dst_char;
					if (c2 != 0) {
						*dst_char = (c2 == 0xF) ? color : (0xE0 + c2);
					}
					++dst_char;
				}
				dst_char += 256 - 8;
			}
			xx += 8;
		}
	}
}

void Cutscene::swapLayers() {
	if (_clearScreen == 0) {
		memcpy(_page1, _pageC, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
	} else {
		memset(_page1, 0xC0, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
	}
}

void Cutscene::drawCreditsText() {
	if (_creditsSequence) {
		if (_textUnk2 != 0) {
			if (_varText == 0) {
				_textUnk2 = 0;
			} else {
				return;
			}
		}
		if (_creditsTextCounter <= 0) { // XXX
			uint8_t code = *_textCurPtr;
			if (code == 0xFF) {
				_textBuf[0] = 0xA;
//				_cut_status = 0;
			} else if (code == 0xFE) {
				++_textCurPtr;
				code = *_textCurPtr++;
				_creditsTextCounter = code;
			} else if (code == 1) {
				++_textCurPtr;
				_creditsTextPosX = *_textCurPtr++;
				_creditsTextPosY = *_textCurPtr++;
			} else if (code == 0) {
				_textCurBuf = _textBuf;
				_textBuf[0] = 0xA;
				++_textCurPtr;
				if (_varText != 0) {
					_textUnk2 = 0xFF;
				}
			} else {
				*_textCurBuf++ = code;
				_textCurBuf = _textCurBuf;
				*_textCurBuf = 0xA;
				++_textCurPtr;
			}
		} else {
			_creditsTextCounter -= 10; // XXX adjust
		}
		drawText((_creditsTextPosX - 1) * 8, _creditsTextPosY * 8, _textBuf, 0xEF, _page1, 0);
	}
}

void Cutscene::drawProtectionShape(uint8_t shapeNum, int16_t zoom) {
	debug(DBG_CUT, "Cutscene::drawProtectionShape() shapeNum = %d", shapeNum);
	_shape_ix = 64;
	_shape_iy = 64;
	_shape_count = 0;

	int16_t x = 0;
	int16_t y = 0;
	zoom += 512;
	initRotationData(0, 180, 90);

	const uint8_t *shapeOffsetTable    = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x02);
	const uint8_t *shapeDataTable      = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x0E);
	const uint8_t *verticesOffsetTable = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x0A);
	const uint8_t *verticesDataTable   = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x12);

	++shapeNum;
	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeNum & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	while (primitiveCount--) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
		const uint8_t *p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData); shapeData += 2;
			dy = READ_BE_UINT16(shapeData); shapeData += 2;
		}
		_hasAlphaColor = (verticesOffset & 0x4000) != 0;
		_primitiveColor = 0xC0 + *shapeData++;
		drawShapeScaleRotate(p, zoom, dx, dy, x, y, 0, 0);
		++_shape_count;
	}
}

void Cutscene::op_markCurPos() {
	debug(DBG_CUT, "Cutscene::op_markCurPos()");
	_cmdPtrBak = _cmdPtr;
	drawCreditsText();
	_frameDelay = 5;
	setPalette();
	swapLayers();
	_varText = 0;
}

void Cutscene::op_refreshScreen() {
	debug(DBG_CUT, "Cutscene::op_refreshScreen()");
	_clearScreen = fetchNextCmdByte();
	if (_clearScreen != 0) {
		swapLayers();
		_varText = 0;
	}
}

void Cutscene::op_waitForSync() {
	debug(DBG_CUT, "Cutscene::op_waitForSync()");
	if (_creditsSequence) {
		uint16_t n = fetchNextCmdByte() * 2;
		do {
			_varText = 0xFF;
			_frameDelay = 3;
			if (_textBuf == _textCurBuf) {
				_creditsTextCounter = 20;
			}
			memcpy(_page1, _page0, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
			drawCreditsText();
			setPalette();
		} while (--n);
		swapLayers();
		_varText = 0;
	} else {
		_frameDelay = fetchNextCmdByte() * 4;
		sync(); // XXX handle input
	}
}

void Cutscene::drawShape(const uint8_t *data, int16_t x, int16_t y) {
	debug(DBG_CUT, "Cutscene::drawShape()");
	_gfx._layer = _page1;
	uint8_t numVertices = *data++;
	if (numVertices & 0x80) {
		Point pt;
		pt.x = READ_BE_UINT16(data) + x; data += 2;
		pt.y = READ_BE_UINT16(data) + y; data += 2;
		uint16_t rx = READ_BE_UINT16(data); data += 2;
		uint16_t ry = READ_BE_UINT16(data); data += 2;
		_gfx.drawEllipse(_primitiveColor, _hasAlphaColor, &pt, rx, ry);
	} else if (numVertices == 0) {
		Point pt;
		pt.x = READ_BE_UINT16(data) + x; data += 2;
		pt.y = READ_BE_UINT16(data) + y; data += 2;
		_gfx.drawPoint(_primitiveColor, &pt);
	} else {
		Point *pt = _vertices;
		int16_t ix = READ_BE_UINT16(data); data += 2;
		int16_t iy = READ_BE_UINT16(data); data += 2;
		pt->x = ix + x;
		pt->y = iy + y;
		++pt;
		int16_t n = numVertices - 1;
		++numVertices;
		for (; n >= 0; --n) {
			int16_t dx = (int8_t)*data++;
			int16_t dy = (int8_t)*data++;
			if (dy == 0 && n != 0 && *(data + 1) == 0) {
				ix += dx;
				--numVertices;
			} else {
				ix += dx;
				iy += dy;
				pt->x = ix + x;
				pt->y = iy + y;
				++pt;
			}
		}
		_gfx.drawPolygon(_primitiveColor, _hasAlphaColor, _vertices, numVertices);
	}
}

void Cutscene::op_drawShape() {
	debug(DBG_CUT, "Cutscene::op_drawShape()");

	int16_t x = 0;
	int16_t y = 0;
	uint16_t shapeOffset = fetchNextCmdWord();
	if (shapeOffset & 0x8000) {
		x = fetchNextCmdWord();
		y = fetchNextCmdWord();
	}

	const uint8_t *shapeOffsetTable    = _polPtr + READ_BE_UINT16(_polPtr + 0x02);
	const uint8_t *shapeDataTable      = _polPtr + READ_BE_UINT16(_polPtr + 0x0E);
	const uint8_t *verticesOffsetTable = _polPtr + READ_BE_UINT16(_polPtr + 0x0A);
	const uint8_t *verticesDataTable   = _polPtr + READ_BE_UINT16(_polPtr + 0x12);

	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeOffset & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	while (primitiveCount--) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
		const uint8_t *primitiveVertices = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData); shapeData += 2;
			dy = READ_BE_UINT16(shapeData); shapeData += 2;
		}
		_hasAlphaColor = (verticesOffset & 0x4000) != 0;
		uint8_t color = *shapeData++;
		if (_clearScreen == 0) {
			color += 0x10;
		}
		_primitiveColor = 0xC0 + color;
		drawShape(primitiveVertices, x + dx, y + dy);
	}
	if (_clearScreen != 0) {
		memcpy(_pageC, _page1, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
	}
}

void Cutscene::op_setPalette() {
	debug(DBG_CUT, "Cutscene::op_setPalette()");
	uint8_t num = fetchNextCmdByte();
	uint8_t palNum = fetchNextCmdByte();
	uint16_t off = READ_BE_UINT16(_polPtr + 6);
	const uint8_t *p = _polPtr + off + num * 32;
	copyPalette(p, palNum ^ 1);
	if (_creditsSequence) {
		_palBuf[0x20] = 0x0F;
		_palBuf[0x21] = 0xFF;
	}
}

void Cutscene::op_drawStringAtBottom() {
	debug(DBG_CUT, "Cutscene::op_drawStringAtBottom()");
	uint16_t strId = fetchNextCmdWord();
	if (!_creditsSequence) {
		memset(_pageC + 179 * 256, 0xC0, 45 * 256);
		memset(_page1 + 179 * 256, 0xC0, 45 * 256);
		memset(_page0 + 179 * 256, 0xC0, 45 * 256);
		if (strId != 0xFFFF) {
			const uint8_t *str = _res->getCineString(strId);
			if (str) {
				drawText(0, 129, str, 0xEF, _page1, 1);
				drawText(0, 129, str, 0xEF, _pageC, 1);
			}
		}
	}
}

void Cutscene::op_nop() {
	debug(DBG_CUT, "Cutscene::op_nop()");
}

void Cutscene::op_skip3() {
	debug(DBG_CUT, "Cutscene::op_skip3()");
	_cmdPtr += 3;
}

void Cutscene::op_refreshAll() {
	debug(DBG_CUT, "Cutscene::op_refreshAll()");
	_frameDelay = 5;
	setPalette();
	swapLayers();
	_varText = 0xFF;
	op_handleKeys();
}

void Cutscene::drawShapeScale(const uint8_t *data, int16_t zoom, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g) {
	debug(DBG_CUT, "Cutscene::drawShapeScale(%d, %d, %d, %d, %d, %d, %d)", zoom, b, c, d, e, f, g);
	_gfx._layer = _page1;
	uint8_t numVertices = *data++;
	if (numVertices & 0x80) {
		int16_t x, y;
		Point *pt = _vertices;
		Point pr[2];
		_shape_cur_x = b + READ_BE_UINT16(data); data += 2;
		_shape_cur_y = c + READ_BE_UINT16(data); data += 2;
		x = READ_BE_UINT16(data); data += 2;
		y = READ_BE_UINT16(data); data += 2;
		_shape_cur_x16 = 0;
		_shape_cur_y16 = 0;
		pr[0].x =  0;
		pr[0].y = -y;
		pr[1].x = -x;
		pr[1].y =  y;
		if (_shape_count == 0) {
			f -= ((((_shape_ix - _shape_ox) * zoom) * 128) + 0x8000) >> 16;
			g -= ((((_shape_iy - _shape_oy) * zoom) * 128) + 0x8000) >> 16;
			pt->x = f;
			pt->y = g;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((_shape_cur_x - _shape_prev_x) * zoom) * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			_shape_cur_y16 = _shape_prev_y16 + ((_shape_cur_y - _shape_prev_y) * zoom) * 128;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		for (int i = 0; i < 2; ++i) {
			_shape_cur_x += pr[i].x;
			_shape_cur_x16 += pr[i].x * zoom * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			_shape_cur_y += pr[i].y;
			_shape_cur_y16 += pr[i].y * zoom * 128;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		Point po;
		po.x = _vertices[0].x + d + _shape_ix;
		po.y = _vertices[0].y + e + _shape_iy;
		int16_t rx = _vertices[0].x - _vertices[2].x;
		int16_t ry = _vertices[0].y - _vertices[1].y;
		_gfx.drawEllipse(_primitiveColor, _hasAlphaColor, &po, rx, ry);
	} else if (numVertices == 0) {
		Point pt;
 		pt.x = _shape_cur_x = b + READ_BE_UINT16(data); data += 2;
	 	pt.y = _shape_cur_y = c + READ_BE_UINT16(data); data += 2;
 		if (_shape_count == 0) {
			f -= ((((_shape_ix - pt.x) * zoom) * 128) + 0x8000) >> 16;
			g -= ((((_shape_iy - pt.y) * zoom) * 128) + 0x8000) >> 16;
			pt.x = f + _shape_ix + d;
			pt.y = g + _shape_iy + e;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((pt.x - _shape_prev_x) * zoom) * 128;
			_shape_cur_y16 = _shape_prev_y16 + ((pt.y - _shape_prev_y) * zoom) * 128;
			pt.x = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
			pt.y = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		_gfx.drawPoint(_primitiveColor, &pt);
	} else {
		Point *pt = _vertices;
		int16_t ix, iy;
		_shape_cur_x = ix = READ_BE_UINT16(data) + b; data += 2;
		_shape_cur_y = iy = READ_BE_UINT16(data) + c; data += 2;
		if (_shape_count == 0) {
			f -= ((((_shape_ix - _shape_ox) * zoom) * 128) + 0x8000) >> 16;
			g -= ((((_shape_iy - _shape_oy) * zoom) * 128) + 0x8000) >> 16;
			pt->x = f + _shape_ix + d;
			pt->y = g + _shape_iy + e;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((_shape_cur_x - _shape_prev_x) * zoom) * 128;
			_shape_cur_y16 = _shape_prev_y16 + ((_shape_cur_y - _shape_prev_y) * zoom) * 128;
			pt->x = ix = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
			pt->y = iy = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
			++pt;
		}
		int16_t n = numVertices - 1;
		++numVertices;
		int16_t sx = 0;
		for (; n >= 0; --n) {
			ix = (int8_t)(*data++) + sx;
			iy = (int8_t)(*data++);
			if (iy == 0 && n != 0 && *(data + 1) == 0) {
				sx = ix;
				--numVertices;
			} else {
				sx = 0;
				_shape_cur_x += ix;
				_shape_cur_y += iy;
				_shape_cur_x16 += ix * zoom * 128;
				_shape_cur_y16 += iy * zoom * 128;
				pt->x = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
				pt->y = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
				++pt;
			}
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		_gfx.drawPolygon(_primitiveColor, _hasAlphaColor, _vertices, numVertices);
	}
}

void Cutscene::op_drawShapeScale() {
	debug(DBG_CUT, "Cutscene::op_drawShapeScale()");

	_shape_count = 0;

	int16_t x = 0;
	int16_t y = 0;
	uint16_t shapeOffset = fetchNextCmdWord();
	if (shapeOffset & 0x8000) {
		x = fetchNextCmdWord();
		y = fetchNextCmdWord();
	}

	uint16_t zoom = fetchNextCmdWord() + 512;
	_shape_ix = fetchNextCmdByte();
	_shape_iy = fetchNextCmdByte();

	const uint8_t *shapeOffsetTable    = _polPtr + READ_BE_UINT16(_polPtr + 0x02);
	const uint8_t *shapeDataTable      = _polPtr + READ_BE_UINT16(_polPtr + 0x0E);
	const uint8_t *verticesOffsetTable = _polPtr + READ_BE_UINT16(_polPtr + 0x0A);
	const uint8_t *verticesDataTable   = _polPtr + READ_BE_UINT16(_polPtr + 0x12);

	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeOffset & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	if (primitiveCount != 0) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData + 2);
			dy = READ_BE_UINT16(shapeData + 4);
		}
		const uint8_t *p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2) + 1;
		_shape_ox = READ_BE_UINT16(p) + dx; p += 2;
		_shape_oy = READ_BE_UINT16(p) + dy; p += 2;
		while (primitiveCount--) {
			verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
			p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
			dx = 0;
			dy = 0;
			if (verticesOffset & 0x8000) {
				dx = READ_BE_UINT16(shapeData); shapeData += 2;
				dy = READ_BE_UINT16(shapeData); shapeData += 2;
			}
			_hasAlphaColor = (verticesOffset & 0x4000) != 0;
			uint8_t color = *shapeData++;
			if (_clearScreen == 0) {
				color += 0x10; // 2nd pal buf
			}
			_primitiveColor = 0xC0 + color;
			drawShapeScale(p, zoom, dx, dy, x, y, 0, 0);
			++_shape_count;
		}
	}
}

void Cutscene::drawShapeScaleRotate(const uint8_t *data, int16_t zoom, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g) {
	debug(DBG_CUT, "Cutscene::drawShapeScaleRotate(%d, %d, %d, %d, %d, %d, %d)", zoom, b, c, d, e, f, g);
	_gfx._layer = _page1;
	uint8_t numVertices = *data++;
	if (numVertices & 0x80) {
		int16_t x, y, ix, iy;
		Point pr[2];
		Point *pt = _vertices;
		_shape_cur_x = ix = b + READ_BE_UINT16(data); data += 2;
		_shape_cur_y = iy = c + READ_BE_UINT16(data); data += 2;
		x = READ_BE_UINT16(data); data += 2;
		y = READ_BE_UINT16(data); data += 2;
		_shape_cur_x16 = _shape_ix - ix;
		_shape_cur_y16 = _shape_iy - iy;
		_shape_ox = _shape_cur_x = _shape_ix + ((_shape_cur_x16 * _rotData[0] + _shape_cur_y16 * _rotData[1]) >> 8);
		_shape_oy = _shape_cur_y = _shape_iy + ((_shape_cur_x16 * _rotData[2] + _shape_cur_y16 * _rotData[3]) >> 8);
		pr[0].x =  0;
		pr[0].y = -y;
		pr[1].x = -x;
		pr[1].y =  y;
		if (_shape_count == 0) {
			f -= ((_shape_ix - _shape_cur_x) * zoom * 128 + 0x8000) >> 16;
			g -= ((_shape_iy - _shape_cur_y) * zoom * 128 + 0x8000) >> 16;
			pt->x = f;
			pt->y = g;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + (_shape_cur_x - _shape_prev_x) * zoom * 128;
			_shape_cur_y16 = _shape_prev_y16 + (_shape_cur_y - _shape_prev_y) * zoom * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		for (int i = 0; i < 2; ++i) {
			_shape_cur_x += pr[i].x;
			_shape_cur_x16 += pr[i].x * zoom * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			_shape_cur_y += pr[i].y;
			_shape_cur_y16 += pr[i].y * zoom * 128;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		Point po;
		po.x = _vertices[0].x + d + _shape_ix;
		po.y = _vertices[0].y + e + _shape_iy;
		int16_t rx = _vertices[0].x - _vertices[2].x;
		int16_t ry = _vertices[0].y - _vertices[1].y;
		_gfx.drawEllipse(_primitiveColor, _hasAlphaColor, &po, rx, ry);
	} else if (numVertices == 0) {
		Point pt;
		pt.x = b + READ_BE_UINT16(data); data += 2;
		pt.y = c + READ_BE_UINT16(data); data += 2;
		_shape_cur_x16 = _shape_ix - pt.x;
		_shape_cur_y16 = _shape_iy - pt.y;
		_shape_cur_x = _shape_ix + ((_rotData[0] * _shape_cur_x16 + _rotData[1] * _shape_cur_y16) >> 8);
		_shape_cur_y = _shape_iy + ((_rotData[2] * _shape_cur_x16 + _rotData[3] * _shape_cur_y16) >> 8);
		if (_shape_count != 0) {
			_shape_cur_x16 = _shape_prev_x16 + (_shape_cur_x - _shape_prev_x) * zoom * 128;
			pt.x = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
			_shape_cur_y16 = _shape_prev_y16 + (_shape_cur_y - _shape_prev_y) * zoom * 128;
			pt.y = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
		} else {
			f -= (((_shape_ix - _shape_cur_x) * zoom * 128) + 0x8000) >> 16;
			g -= (((_shape_iy - _shape_cur_y) * zoom * 128) + 0x8000) >> 16;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
			pt.x = f + _shape_ix + d;
			pt.y = g + _shape_iy + e;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		_gfx.drawPoint(_primitiveColor, &pt);
	} else {
		int16_t x, y, a, shape_last_x, shape_last_y;
		Point tempVertices[40];
		_shape_cur_x = b + READ_BE_UINT16(data); data += 2;
		x = _shape_cur_x;
		_shape_cur_y = c + READ_BE_UINT16(data); data += 2;
		y = _shape_cur_y;
		_shape_cur_x16 = _shape_ix - x;
		_shape_cur_y16 = _shape_iy - y;

		a = _shape_ix + ((_rotData[0] * _shape_cur_x16 + _rotData[1] * _shape_cur_y16) >> 8);
		if (_shape_count == 0) {
			_shape_ox = a;
		}
		_shape_cur_x = shape_last_x = a;
		a = _shape_iy + ((_rotData[2] * _shape_cur_x16 + _rotData[3] * _shape_cur_y16) >> 8);
		if (_shape_count == 0) {
			_shape_oy = a;
		}
		_shape_cur_y = shape_last_y = a;

		int16_t ix = x;
		int16_t iy = y;
		Point *pt2 = tempVertices;
		int16_t sx = 0;
		for (int16_t n = numVertices - 1; n >= 0; --n) {
			x = (int8_t)(*data++) + sx;
			y = (int8_t)(*data++);
			if (y == 0 && n != 0 && *(data + 1) == 0) {
				sx = x;
				--numVertices;
			} else {
				ix += x;
				iy += y;
				sx = 0;
				_shape_cur_x16 = _shape_ix - ix;
				_shape_cur_y16 = _shape_iy - iy;
				a = _shape_ix + ((_rotData[0] * _shape_cur_x16 + _rotData[1] * _shape_cur_y16) >> 8);
				pt2->x = a - shape_last_x;
				shape_last_x = a;
				a = _shape_iy + ((_rotData[2] * _shape_cur_x16 + _rotData[3] * _shape_cur_y16) >> 8);
				pt2->y = a - shape_last_y;
				shape_last_y = a;
				++pt2;
			}
		}
		Point *pt = _vertices;
		if (_shape_count == 0) {
			ix = _shape_ox;
			iy = _shape_oy;
			f -= (((_shape_ix - ix) * zoom * 128) + 0x8000) >> 16;
			g -= (((_shape_iy - iy) * zoom * 128) + 0x8000) >> 16;
			pt->x = f + _shape_ix + d;
			pt->y = g + _shape_iy + e;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((_shape_cur_x - _shape_prev_x) * zoom * 128);
			pt->x = _shape_ix + d + ((_shape_cur_x16 + 0x8000) >> 16);
			_shape_cur_y16 = _shape_prev_y16 + ((_shape_cur_y - _shape_prev_y) * zoom * 128);
			pt->y = _shape_iy + e + ((_shape_cur_y16 + 0x8000) >> 16);
			++pt;
		}
		for (int i = 0; i < numVertices; ++i) {
			_shape_cur_x += tempVertices[i].x;
			_shape_cur_x16 += tempVertices[i].x * zoom * 128;
			pt->x = d + _shape_ix + ((_shape_cur_x16 + 0x8000) >> 16);
			_shape_cur_y += tempVertices[i].y;
			_shape_cur_y16 += tempVertices[i].y * zoom * 128;
			pt->y = e + _shape_iy + ((_shape_cur_y16 + 0x8000) >> 16);
			++pt;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		_gfx.drawPolygon(_primitiveColor, _hasAlphaColor, _vertices, numVertices + 1);
	}
}

void Cutscene::op_drawShapeScaleRotate() {
	debug(DBG_CUT, "Cutscene::op_drawShapeScaleRotate()");

	_shape_count = 0;

	int16_t x = 0;
	int16_t y = 0;
	uint16_t shapeOffset = fetchNextCmdWord();
	if (shapeOffset & 0x8000) {
		x = fetchNextCmdWord();
		y = fetchNextCmdWord();
	}

	uint16_t zoom = 512;
	if (shapeOffset & 0x4000) {
		zoom += fetchNextCmdWord();
	}
	_shape_ix = fetchNextCmdByte();
	_shape_iy = fetchNextCmdByte();

	uint16_t r1, r2, r3;
	r1 = fetchNextCmdWord();
	r2 = 180;
	if (shapeOffset & 0x2000) {
		r2 = fetchNextCmdWord();
	}
	r3 = 90;
	if (shapeOffset & 0x1000) {
		r3 = fetchNextCmdWord();
	}
	initRotationData(r1, r2, r3);

	const uint8_t *shapeOffsetTable    = _polPtr + READ_BE_UINT16(_polPtr + 0x02);
	const uint8_t *shapeDataTable      = _polPtr + READ_BE_UINT16(_polPtr + 0x0E);
	const uint8_t *verticesOffsetTable = _polPtr + READ_BE_UINT16(_polPtr + 0x0A);
	const uint8_t *verticesDataTable   = _polPtr + READ_BE_UINT16(_polPtr + 0x12);

	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeOffset & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	while (primitiveCount--) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
		const uint8_t *p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData); shapeData += 2;
			dy = READ_BE_UINT16(shapeData); shapeData += 2;
		}
		_hasAlphaColor = (verticesOffset & 0x4000) != 0;
		uint8_t color = *shapeData++;
		if (_clearScreen == 0) {
			color += 0x10; // 2nd pal buf
		}
		_primitiveColor = 0xC0 + color;
		drawShapeScaleRotate(p, zoom, dx, dy, x, y, 0, 0);
		++_shape_count;
	}
}

void Cutscene::op_drawCreditsText() {
	debug(DBG_CUT, "Cutscene::op_drawCreditsText()");
	_varText = 0xFF;
	if (_textCurBuf == _textBuf) {
		++_creditsTextCounter;
	}
	memcpy(_page1, _page0, Video::GAMESCREEN_W * Video::GAMESCREEN_H);
	_frameDelay = 10;
	setPalette();
}

void Cutscene::op_drawStringAtPos() {
	debug(DBG_CUT, "Cutscene::op_drawStringAtPos()");
	uint16_t strId = fetchNextCmdWord();
	if (strId != 0xFFFF) {
		int16_t x = (int8_t)fetchNextCmdByte() * 8;
		int16_t y = (int8_t)fetchNextCmdByte() * 8;
		if (!_creditsSequence) {
			const uint8_t *str = _res->getCineString(strId & 0xFFF);
			if (str) {
				uint8_t color = 0xD0 + (strId >> 0xC);
				drawText(x, y, str, color, _page1, 2);
			}
			// workaround for buggy cutscene script
			if (_id == 0x34 && (strId & 0xFFF) == 0x45) {
				if ((_cmdPtr - _cmdPtrBak) == 0xA) {
					_stub->copyRect(0, 0, Video::GAMESCREEN_W, Video::GAMESCREEN_H, _page1, 256);
					_stub->updateScreen(0);
				} else {
					_stub->sleep(15);
				}
			}
		}
	}
}

void Cutscene::op_handleKeys() {
	debug(DBG_CUT, "Cutscene::op_handleKeys()");
	while (1) {
		uint8_t key_mask = fetchNextCmdByte();
		if (key_mask == 0xFF) {
			return;
		}
		bool b = true;
		switch (key_mask) {
		case 1:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_UP) != 0;
			break;
		case 2:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) != 0;
			break;
		case 4:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) != 0;
			break;
		case 8:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) != 0;
			break;
		case 0x80:
			b = _stub->_pi.space || _stub->_pi.enter || _stub->_pi.shift;
			break;
		}
		if (b) {
			break;
		}
		_cmdPtr += 2;
	}
	_stub->_pi.dirMask = 0;
	_stub->_pi.enter = false;
	_stub->_pi.space = false;
	_stub->_pi.shift = false;
	int16_t n = fetchNextCmdWord();
	if (n < 0) {
		n = -n - 1;
		if (_varKey == 0) {
			_stop = true;
			return;
		}
		if (_varKey != n) {
			_cmdPtr = _cmdPtrBak;
			return;
		}
		_varKey = 0;
		--n;
		_cmdPtr = _res->_cmd;
		n = READ_BE_UINT16(_cmdPtr + n * 2 + 2);
	}
	_cmdPtr = _cmdPtrBak = _res->_cmd + n + _startOffset;
}

uint8_t Cutscene::fetchNextCmdByte() {
	return *_cmdPtr++;
}

uint16_t Cutscene::fetchNextCmdWord() {
	uint16_t i = READ_BE_UINT16(_cmdPtr);
	_cmdPtr += 2;
	return i;
}

void Cutscene::mainLoop(uint16_t offset) {
	_frameDelay = 5;
	_tstamp = _stub->getTimeStamp();

	Color c;
	c.r = c.g = c.b = 0;
	for (int i = 0; i < 0x20; ++i) {
		_stub->setPaletteEntry(0xC0 + i, &c);
	}
	_newPal = false;
	_hasAlphaColor = false;
	uint8_t *p = _res->_cmd;
	if (offset != 0) {
		offset = READ_BE_UINT16(p + (offset + 1) * 2);
	}
	_startOffset = (READ_BE_UINT16(p) + 1) * 2;
	_varKey = 0;
	_cmdPtr = _cmdPtrBak = p + _startOffset + offset;
	_polPtr = _res->_pol;
	debug(DBG_CUT, "_startOffset = %d offset = %d", _startOffset, offset);

	while (!_stub->_pi.quit && !_interrupted && !_stop) {
		uint8_t op = fetchNextCmdByte();
		debug(DBG_CUT, "Cutscene::play() opcode = 0x%X (%d)", op, (op >> 2));
		if (op & 0x80) {
			break;
		}
		op >>= 2;
		if (op >= NUM_OPCODES) {
			error("Invalid cutscene opcode = 0x%02X", op);
		}
		(this->*_opcodeTable[op])();
		_stub->processEvents();
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			_interrupted = true;
		}
	}
}

void Cutscene::load(uint16_t cutName) {
	assert(cutName != 0xFFFF);
	const char *name = _namesTable[cutName & 0xFF];
	switch (_res->_type) {
	case kResourceTypeAmiga:
		if (strncmp(name, "INTRO", 5) == 0) {
			name = "INTRO";
		}
		_res->load(name, Resource::OT_CMP);
		break;
	case kResourceTypePC:
		_res->load(name, Resource::OT_CMD);
		_res->load(name, Resource::OT_POL);
		_res->load_CINE();
		break;
	}
}

void Cutscene::prepare() {
	_page0 = _vid->_frontLayer;
	_page1 = _vid->_tempLayer;
	_pageC = _vid->_tempLayer2;
	_stub->_pi.dirMask = 0;
	_stub->_pi.enter = false;
	_stub->_pi.space = false;
	_stub->_pi.shift = false;
	_interrupted = false;
	_stop = false;
	_gfx.setClippingRect(8, 50, 240, 128);
}

void Cutscene::startCredits() {
	_textCurPtr = _creditsData;
	_textBuf[0] = 0xA;
	_textCurBuf = _textBuf;
	_creditsSequence = true;
//	_cut_status = 1;
	_varText = 0;
	_textUnk2 = 0;
	_creditsTextCounter = 0;
	_interrupted = false;
	const uint16_t *cut_seq = _creditsCutSeq;
	while (!_stub->_pi.quit && !_interrupted) {
		uint16_t cut_id = *cut_seq++;
		if (cut_id == 0xFFFF) {
			break;
		}
		prepare();
		uint16_t cutName = _offsetsTable[cut_id * 2 + 0];
		uint16_t cutOff  = _offsetsTable[cut_id * 2 + 1];
		load(cutName);
		mainLoop(cutOff);
	}
	_creditsSequence = false;
}

void Cutscene::play() {
	if (_id != 0xFFFF) {
		_textCurBuf = NULL;
		debug(DBG_CUT, "Cutscene::play() _id=0x%X", _id);
		_creditsSequence = false;
		prepare();
		uint16_t cutName = _offsetsTable[_id * 2 + 0];
		uint16_t cutOff  = _offsetsTable[_id * 2 + 1];
		if (cutName != 0xFFFF) {
			load(cutName);
			mainLoop(cutOff);
		}
		_vid->fullRefresh();
		if (_id != 0x3D) {
			_id = 0xFFFF;
		}
	}
}
