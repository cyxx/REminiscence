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

#ifndef MENU_H__
#define MENU_H__

#include "intern.h"

struct Resource;
struct SystemStub;
struct Video;

struct Menu {
	enum {
		MENU_OPTION_ITEM_START,
		MENU_OPTION_ITEM_SKILL,
		MENU_OPTION_ITEM_PASSWORD,
		MENU_OPTION_ITEM_LEVEL,
		MENU_OPTION_ITEM_INFO,
		MENU_OPTION_ITEM_QUIT
	};

	enum {
		EVENTS_DELAY = 80
	};

	static const char *_passwords[8][3];

	Resource *_res;
	SystemStub *_stub;
	Video *_vid;

	const char **_textOptions;
	uint8_t _charVar1;
	uint8_t _charVar2;
	uint8_t _charVar3;
	uint8_t _charVar4;
	uint8_t _charVar5;

	Menu(Resource *res, SystemStub *stub, Video *vid);

	void drawString(const char *str, int16_t y, int16_t x, uint8_t color);
	void drawString2(const char *str, int16_t y, int16_t x);
	void loadPicture(const char *prefix);
	void handleInfoScreen();
	void handleSkillScreen(uint8_t &new_skill);
	bool handlePasswordScreen(uint8_t &new_skill, uint8_t &new_level);
	bool handleLevelScreen(uint8_t &new_skill, uint8_t &new_level);
	bool handleTitleScreen(uint8_t &new_skill, uint8_t &new_level);
};

#endif // MENU_H__
