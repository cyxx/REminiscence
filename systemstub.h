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

	bool inpRecord;
	bool inpReplay;

	bool mirrorMode;

	uint8_t dbgMask;
	bool quit;
};

struct SystemStub {
	typedef void (*AudioCallback)(void *param, int8_t *stream, int len);

	PlayerInput _pi;

	virtual ~SystemStub() {}

	virtual void init(const char *title, int w, int h) = 0;
	virtual void destroy() = 0;

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
