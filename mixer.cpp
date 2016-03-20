
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "mixer.h"
#include "systemstub.h"
#include "util.h"

Mixer::Mixer(FileSystem *fs, SystemStub *stub)
	: _stub(stub), _musicType(MT_NONE), _mod(this, fs), _ogg(this, fs), _sfx(this) {
	_musicTrack = -1;
}

void Mixer::init() {
	memset(_channels, 0, sizeof(_channels));
	_premixHook = 0;
	_stub->startAudio(Mixer::mixCallback, this);
}

void Mixer::free() {
	setPremixHook(0, 0);
	stopAll();
	_stub->stopAudio();
}

void Mixer::setPremixHook(PremixHook premixHook, void *userData) {
	debug(DBG_SND, "Mixer::setPremixHook()");
	LockAudioStack las(_stub);
	_premixHook = premixHook;
	_premixHookData = userData;
}

void Mixer::play(const MixerChunk *mc, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::play(%d, %d)", freq, volume);
	LockAudioStack las(_stub);
	MixerChannel *ch = 0;
	for (int i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *cur = &_channels[i];
		if (cur->active) {
			if (cur->chunk.data == mc->data) {
				cur->chunkPos = 0;
				return;
			}
		} else {
			ch = cur;
			break;
		}
	}
	if (ch) {
		ch->active = true;
		ch->volume = volume;
		ch->chunk = *mc;
		ch->chunkPos = 0;
		ch->chunkInc = (freq << FRAC_BITS) / _stub->getOutputSampleRate();
	}
}

bool Mixer::isPlaying(const MixerChunk *mc) const {
	debug(DBG_SND, "Mixer::isPlaying");
	LockAudioStack las(_stub);
	for (int i = 0; i < NUM_CHANNELS; ++i) {
		const MixerChannel *ch = &_channels[i];
		if (ch->active && ch->chunk.data == mc->data) {
			return true;
		}
	}
	return false;
}

uint32_t Mixer::getSampleRate() const {
	return _stub->getOutputSampleRate();
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	LockAudioStack las(_stub);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;
	}
}

static bool isMusicSfx(int num) {
	return (num >= 68 && num <= 75);
}

void Mixer::playMusic(int num) {
	debug(DBG_SND, "Mixer::playMusic(%d)", num);
	if (num > MUSIC_TRACK && num != _musicTrack) {
		if (_ogg.playTrack(num - MUSIC_TRACK)) {
			_musicType = MT_OGG;
			_musicTrack = num;
			return;
		}
	}
	if (num == 1) { // menu screen
		if (_ogg.playTrack(2)) {
			_musicType = MT_OGG;
			_musicTrack = 2;
			return;
		}
	}
	if (isMusicSfx(num)) { // level action sequence
		_sfx.play(num);
		_musicType = MT_SFX;
	} else { // cutscene
		_mod.play(num);
		_musicType = MT_MOD;
	}
}

void Mixer::stopMusic() {
	debug(DBG_SND, "Mixer::stopMusic()");
	switch (_musicType) {
	case MT_NONE:
		break;
	case MT_MOD:
		_mod.stop();
		break;
	case MT_OGG:
		_ogg.stopTrack();
		_musicTrack = -1;
		break;
	case MT_SFX:
		_sfx.stop();
		break;
	}
	_musicType = MT_NONE;
	if (_musicTrack != -1) {
		_ogg.resumeTrack();
		_musicType = MT_OGG;
	}
}

static void nr(const int8_t *in, int len, int8_t *out) {
	static int prev = 0;
	for (int i = 0; i < len; ++i) {
		const int vnr = in[i] >> 1;
		out[i] = vnr + prev;
		prev = vnr;
	}
}

void Mixer::mix(int8_t *out, int len) {
	int8_t buf[len];
	memset(buf, 0, len);
	if (_premixHook) {
		if (!_premixHook(_premixHookData, buf, len)) {
			_premixHook = 0;
			_premixHookData = 0;
		}
	}
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *ch = &_channels[i];
		if (ch->active) {
			for (int pos = 0; pos < len; ++pos) {
				if ((ch->chunkPos >> FRAC_BITS) >= (ch->chunk.len - 1)) {
					ch->active = false;
					break;
				}
				const int sample = ch->chunk.getPCM(ch->chunkPos >> FRAC_BITS);
				addclamp(buf[pos], sample * ch->volume / Mixer::MAX_VOLUME);
				ch->chunkPos += ch->chunkInc;
			}
		}
	}
	nr(buf, len, out);
}

void Mixer::addclamp(int8_t& a, int b) {
	int add = a + b;
	if (add < -128) {
		add = -128;
	} else if (add > 127) {
		add = 127;
	}
	a = add;
}

void Mixer::mixCallback(void *param, int8_t *buf, int len) {
	((Mixer *)param)->mix(buf, len);
}
