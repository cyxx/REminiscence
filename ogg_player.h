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

#ifndef OGG_PLAYER_H__
#define OGG_PLAYER_H__

#include "intern.h"

struct FileSystem;
struct Mixer;
struct OggDecoder_impl;

struct OggPlayer {
	OggPlayer(Mixer *mixer, FileSystem *fs);
	~OggPlayer();

	bool playTrack(int num);
	void stopTrack();
	void pauseTrack();
	void resumeTrack();
	bool isPlaying() const { return _impl != 0; }
	bool mix(int8_t *buf, int len);
	static bool mixCallback(void *param, int8_t *buf, int len);

	Mixer *_mix;
	FileSystem *_fs;
	OggDecoder_impl *_impl;
};

#endif // OGG_PLAYER_H__

