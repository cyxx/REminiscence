
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
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
		MENU_OPTION_ITEM_DEMO,
		MENU_OPTION_ITEM_QUIT
	};
	enum {
		SCREEN_TITLE,
		SCREEN_SKILL,
		SCREEN_PASSWORD,
		SCREEN_LEVEL,
		SCREEN_INFO
	};

	enum {
		EVENTS_DELAY = 80
	};

	struct Item {
		int str;
		int opt;
	};

	static const char *_passwords[8][3];
	static const char *_passwordsFrAmiga[];
	static const char *_passwordsEnAmiga[];

	Resource *_res;
	SystemStub *_stub;
	Video *_vid;

	int _currentScreen;
	int _nextScreen;
	int _selectedOption;

	int _skill;
	int _level;

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
	void handleSkillScreen();
	bool handlePasswordScreen();
	bool handleLevelScreen();
	void handleTitleScreen();
};

#endif // MENU_H__
