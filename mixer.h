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

#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"
#include "mod_player.h"
#include "ogg_player.h"
#include "sfx_player.h"

struct MixerChunk {
	uint8_t *data;
	uint32_t len;

	MixerChunk()
		: data(0), len(0) {
	}

	int8_t getPCM(int offset) const {
		if (offset < 0) {
			offset = 0;
		} else if (offset >= (int)len) {
			offset = len - 1;
		}
		return (int8_t)data[offset];
	}
};

struct MixerChannel {
	uint8_t active;
	uint8_t volume;
	MixerChunk chunk;
	uint32_t chunkPos;
	uint32_t chunkInc;
};

struct FileSystem;
struct SystemStub;

struct Mixer {
	typedef bool (*PremixHook)(void *userData, int8_t *buf, int len);

	enum MusicType {
		MT_NONE,
		MT_MOD,
		MT_OGG,
		MT_SFX,
	};

	enum {
		MUSIC_TRACK = 1000,
		NUM_CHANNELS = 4,
		FRAC_BITS = 12,
		MAX_VOLUME = 64
	};

	FileSystem *_fs;
	SystemStub *_stub;
	MixerChannel _channels[NUM_CHANNELS];
	PremixHook _premixHook;
	void *_premixHookData;
	MusicType _musicType;
	ModPlayer _mod;
	OggPlayer _ogg;
	SfxPlayer _sfx;
	int _musicTrack;

	Mixer(FileSystem *fs, SystemStub *stub);
	void init();
	void free();
	void setPremixHook(PremixHook premixHook, void *userData);
	void play(const MixerChunk *mc, uint16_t freq, uint8_t volume);
	bool isPlaying(const MixerChunk *mc) const;
	uint32_t getSampleRate() const;
	void stopAll();
	void playMusic(int num);
	void stopMusic();
	void mix(int8_t *buf, int len);

	static void addclamp(int8_t &a, int b);
	static void mixCallback(void *param, int8_t *buf, int len);
};

template <class T>
int resampleLinear(T *sample, int pos, int step, int fracBits) {
	const int inputPos = pos >> fracBits;
	const int inputFrac = pos & ((1 << fracBits) - 1);
	int out = sample->getPCM(inputPos);
	out += (sample->getPCM(inputPos + 1) - out) * inputFrac >> fracBits;
	return out;
}

#endif // MIXER_H__
