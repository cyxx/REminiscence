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
