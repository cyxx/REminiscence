
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SYSTEMSTUB_H__
#define SYSTEMSTUB_H__

#include "intern.h"

struct PlayerInput {
	enum {
		DIR_UP    = 1 << 0,
		DIR_DOWN  = 1 << 1,
		DIR_LEFT  = 1 << 2,
		DIR_RIGHT = 1 << 3
	};
	enum {
		DF_FASTMODE = 1 << 0,
		DF_DBLOCKS  = 1 << 1,
		DF_SETLIFE  = 1 << 2
	};

	uint8_t dirMask;
	bool enter;
	bool space;
	bool shift;
	bool backspace;
	bool escape;

	char lastChar;

	bool save;
	bool load;
	int stateSlot;

	bool mirrorMode;

	uint8_t dbgMask;
	bool quit;
};

struct SystemStub {
	typedef void (*AudioCallback)(void *param, int16_t *stream, int len);

	PlayerInput _pi;

	virtual ~SystemStub() {}

	virtual void init(const char *title, int w, int h, int scaler, bool fullscreen) = 0;
	virtual void destroy() = 0;

	virtual void setScreenSize(int w, int h) = 0;
	virtual void setPalette(const uint8_t *pal, int n) = 0;
	virtual void setPaletteEntry(int i, const Color *c) = 0;
	virtual void getPaletteEntry(int i, Color *c) = 0;
	virtual void setOverscanColor(int i) = 0;
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) = 0;
	virtual void fadeScreen() = 0;
	virtual void updateScreen(int shakeOffset) = 0;

	virtual void processEvents() = 0;
	virtual void sleep(int duration) = 0;
	virtual uint32_t getTimeStamp() = 0;

	virtual void startAudio(AudioCallback callback, void *param) = 0;
	virtual void stopAudio() = 0;
	virtual uint32_t getOutputSampleRate() = 0;
	virtual void lockAudio() = 0;
	virtual void unlockAudio() = 0;
};

struct LockAudioStack {
	LockAudioStack(SystemStub *stub)
		: _stub(stub) {
		_stub->lockAudio();
	}
	~LockAudioStack() {
		_stub->unlockAudio();
	}
	SystemStub *_stub;
};

extern SystemStub *SystemStub_SDL_create();

#endif // SYSTEMSTUB_H__
