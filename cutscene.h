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

#ifndef CUTSCENE_H__
#define CUTSCENE_H__

#include "intern.h"
#include "graphics.h"

struct Resource;
struct SystemStub;
struct Video;

struct Cutscene {
	typedef void (Cutscene::*OpcodeStub)();

	enum {
		NUM_OPCODES = 15,
		TIMER_SLICE = 15
	};

	static const OpcodeStub _opcodeTable[];
	static const char *_namesTable[];
	static const uint16_t _offsetsTable[];
	static const uint16_t _cosTable[];
	static const uint16_t _sinTable[];
	static const uint8_t _creditsData[];
	static const uint16_t _creditsCutSeq[];
	static const uint8_t _musicTable[];
	static const uint8_t _protectionShapeData[];

	Graphics _gfx;
	Resource *_res;
	SystemStub *_stub;
	Video *_vid;

	uint16_t _id;
	uint16_t _deathCutsceneId;
	bool _interrupted;
	bool _stop;
	uint8_t *_polPtr;
	uint8_t *_cmdPtr;
	uint8_t *_cmdPtrBak;
	uint32_t _tstamp;
	uint8_t _frameDelay;
	bool _newPal;
	uint8_t _palBuf[0x20 * 2];
	uint16_t _startOffset;
	bool _creditsSequence;
	uint32_t _rotData[4];
	uint8_t _primitiveColor;
	uint8_t _clearScreen;
	Point _vertices[0x80];
	bool _hasAlphaColor;
	uint8_t _varText;
	uint8_t _varKey;
	int16_t _shape_ix;
	int16_t _shape_iy;
	int16_t _shape_ox;
	int16_t _shape_oy;
	int16_t _shape_cur_x;
	int16_t _shape_cur_y;
	int16_t _shape_prev_x;
	int16_t _shape_prev_y;
	uint16_t _shape_count;
	uint32_t _shape_cur_x16;
	uint32_t _shape_cur_y16;
	uint32_t _shape_prev_x16;
	uint32_t _shape_prev_y16;
	uint8_t _textSep[0x14];
	uint8_t _textBuf[500];
	const uint8_t *_textCurPtr;
	uint8_t *_textCurBuf;
	uint8_t _textUnk2;
	uint8_t _creditsTextPosX;
	uint8_t _creditsTextPosY;
	int16_t _creditsTextCounter;
	uint8_t *_page0, *_page1, *_pageC;

	Cutscene(Resource *res, SystemStub *stub, Video *vid);

	void sync();
	void copyPalette(const uint8_t *pal, uint16_t num);
	void updatePalette();
	void setPalette();
	void initRotationData(uint16_t a, uint16_t b, uint16_t c);
	uint16_t findTextSeparators(const uint8_t *p);
	void drawText(int16_t x, int16_t y, const uint8_t *p, uint16_t color, uint8_t *page, uint8_t n);
	void swapLayers();
	void drawCreditsText();
	void drawProtectionShape(uint8_t shapeNum, int16_t zoom);
	void drawShape(const uint8_t *data, int16_t x, int16_t y);
	void drawShapeScale(const uint8_t *data, int16_t zoom, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g);
	void drawShapeScaleRotate(const uint8_t *data, int16_t zoom, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g);

	void op_markCurPos();
	void op_refreshScreen();
	void op_waitForSync();
	void op_drawShape();
	void op_setPalette();
	void op_drawStringAtBottom();
	void op_nop();
	void op_skip3();
	void op_refreshAll();
	void op_drawShapeScale();
	void op_drawShapeScaleRotate();
	void op_drawCreditsText();
	void op_drawStringAtPos();
	void op_handleKeys();

	uint8_t fetchNextCmdByte();
	uint16_t fetchNextCmdWord();
	void mainLoop(uint16_t offset);
	void load(uint16_t cutName);
	void prepare();
	void startCredits();
	void play();
};

#endif // CUTSCENE_H__
