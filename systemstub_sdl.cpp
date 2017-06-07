
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include "scaler.h"
#include "systemstub.h"
#include "util.h"

static const int kAudioHz = 22050;

static const int kJoystickIndex = 0;
static const int kJoystickCommitValue = 3200;

struct SystemStub_SDL : SystemStub {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window *_window;
	SDL_Renderer *_renderer;
	SDL_Texture *_texture;
	int _texW, _texH;
	SDL_GameController *_controller;
#else
	SDL_Surface *_surface;
#endif
	SDL_PixelFormat *_fmt;
	const char *_caption;
	uint16_t *_screenBuffer;
	uint16_t *_fadeScreenBuffer;
	bool _fullscreen;
	int _scaler;
	uint8_t _overscanColor;
	uint16_t _pal[256];
	int _screenW, _screenH;
	SDL_Joystick *_joystick;
	SDL_Rect _blitRects[200];
	int _numBlitRects;
	bool _fadeOnUpdateScreen;
	void (*_audioCbProc)(void *, int16_t *, int);
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
	void prepareGraphics();
	void cleanupGraphics();
	void changeGraphics(bool fullscreen, uint8_t scaler);
	void flipGraphics();
	void forceGraphicsRedraw();
	void drawRect(SDL_Rect *rect, uint8_t color, uint16_t *dst, uint16_t dstPitch);
};

SystemStub *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}

void SystemStub_SDL::init(const char *title, int w, int h, int scaler, bool fullscreen) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
	SDL_ShowCursor(SDL_DISABLE);
	_caption = title;
	memset(&_pi, 0, sizeof(_pi));
	_screenBuffer = 0;
	_fadeScreenBuffer = 0;
	_fadeOnUpdateScreen = false;
	_fullscreen = fullscreen;
	_scaler = scaler;
	memset(_pal, 0, sizeof(_pal));
	_screenW = _screenH = 0;
	setScreenSize(w, h);
	_joystick = 0;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	_controller = 0;
	if (SDL_NumJoysticks() > 0) {
		SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
		if (SDL_IsGameController(kJoystickIndex)) {
			_controller = SDL_GameControllerOpen(kJoystickIndex);
		}
		if (!_controller) {
			_joystick = SDL_JoystickOpen(kJoystickIndex);
		}
	}
#else
	if (SDL_NumJoysticks() > 0) {
		_joystick = SDL_JoystickOpen(kJoystickIndex);
	}
#endif
	_screenshot = 1;
}

void SystemStub_SDL::destroy() {
	cleanupGraphics();
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (_controller) {
		SDL_GameControllerClose(_controller);
		_controller = 0;
	}
#endif
	if (_joystick) {
		SDL_JoystickClose(_joystick);
		_joystick = 0;
	}
	SDL_Quit();
}

void SystemStub_SDL::setScreenSize(int w, int h) {
	if (_screenW == w && _screenH == h) {
		return;
	}
	cleanupGraphics();
	// allocate some extra bytes for the scaling routines
	const int screenBufferSize = (w + 2) * (h + 2) * sizeof(uint16_t);
	_screenBuffer = (uint16_t *)calloc(1, screenBufferSize);
	if (!_screenBuffer) {
		error("SystemStub_SDL::setScreenSize() Unable to allocate offscreen buffer, w=%d, h=%d", w, h);
	}
	_screenW = w;
	_screenH = h;
	prepareGraphics();
}

void SystemStub_SDL::setPalette(const uint8_t *pal, int n) {
	assert(n <= 256);
	for (int i = 0; i < n; ++i) {
		uint8_t r = pal[i * 3 + 0];
		uint8_t g = pal[i * 3 + 1];
		uint8_t b = pal[i * 3 + 2];
		_pal[i] = SDL_MapRGB(_fmt, r, g, b);
	}
}

void SystemStub_SDL::setPaletteEntry(int i, const Color *c) {
	_pal[i] = SDL_MapRGB(_fmt, c->r, c->g, c->b);
}

void SystemStub_SDL::getPaletteEntry(int i, Color *c) {
	SDL_GetRGB(_pal[i], _fmt, &c->r, &c->g, &c->b);
}

void SystemStub_SDL::setOverscanColor(int i) {
	_overscanColor = i;
}

void SystemStub_SDL::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	if (_numBlitRects >= (int)ARRAYSIZE(_blitRects)) {
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

		br->x = x;
		br->y = y;
		br->w = w;
		br->h = h;
		++_numBlitRects;

		uint16_t *p = _screenBuffer + (br->y + 1) * _screenW + (br->x + 1);
		buf += y * pitch + x;

		while (h--) {
			for (int i = 0; i < w; ++i) {
				p[i] = _pal[buf[i]];
			}
			p += _screenW;
			buf += pitch;
		}
		if (_pi.dbgMask & PlayerInput::DF_DBLOCKS) {
			drawRect(br, 0xE7, _screenBuffer + _screenW + 1, _screenW * 2);
		}
	}
}

void SystemStub_SDL::fadeScreen() {
	const int bufferSize = _screenH * _screenW * sizeof(uint16_t);
	if (!_fadeScreenBuffer) {
		_fadeScreenBuffer = (uint16_t *)malloc(bufferSize);
		if (!_fadeScreenBuffer) {
			warning("SystemStub_SDL::fadeScreen() Unable to allocate buffer size %d", bufferSize);
			return;
		}
	}
	_fadeOnUpdateScreen = true;
	memcpy(_fadeScreenBuffer, _screenBuffer + _screenW + 1, bufferSize);
}

static uint16_t blendPixel16(uint16_t colorSrc, uint16_t colorDst, uint32_t mask, int step) {
	const uint32_t pSrc = (colorSrc | (colorSrc << 16)) & mask;
	const uint32_t pDst = (colorDst | (colorDst << 16)) & mask;
	const uint32_t pRes = ((pDst - pSrc) * step / 16 + pSrc) & mask;
	return pRes | (pRes >> 16);
}

void SystemStub_SDL::updateScreen(int shakeOffset) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (_texW != _screenW || _texH != _screenH) {
		void *dst = 0;
		int pitch = 0;
		if (SDL_LockTexture(_texture, 0, &dst, &pitch) == 0) {
			(*_scalers[_scaler].proc)((uint16_t *)dst, pitch, _screenBuffer + _screenW + 1, _screenW, _screenW, _screenH);
			SDL_UnlockTexture(_texture);
		}
	} else {
		SDL_UpdateTexture(_texture, 0, _screenBuffer + _screenW + 1, _screenW * sizeof(uint16_t));
	}
	SDL_RenderClear(_renderer);
	if (_fadeOnUpdateScreen) {
		SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
		SDL_Rect r;
		r.x = r.y = 0;
		SDL_GetRendererOutputSize(_renderer, &r.w, &r.h);
		for (int i = 1; i <= 16; ++i) {
			SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 256 - i * 16);
			SDL_RenderCopy(_renderer, _texture, 0, 0);
			SDL_RenderFillRect(_renderer, &r);
			SDL_RenderPresent(_renderer);
			SDL_Delay(30);
		}
		_fadeOnUpdateScreen = false;
		return;
	}
	if (shakeOffset != 0) {
		SDL_Rect r;
		r.x = 0;
		r.y = shakeOffset * _scalers[_scaler].factor;
		SDL_GetRendererOutputSize(_renderer, &r.w, &r.h);
		r.h -= r.y;
		SDL_RenderCopy(_renderer, _texture, 0, &r);
	} else {
		SDL_RenderCopy(_renderer, _texture, 0, 0);
	}
	SDL_RenderPresent(_renderer);
#else
	const int mul = _scalers[_scaler].factor;
	if (_fadeOnUpdateScreen) {
		const int tempScreenBufferSize = (_screenH + 2) * (_screenW + 2) * sizeof(uint16_t);
		uint16_t *tempScreenBuffer = (uint16_t *)calloc(tempScreenBufferSize, 1);
		assert(tempScreenBuffer);
		const uint32_t colorMask = (_fmt->Gmask << 16) | (_fmt->Rmask | _fmt->Bmask);
		const uint16_t *screenBuffer = _screenBuffer + _screenW + 1;
		for (int i = 1; i <= 16; ++i) {
			for (int x = 0; x < _screenH * _screenW; ++x) {
				tempScreenBuffer[_screenW + 1 + x] = blendPixel16(_fadeScreenBuffer[x], screenBuffer[x], colorMask, i);
			}
			SDL_LockSurface(_surface);
			uint16_t *dst = (uint16_t *)_surface->pixels;
			const uint16_t *src = tempScreenBuffer + _screenW + 1;
			(*_scalers[_scaler].proc)(dst, _surface->pitch, src, _screenW, _screenW, _screenH);
			SDL_UnlockSurface(_surface);
			SDL_UpdateRect(_surface, 0, 0, _screenW * mul, _screenH * mul);
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
			SDL_LockSurface(_surface);
			uint16_t *dst = (uint16_t *)_surface->pixels + dy * _surface->pitch / 2 + dx;
			const uint16_t *src = _screenBuffer + (br->y + 1) * _screenW + (br->x + 1);
			(*_scalers[_scaler].proc)(dst, _surface->pitch, src, _screenW, br->w, br->h);
			SDL_UnlockSurface(_surface);
			br->x *= mul;
			br->y *= mul;
			br->w *= mul;
			br->h *= mul;
		}
		SDL_UpdateRects(_surface, _numBlitRects, _blitRects);
	} else {
		SDL_LockSurface(_surface);
		int w = _screenW;
		int h = _screenH - shakeOffset;
		uint16_t *dst = (uint16_t *)_surface->pixels + shakeOffset * mul * _surface->pitch / 2;
		const uint16_t *src = _screenBuffer + _screenW + 1;
		(*_scalers[_scaler].proc)(dst, _surface->pitch, src, _screenW, w, h);
		SDL_UnlockSurface(_surface);

		SDL_Rect r;
		r.x = 0;
		r.y = 0;
		r.w = _screenW * mul;
		r.h = shakeOffset * mul;
		SDL_FillRect(_surface, &r, _pal[_overscanColor]);

		SDL_UpdateRect(_surface, 0, 0, _screenW * mul, _screenH * mul);
	}
#endif
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
#if SDL_VERSION_ATLEAST(2, 0, 0)
	case SDL_WINDOWEVENT:
		switch (ev.window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST:
			paused = (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST);
			SDL_PauseAudio(paused);
			break;
		}
		break;
#else
		case SDL_ACTIVEEVENT:
			if (ev.active.state & SDL_APPINPUTFOCUS) {
				paused = ev.active.gain == 0;
				SDL_PauseAudio(paused ? 1 : 0);
			}
			break;
#endif
	case SDL_JOYHATMOTION:
		if (_joystick) {
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
		}
		break;
	case SDL_JOYAXISMOTION:
		if (_joystick) {
			switch (ev.jaxis.axis) {
			case 0:
				_pi.dirMask &= ~(PlayerInput::DIR_RIGHT | PlayerInput::DIR_LEFT);
				if (ev.jaxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				} else if (ev.jaxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				}
				break;
			case 1:
				_pi.dirMask &= ~(PlayerInput::DIR_UP | PlayerInput::DIR_DOWN);
				if (ev.jaxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				} else if (ev.jaxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				}
				break;
			}
		}
		break;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		if (_joystick) {
			const bool pressed = (ev.jbutton.state == SDL_PRESSED);
			switch (ev.jbutton.button) {
			case 0:
				_pi.space = pressed;
				break;
			case 1:
				_pi.shift = pressed;
				break;
			case 2:
				_pi.enter = pressed;
				break;
			case 3:
				_pi.backspace = pressed;
				break;
			}
		}
		break;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	case SDL_CONTROLLERAXISMOTION:
		if (_controller) {
			switch (ev.caxis.axis) {
			case SDL_CONTROLLER_AXIS_LEFTX:
			case SDL_CONTROLLER_AXIS_RIGHTX:
				if (ev.caxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				}
				if (ev.caxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				}
				break;
			case SDL_CONTROLLER_AXIS_LEFTY:
			case SDL_CONTROLLER_AXIS_RIGHTY:
				if (ev.caxis.value < -kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_UP;
				}
				if (ev.caxis.value > kJoystickCommitValue) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				}
				break;
			}
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		if (_controller) {
			const bool pressed = (ev.cbutton.state == SDL_PRESSED);
			switch (ev.cbutton.button) {
			case SDL_CONTROLLER_BUTTON_A:
				_pi.enter = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_B:
				_pi.space = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_X:
				_pi.shift = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_Y:
				_pi.backspace = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_BACK:
			case SDL_CONTROLLER_BUTTON_START:
				_pi.escape = pressed;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_UP:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_UP;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_UP;
				}
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_DOWN;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				}
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_LEFT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				}
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
				if (pressed) {
					_pi.dirMask |= PlayerInput::DIR_RIGHT;
				} else {
					_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				}
				break;
			}
		}
		break;
#endif
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
					changeGraphics(!_fullscreen, _scaler);
				} else if (ev.key.keysym.sym == SDLK_KP_PLUS || ev.key.keysym.sym == SDLK_PAGEUP) {
					uint8_t s = _scaler + 1;
					if (s < NUM_SCALERS) {
						changeGraphics(_fullscreen, s);
					}
				} else if (ev.key.keysym.sym == SDLK_KP_MINUS || ev.key.keysym.sym == SDLK_PAGEDOWN) {
					int8_t s = _scaler - 1;
					if (_scaler > 0) {
						changeGraphics(_fullscreen, s);
					}
				} else if (ev.key.keysym.sym == SDLK_s) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
#else
					char name[32];
					snprintf(name, sizeof(name), "screenshot-%03d.bmp", _screenshot);
					SDL_SaveBMP(_surface, name);
					++_screenshot;
					debug(DBG_INFO, "Written '%s'", name);
#endif
				}
				break;
			} else if (ev.key.keysym.mod & KMOD_CTRL) {
				if (ev.key.keysym.sym == SDLK_f) {
					_pi.dbgMask ^= PlayerInput::DF_FASTMODE;
				} else if (ev.key.keysym.sym == SDLK_b) {
					_pi.dbgMask ^= PlayerInput::DF_DBLOCKS;
				} else if (ev.key.keysym.sym == SDLK_i) {
					_pi.dbgMask ^= PlayerInput::DF_SETLIFE;
				} else if (ev.key.keysym.sym == SDLK_s) {
					_pi.save = true;
				} else if (ev.key.keysym.sym == SDLK_l) {
					_pi.load = true;
				} else if (ev.key.keysym.sym == SDLK_KP_PLUS || ev.key.keysym.sym == SDLK_PAGEUP) {
					_pi.stateSlot = 1;
				} else if (ev.key.keysym.sym == SDLK_KP_MINUS || ev.key.keysym.sym == SDLK_PAGEDOWN) {
					_pi.stateSlot = -1;
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

static void mixAudioS16(void *param, uint8_t *buf, int len) {
	SystemStub_SDL *stub = (SystemStub_SDL *)param;
	memset(buf, 0, len);
	stub->_audioCbProc(stub->_audioCbData, (int16_t *)buf, len / 2);
}

void SystemStub_SDL::startAudio(AudioCallback callback, void *param) {
	SDL_AudioSpec desired, obtained;
	memset(&desired, 0, sizeof(desired));
	desired.freq = kAudioHz;
	desired.format = AUDIO_S16SYS;
	desired.channels = 1;
	desired.samples = 2048;
	desired.callback = mixAudioS16;
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
	return kAudioHz;
}

void SystemStub_SDL::lockAudio() {
	SDL_LockAudio();
}

void SystemStub_SDL::unlockAudio() {
	SDL_UnlockAudio();
}

void SystemStub_SDL::prepareGraphics() {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	switch (_scaler) {
	case SCALER_SCALE_2X:
	case SCALER_SCALE_3X:
	case SCALER_SCALE_4X:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
		_texW = _screenW * _scalers[_scaler].factor;
		_texH = _screenH * _scalers[_scaler].factor;
		break;
	default:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest pixel sampling
		_texW = _screenW;
		_texH = _screenH;
		break;
	}
	const int windowW = _screenW * _scalers[_scaler].factor;
	const int windowH = _screenH * _scalers[_scaler].factor;
	int flags = 0;
	if (_fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	_window = SDL_CreateWindow(_caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, flags);
	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderSetLogicalSize(_renderer, windowW, windowH);
        static const uint32_t kPixelFormat = SDL_PIXELFORMAT_RGB565;
	_texture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, _texW, _texH);
	_fmt = SDL_AllocFormat(kPixelFormat);
#else
	SDL_WM_SetCaption(_caption, NULL);
	const int w = _screenW * _scalers[_scaler].factor;
	const int h = _screenH * _scalers[_scaler].factor;
	int flags = SDL_HWSURFACE;
	if (_fullscreen) {
		flags |= SDL_FULLSCREEN;
	}
	static const int kBitDepth = 16;
	_surface = SDL_SetVideoMode(w, h, kBitDepth, flags);
	if (!_surface) {
		error("SystemStub_SDL::prepareGraphics() Unable to allocate _screen buffer");
	}
	_fmt = _surface->format;
#endif
	forceGraphicsRedraw();
}

void SystemStub_SDL::cleanupGraphics() {
	if (_screenBuffer) {
		free(_screenBuffer);
		_screenBuffer = 0;
	}
	if (_fadeScreenBuffer) {
		free(_fadeScreenBuffer);
		_fadeScreenBuffer = 0;
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	if (_fmt) {
		SDL_FreeFormat(_fmt);
		_fmt = 0;
	}
#else
	if (_surface) {
		// freed by SDL_Quit()
		_surface = 0;
	}
#endif
}

void SystemStub_SDL::changeGraphics(bool fullscreen, uint8_t scaler) {
	if (_fadeScreenBuffer) {
		free(_fadeScreenBuffer);
		_fadeScreenBuffer = 0;
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	if (_fmt) {
		SDL_FreeFormat(_fmt);
		_fmt = 0;
	}
#else
	SDL_FreeSurface(_surface);
	_surface = 0;
#endif
	_fullscreen = fullscreen;
	_scaler = scaler;
	prepareGraphics();
	forceGraphicsRedraw();
}

void SystemStub_SDL::flipGraphics() {
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
	forceGraphicsRedraw();
}

void SystemStub_SDL::forceGraphicsRedraw() {
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
