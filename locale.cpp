
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "locale.h"


Locale::Locale(Version ver)
	: _ver(ver) {
	switch (_ver) {
	case LANG_FR:
		_stringsTable = _stringsTableFR;
		_textsTable = _textsTableFR;
		break;
	case LANG_EN:
		_stringsTable = _stringsTableEN;
		_textsTable = _textsTableEN;
		break;
	case LANG_DE:
		_stringsTable = _stringsTableDE;
		_textsTable = _textsTableDE;
		break;
	case LANG_SP:
		_stringsTable = _stringsTableSP;
		_textsTable = _textsTableSP;
		break;
	}
}

const char *Locale::get(int id) const {
	const char *text = 0;
	if (id >= 0 && id < LI_NUM) {
		text = _textsTable[id];
	}
	return text;
}
