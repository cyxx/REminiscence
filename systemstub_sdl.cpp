
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include "scaler.h"
#include "systemstub.h"
#include "util.h"

struct SystemStub_SDL : SystemStub {
	enum {
		MAX_BLIT_RECTS = 200,
		SOUND_SAMPLE_RATE = 22050,
		JOYSTICK_COMMIT_VALUE = 3200
	};

	uint16_t *_screenBuffer;
	uint16_t *_fadeScreenBuffer;
	SDL_Surface *_screenSurface;
	bool _fullscreen;
	int _currentScaler;
	uint8_t _overscanColor;
	uint16_t _pal[256];
	int _screenW, _screenH;
	SDL_Joystick *_joystick;
	SDL_Rect _blitRects[MAX_BLIT_RECTS];
	int _numBlitRects;
	bool _fadeOnUpdateScreen;
	void (*_audioCbProc)(void *, int8_t *, int);
	void *_audioCbData;
	int _screenshot;

	virtual ~SystemStub_SDL() {}
	virtual void init(const char *title, int w, int h, int scaler, bool fullscreen);
	virtual void destroy();
	virtual void setScreenSize(int w, int h);
	virtual void setPalette(const uint8_t *pal, int n);
	virtual void setPaletteEntry(int i, const Color *c);
	virtual void getPaletteEntry(int i, Color *c);
	virtual void setOverscanColor(int i);
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch);
	virtual void fadeScreen();
	virtual void updateScreen(int shakeOffset);
	virtual void processEvents();
	virtual void sleep(int duration);
	virtual uint32_t getTimeStamp();
	virtual void startAudio(AudioCallback callback, void *param);
	virtual void stopAudio();
	virtual uint32_t getOutputSampleRate();
	virtual void lockAudio();
	virtual void unlockAudio();

	void processEvent(const SDL_Event &ev, bool &paused);
	void prepareGfxMode();
	void cleanupGfxMode();
	void switchGfxMode(bool fullscreen, uint8_t scaler);
	void flipGfx();
	void forceGfxRedraw();
	void drawRect(SDL_Rect *rect, uint8_t color, uint16_t *dst, uint16_t dstPitch);
};

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}

void SystemStub_SDL::init(const char *title, int w, int h, int scaler, bool fullscreen) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(title, NULL);
	memset(&_pi, 0, sizeof(_pi));
	_screenBuffer = 0;
	_fadeScreenBuffer = 0;
	_fadeOnUpdateScreen = false;
	_fullscreen = fullscreen;
	_currentScaler = scaler;
	memset(_pal, 0, sizeof(_pal));
	setScreenSize(w, h);
	_joystick = NULL;
	if (SDL_NumJoysticks() > 0) {
		_joystick = SDL_JoystickOpen(0);
	}
	_screenshot = 1;
}

void SystemStub_SDL::destroy() {
	cleanupGfxMode();
	if (SDL_JoystickOpened(0)) {
		SDL_JoystickClose(_joystick);
	}
	SDL_Quit();
}

void SystemStub_SDL::setScreenSize(int w, int h) {
	if (_screenW == w && _screenH == h) {
		return;
	}
	free(_screenBuffer);
	_screenBuffer = 0;
	free(_fadeScreenBuffer);
	_fadeScreenBuffer = 0;
	// allocate some extra bytes for the scaling routines
	const int screenBufferSize = (w + 2) * (h + 2) * sizeof(uint16_t);
	_screenBuffer = (uint16_t *)calloc(1, screenBufferSize);
	if (!_screenBuffer) {
		error("SystemStub_SDL::setScreenSize() Unable to allocate offscreen buffer, w=%d, h=%d", w, h);
	}
	_screenW = w;
	_screenH = h;
	prepareGfxMode();
}

void SystemStub_SDL::setPalette(const uint8_t *pal, int n) {
	assert(n <= 256);
	for (int i = 0; i < n; ++i) {
		uint8_t r = pal[i * 3 + 0];
		uint8_t g = pal[i * 3 + 1];
		uint8_t b = pal[i * 3 + 2];
		_pal[i] = SDL_MapRGB(_screenSurface->format, r, g, b);
	}
}

void SystemStub_SDL::setPaletteEntry(int i, const Color *c) {
	_pal[i] = SDL_MapRGB(_screenSurface->format, c->r, c->g, c->b);
}

void SystemStub_SDL::getPaletteEntry(int i, Color *c) {
	SDL_GetRGB(_pal[i], _screenSurface->format, &c->r, &c->g, &c->b);
}

void SystemStub_SDL::setOverscanColor(int i) {
	_overscanColor = i;
}

void SystemStub_SDL::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	if (_numBlitRects >= MAX_BLIT_RECTS) {
		warning("SystemStub_SDL::copyRect() Too many blit rects, you may experience graphical glitches");
	} else {
		// extend the dirty region by 1 pixel for scalers accessing 'outer' pixels
		--x;
		--y;
		w += 2;
		h += 2;

		if (x < 0) {
			x = 0;
		} else if (x >= _screenW) {
			return;
		}
		if (y < 0) {
			y = 0;
		} else if (y >= _screenH) {
			return;
		}
		if (x + w > _screenW) {
			w = _screenW - x;
		}
		if (y + h > _screenH) {
			h = _screenH - y;
		}
		SDL_Rect *br = &_blitRects[_numBlitRects];

		br->x = _pi.mirrorMode ? _screenW - (x + w) : x;
		br->y = y;
		br->w = w;
		br->h = h;
		++_numBlitRects;

		uint16_t *p = _screenBuffer + (br->y + 1) * _screenW + (br->x + 1);
		buf += y * pitch + x;

		if (_pi.mirrorMode) {
			while (h--) {
				for (int i = 0; i < w; ++i) {
					p[i] = _pal[buf[w - 1 - i]];
				}
				p += _screenW;
				buf += pitch;
			}
		} else {
			while (h--) {
				for (int i = 0; i < w; ++i) {
					p[i] = _pal[buf[i]];
				}
				p += _screenW;
				buf += pitch;
			}
		}
		if (_pi.dbgMask & PlayerInput::DF_DBLOCKS) {
			drawRect(br, 0xE7, _screenBuffer + _screenW + 1, _screenW * 2);
		}
	}
}

void SystemStub_SDL::fadeScreen() {
	const int fadeScreenBufferSize = _screenH * _screenW * sizeof(uint16_t);
	if (!_fadeScreenBuffer) {
		_fadeScreenBuffer = (uint16_t *)malloc(fadeScreenBufferSize);
		assert(_fadeScreenBuffer);
	}
	_fadeOnUpdateScreen = true;
	memcpy(_fadeScreenBuffer, _screenBuffer + _screenW + 1, fadeScreenBufferSize);
}

static uint16_t blendPixel16(uint16_t colorSrc, uint16_t colorDst, uint32_t mask, int step) {
	const uint32_t pSrc = (colorSrc | (colorSrc << 16)) & mask;
	const uint32_t pDst = (colorDst | (colorDst << 16)) & mask;
	const uint32_t pRes = ((pDst - pSrc) * step / 16 + pSrc) & mask;
	return pRes | (pRes >> 16);
}

void SystemStub_SDL::updateScreen(int shakeOffset) {
	const int mul = _scalers[_currentScaler].factor;
	if (_fadeOnUpdateScreen) {
		const int tempScreenBufferSize = (_screenH + 2) * (_screenW + 2) * sizeof(uint16_t);
		uint16_t *tempScreenBuffer = (uint16_t *)calloc(tempScreenBufferSize, 1);
		assert(tempScreenBuffer);
		const SDL_PixelFormat *pf = _screenSurface->format;
		const uint32_t colorMask = (pf->Gmask << 16) | (pf->Rmask | pf->Bmask);
		const uint16_t *screenBuffer = _screenBuffer + _screenW + 1;
		for (int i = 1; i <= 16; ++i) {
			for (int x = 0; x < _screenH * _screenW; ++x) {
				tempScreenBuffer[_screenW + 1 + x] = blendPixel16(_fadeScreenBuffer[x], screenBuffer[x], colorMask, i);
			}
			SDL_LockSurface(_screenSurface);
			uint16_t *dst = (uint16_t *)_screenSurface->pixels;
			const uint16_t *src = tempScreenBuffer + _screenW + 1;
			(*_scalers[_currentScaler].proc)(dst, _screenSurface->pitch, src, _screenW, _screenW, _screenH);
			SDL_UnlockSurface(_screenSurface);
			SDL_UpdateRect(_screenSurface, 0, 0, _screenW * mul, _screenH * mul);
			SDL_Delay(30);
		}
		free(tempScreenBuffer);
		_fadeOnUpdateScreen = false;
		return;
	}
	if (shakeOffset == 0) {
		for (int i = 0; i < _numBlitRects; ++i) {
			SDL_Rect *br = &_blitRects[i];
			int dx = br->x * mul;
			int dy = br->y * mul;
			SDL_LockSurface(_screenSurface);
			uint16_t *dst = (uint16_t *)_screenSurface->pixels + dy * _screenSurface->pitch / 2 + dx;
			const uint16_t *src = _screenBuffer + (br->y + 1) * _screenW + (br->x + 1);
			(*_scalers[_currentScaler].proc)(dst, _screenSurface->pitch, src, _screenW, br->w, br->h);
			SDL_UnlockSurface(_screenSurface);
			br->x *= mul;
			br->y *= mul;
			br->w *= mul;
			br->h *= mul;
		}
		SDL_UpdateRects(_screenSurface, _numBlitRects, _blitRects);
	} else {
		SDL_LockSurface(_screenSurface);
		int w = _screenW;
		int h = _screenH - shakeOffset;
		uint16_t *dst = (uint16_t *)_screenSurface->pixels + shakeOffset * mul * _screenSurface->pitch / 2;
		const uint16_t *src = _screenBuffer + _screenW + 1;
		(*_scalers[_currentScaler].proc)(dst, _screenSurface->pitch, src, _screenW, w, h);
		SDL_UnlockSurface(_screenSurface);

		SDL_Rect r;
		r.x = 0;
		r.y = 0;
		r.w = _screenW * mul;
		r.h = shakeOffset * mul;
		SDL_FillRect(_screenSurface, &r, _pal[_overscanColor]);

		SDL_UpdateRect(_screenSurface, 0, 0, _screenW * mul, _screenH * mul);
	}
	_numBlitRects = 0;
}

void SystemStub_SDL::processEvents() {
	bool paused = false;
	while (true) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			processEvent(ev, paused);
			if (_pi.quit) {
				return;
			}
		}
		if (!paused) {
			break;
		}
		SDL_Delay(100);
	}
}

void SystemStub_SDL::processEvent(const SDL_Event &ev, bool &paused) {
		switch (ev.type) {
		case SDL_QUIT:
			_pi.quit = true;
			break;
		case SDL_ACTIVEEVENT:
			if (ev.active.state & SDL_APPINPUTFOCUS) {
				paused = ev.active.gain == 0;
				SDL_PauseAudio(paused ? 1 : 0);
			}
			break;
		case SDL_JOYHATMOTION:
			_pi.dirMask = 0;
			if (ev.jhat.value & SDL_HAT_UP) {
				_pi.dirMask |= PlayerInput::DIR_UP;
			}
			if (ev.jhat.value & SDL_HAT_DOWN) {
				_pi.dirMask |= PlayerInput::DIR_DOWN;
			}
			if (ev.jhat.value & SDL_HAT_LEFT) {
				_pi.dirMask |= PlayerInput::DIR_LEFT;
			}
			if (ev.jhat.value & SDL_HAT_RIGHT) {
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
			}
			break;
		case SDL_JOYAXISMOTION:
			switch (ev.jaxis.axis) {
			case 0:
				if (ev.jaxis.value > JOYSTICK_COMMIT_VALUE) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
					if (_pi.dirMask & PlayerInput::DIR_LEFT) {
						_pi.dirMask &= ~PlayerInput::DIR_LEFT;
					}
				} else if (ev.jaxis.value < -JOYSTICK_COMMIT_VALUE) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
					if (_pi.dirMask & PlayerInput::DIR_RIGHT) {
						_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
					}
				} else {
					_pi.dirMask &= ~(PlayerInput::DIR_RIGHT | PlayerInput::DIR_LEFT);
				}
				break;
			case 1:
				if (ev.jaxis.value > JOYSTICK_COMMIT_VALUE) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
					if (_pi.dirMask & PlayerInput::DIR_UP) {
						_pi.dirMask &= ~PlayerInput::DIR_UP;
					}
				} else if (ev.jaxis.value < -JOYSTICK_COMMIT_VALUE) {
					_pi.dirMask |= PlayerInput::DIR_UP;
					if (_pi.dirMask & PlayerInput::DIR_DOWN) {
						_pi.dirMask &= ~PlayerInput::DIR_DOWN;
					}
				} else {
					_pi.dirMask &= ~(PlayerInput::DIR_UP | PlayerInput::DIR_DOWN);
				}
				break;
			}
			break;
		case SDL_JOYBUTTONDOWN:
			switch (ev.jbutton.button) {
			case 0:
				_pi.space = true;
				break;
			case 1:
				_pi.shift = true;
				break;
			case 2:
				_pi.enter = true;
				break;
			case 3:
				_pi.backspace = true;
				break;
			}
			break;
		case SDL_JOYBUTTONUP:
			switch (ev.jbutton.button) {
			case 0:
				_pi.space = false;
				break;
			case 1:
				_pi.shift = false;
				break;
			case 2:
				_pi.enter = false;
				break;
			case 3:
				_pi.backspace = false;
				break;
			}
			break;
		case SDL_KEYUP:
			switch (ev.key.keysym.sym) {
			case SDLK_LEFT:
				_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				_pi.dirMask &= ~PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
				_pi.space = false;
				break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
				_pi.shift = false;
				break;
			case SDLK_RETURN:
				_pi.enter = false;
				break;
			case SDLK_ESCAPE:
				_pi.escape = false;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYDOWN:
			if (ev.key.keysym.mod & KMOD_ALT) {
				if (ev.key.keysym.sym == SDLK_RETURN) {
					switchGfxMode(!_fullscreen, _currentScaler);
				} else if (ev.key.keysym.sym == SDLK_KP_PLUS || ev.key.keysym.sym == SDLK_PAGEUP) {
					uint8_t s = _currentScaler + 1;
					if (s < NUM_SCALERS) {
						switchGfxMode(_fullscreen, s);
					}
				} else if (ev.key.keysym.sym == SDLK_KP_MINUS || ev.key.keysym.sym == SDLK_PAGEDOWN) {
					int8_t s = _currentScaler - 1;
					if (_currentScaler > 0) {
						switchGfxMode(_fullscreen, s);
					}
				} else if (ev.key.keysym.sym == SDLK_s) {
					char name[32];
					snprintf(name, sizeof(name), "screenshot-%03d.bmp", _screenshot);
					SDL_SaveBMP(_screenSurface, name);
					++_screenshot;
					debug(DBG_INFO, "Written '%s'", name);
				}
				break;
			} else if (ev.key.keysym.mod & KMOD_CTRL) {
				if (ev.key.keysym.sym == SDLK_f) {
					_pi.dbgMask ^= PlayerInput::DF_FASTMODE;
				} else if (ev.key.keysym.sym == SDLK_b) {
					_pi.dbgMask ^= PlayerInput::DF_DBLOCKS;
				} else if (ev.key.keysym.sym == SDLK_i) {
					_pi.dbgMask ^= PlayerInput::DF_SETLIFE;
				} else if (ev.key.keysym.sym == SDLK_m) {
					_pi.mirrorMode = !_pi.mirrorMode;
					flipGfx();
				} else if (ev.key.keysym.sym == SDLK_s) {
					_pi.save = true;
				} else if (ev.key.keysym.sym == SDLK_l) {
					_pi.load = true;
				} else if (ev.key.keysym.sym == SDLK_KP_PLUS || ev.key.keysym.sym == SDLK_PAGEUP) {
					_pi.stateSlot = 1;
				} else if (ev.key.keysym.sym == SDLK_KP_MINUS || ev.key.keysym.sym == SDLK_PAGEDOWN) {
					_pi.stateSlot = -1;
				} else if (ev.key.keysym.sym == SDLK_r) {
					_pi.inpRecord = true;
				} else if (ev.key.keysym.sym == SDLK_p) {
					_pi.inpReplay = true;
				}
			}
			_pi.lastChar = ev.key.keysym.sym;
			switch (ev.key.keysym.sym) {
			case SDLK_LEFT:
				_pi.dirMask |= PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				_pi.dirMask |= PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				_pi.dirMask |= PlayerInput::DIR_DOWN;
				break;
			case SDLK_BACKSPACE:
			case SDLK_TAB:
				_pi.backspace = true;
				break;
			case SDLK_SPACE:
				_pi.space = true;
				break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
				_pi.shift = true;
				break;
			case SDLK_RETURN:
				_pi.enter = true;
				break;
			case SDLK_ESCAPE:
				_pi.escape = true;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
}

void SystemStub_SDL::sleep(int duration) {
	SDL_Delay(duration);
}

uint32_t SystemStub_SDL::getTimeStamp() {
	return SDL_GetTicks();
}

static void mixAudioS8ToU8(void *param, uint8_t *buf, int len) {
	SystemStub_SDL *stub = (SystemStub_SDL *)param;
	stub->_audioCbProc(stub->_audioCbData, (int8_t *)buf, len);
	for (int i = 0; i < len; ++i) {
		buf[i] ^= 0x80;
	}
}

void SystemStub_SDL::startAudio(AudioCallback callback, void *param) {
	SDL_AudioSpec desired, obtained;
	memset(&desired, 0, sizeof(desired));
	desired.freq = SOUND_SAMPLE_RATE;
	desired.format = AUDIO_U8;
	desired.channels = 1;
	desired.samples = 2048;
	desired.callback = mixAudioS8ToU8;
	desired.userdata = this;
	if (SDL_OpenAudio(&desired, &obtained) == 0) {
		_audioCbProc = callback;
		_audioCbData = param;
		SDL_PauseAudio(0);
	} else {
		error("SystemStub_SDL::startAudio() Unable to open sound device");
	}
}

void SystemStub_SDL::stopAudio() {
	SDL_CloseAudio();
}

uint32_t SystemStub_SDL::getOutputSampleRate() {
	return SOUND_SAMPLE_RATE;
}

void SystemStub_SDL::lockAudio() {
	SDL_LockAudio();
}

void SystemStub_SDL::unlockAudio() {
	SDL_UnlockAudio();
}

void SystemStub_SDL::prepareGfxMode() {
	int w = _screenW * _scalers[_currentScaler].factor;
	int h = _screenH * _scalers[_currentScaler].factor;
	_screenSurface = SDL_SetVideoMode(w, h, 16, _fullscreen ? (SDL_FULLSCREEN | SDL_HWSURFACE) : SDL_HWSURFACE);
	if (!_screenSurface) {
		error("SystemStub_SDL::prepareGfxMode() Unable to allocate _screen buffer");
	}
	forceGfxRedraw();
}

void SystemStub_SDL::cleanupGfxMode() {
	if (_screenBuffer) {
		free(_screenBuffer);
		_screenBuffer = 0;
	}
	if (_fadeScreenBuffer) {
		free(_fadeScreenBuffer);
		_fadeScreenBuffer = 0;
	}
	if (_screenSurface) {
		// freed by SDL_Quit()
		_screenSurface = 0;
	}
}

void SystemStub_SDL::switchGfxMode(bool fullscreen, uint8_t scaler) {
	SDL_FreeSurface(_screenSurface);
	_fullscreen = fullscreen;
	_currentScaler = scaler;
	prepareGfxMode();
	forceGfxRedraw();
}

void SystemStub_SDL::flipGfx() {
	uint16_t scanline[256];
	assert(_screenW <= 256);
	uint16_t *p = _screenBuffer + _screenW + 1;
	for (int y = 0; y < _screenH; ++y) {
		p += _screenW;
		for (int x = 0; x < _screenW; ++x) {
			scanline[x] = *--p;
		}
		memcpy(p, scanline, _screenW * sizeof(uint16_t));
		p += _screenW;
	}
	forceGfxRedraw();
}

void SystemStub_SDL::forceGfxRedraw() {
	_numBlitRects = 1;
	_blitRects[0].x = 0;
	_blitRects[0].y = 0;
	_blitRects[0].w = _screenW;
	_blitRects[0].h = _screenH;
}

void SystemStub_SDL::drawRect(SDL_Rect *rect, uint8_t color, uint16_t *dst, uint16_t dstPitch) {
	dstPitch >>= 1;
	int x1 = rect->x;
	int y1 = rect->y;
	int x2 = rect->x + rect->w - 1;
	int y2 = rect->y + rect->h - 1;
	assert(x1 >= 0 && x2 < _screenW && y1 >= 0 && y2 < _screenH);
	for (int i = x1; i <= x2; ++i) {
		*(dst + y1 * dstPitch + i) = *(dst + y2 * dstPitch + i) = _pal[color];
	}
	for (int j = y1; j <= y2; ++j) {
		*(dst + j * dstPitch + x1) = *(dst + j * dstPitch + x2) = _pal[color];
	}
}
