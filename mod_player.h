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

#ifndef MOD_PLAYER_H__
#define MOD_PLAYER_H__

#include "intern.h"

struct File;
struct FileSystem;
struct Mixer;

struct ModPlayer {
	enum {
		NUM_SAMPLES = 31,
		NUM_TRACKS = 4,
		NUM_PATTERNS = 128,
		FRAC_BITS = 12,
		PAULA_FREQ = 3546897
	};

	struct SampleInfo {
		char name[23];
		uint16_t len;
		uint8_t fineTune;
		uint8_t volume;
		uint16_t repeatPos;
		uint16_t repeatLen;
		int8_t *data;

		int8_t getPCM(int offset) const {
			if (offset < 0) {
				offset = 0;
			} else if (offset >= (int)len) {
				offset = len - 1;
			}
			return data[offset];
		}
	};

	struct ModuleInfo {
		char songName[21];
		SampleInfo samples[NUM_SAMPLES];
		uint8_t numPatterns;
		uint8_t patternOrderTable[NUM_PATTERNS];
		uint8_t *patternsTable;
	};

	struct Track {
		SampleInfo *sample;
		uint8_t volume;
		int pos;
		int freq;
		uint16_t period;
		uint16_t periodIndex;
		uint16_t effectData;
		int vibratoSpeed;
		int vibratoAmp;
		int vibratoPos;
		int portamento;
		int portamentoSpeed;
		int retriggerCounter;
		int delayCounter;
		int cutCounter;
	};

	static const int8_t _sineWaveTable[];
	static const uint16_t _periodTable[];
	static const char *_modulesFiles[][2];
	static const int _modulesFilesCount;

	ModuleInfo _modInfo;
	uint8_t _currentPatternOrder;
	uint8_t _currentPatternPos;
	uint8_t _currentTick;
	uint8_t _songSpeed;
	uint8_t _songTempo;
	int _patternDelay;
	int _patternLoopPos;
	int _patternLoopCount;
	int _samplesLeft;
	uint8_t _songNum;
	bool _introSongHack;
	bool _playing;
	Track _tracks[NUM_TRACKS];
	Mixer *_mix;
	FileSystem *_fs;

	ModPlayer(Mixer *mixer, FileSystem *fs);

	uint16_t findPeriod(uint16_t period, uint8_t fineTune) const;
	void load(File *f);
	void unload();
	void play(uint8_t num);
	void stop();
	void handleNote(int trackNum, uint32_t noteData);
	void handleTick();
	void applyVolumeSlide(int trackNum, int amount);
	void applyVibrato(int trackNum);
	void applyPortamento(int trackNum);
	void handleEffect(int trackNum, bool tick);
	void mixSamples(int8_t *buf, int len);
	bool mix(int8_t *buf, int len);

	static bool mixCallback(void *param, int8_t *buf, int len);
};

#endif // MOD_PLAYER_H__
