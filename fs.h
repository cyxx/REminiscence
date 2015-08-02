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

#ifndef FS_H__
#define FS_H__

#include "intern.h"

struct FileSystem_impl;

struct FileSystem {
	FileSystem(const char *dataPath);
	~FileSystem();

	FileSystem_impl *_impl;

	char *findPath(const char *filename) const;
	bool exists(const char *filename) const;
};

#endif // FS_H__

