
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
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
		MAX_VERTICES = 128,
		NUM_OPCODES = 15
	};

	enum {
		kTextJustifyLeft = 0,
		kTextJustifyAlign = 1,
		kTextJustifyCenter = 2,
	};

	enum {
		kCineDebut = 0,
		kCineChute = 47,
		kCineMemo = 48,
		kCineVoyage = 52,
		kCineEspions = 57
	};

	struct SetShape {
		uint16_t offset;
		uint16_t size;
	};

	struct Text {
		int num;
		const char *str;
	};

	static const OpcodeStub _opcodeTable[];
	static const char *const _namesTableDOS[];
	static const uint16_t _offsetsTableDOS[];
	static const uint16_t _offsetsTableAmiga[];
	static const uint8_t _amigaDemoOffsetsTable[];
	static const uint8_t _ssiOffsetsTable[];
	static const uint8_t _creditsDataDOS[];
	static const uint8_t _creditsDataAmiga[];
	static const uint8_t _creditsCutSeq[];
	static const uint8_t _musicTableDOS[];
	static const uint8_t _musicTableAmiga[];
	static const uint8_t _protectionShapeData[];
	static const Text _frTextsTable[];
	static const Text _enTextsTable[];
	static const uint8_t _caillouSetData[];
	static const uint8_t _memoSetShape2Data[];
	static const uint8_t _memoSetShape4Data[];

	Graphics _gfx;
	Resource *_res;
	SystemStub *_stub;
	Video *_vid;
	const uint8_t *_patchedOffsetsTable;

	uint16_t _id;
	uint16_t _deathCutsceneId;
	bool _interrupted;
	bool _stop;
	const uint8_t *_polPtr;
	const uint8_t *_cmdPtr;
	const uint8_t *_cmdPtrBak;
	uint32_t _tstamp;
	uint8_t _frameDelay;
	bool _newPal;
	uint8_t _palBuf[16 * sizeof(uint16_t) * 2];
	uint16_t _baseOffset;
	bool _creditsSequence;
	uint32_t _rotMat[4];
	uint8_t _primitiveColor;
	uint8_t _clearScreen;
	Point _vertices[MAX_VERTICES];
	bool _hasAlphaColor;
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
	bool _creditsSlowText;
	bool _creditsKeepText;
	uint8_t _creditsTextPosX;
	uint8_t _creditsTextPosY;
	int16_t _creditsTextCounter;
	int _creditsTextIndex; /* MAC has the credits data in a resource */
	int _creditsTextLen;
	uint8_t *_frontPage, *_backPage, *_auxPage;
	int _paletteNum;

	Cutscene(Resource *res, SystemStub *stub, Video *vid);

	const uint8_t *getCommandData() const;
	const uint8_t *getPolygonData() const;

	void sync(int delay);
	void copyPalette(const uint8_t *pal, uint16_t num);
	void updatePalette();
	void updateScreen();
	void setRotationTransform(uint16_t a, uint16_t b, uint16_t c);
	uint16_t findTextSeparators(const uint8_t *p, int len);
	void drawText(int16_t x, int16_t y, const uint8_t *p, uint16_t color, uint8_t *page, int textJustify);
	void clearBackPage();
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
	void op_drawCaptionText();
	void op_nop();
	void op_skip3();
	void op_refreshAll();
	void op_drawShapeScale();
	void op_drawShapeScaleRotate();
	void op_copyScreen();
	void op_drawTextAtPos();
	void op_handleKeys();

	uint8_t fetchNextCmdByte();
	uint16_t fetchNextCmdWord();
	void mainLoop(uint16_t num);
	bool load(uint16_t cutName);
	void unload();
	void prepare();
	void playCredits();
	void playText(const char *str);
	void play();

	void drawSetShape(const uint8_t *p, uint16_t offset, int x, int y, const uint8_t *paletteLut);
	void playSet(const uint8_t *p, int offset);
};

#endif // CUTSCENE_H__
