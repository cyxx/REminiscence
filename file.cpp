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

#include "fs.h"
#ifdef USE_ZLIB
#include "zlib.h"
#endif
#include "file.h"


struct File_impl {
	bool _ioErr;
	File_impl() : _ioErr(false) {}
	virtual ~File_impl() {}
	virtual bool open(const char *path, const char *mode) = 0;
	virtual void close() = 0;
	virtual uint32_t size() = 0;
	virtual void seek(int32_t off) = 0;
	virtual uint32_t read(void *ptr, uint32_t len) = 0;
	virtual uint32_t write(void *ptr, uint32_t len) = 0;
};

struct stdFile : File_impl {
	FILE *_fp;
	stdFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = fopen(path, mode);
		return (_fp != 0);
	}
	void close() {
		if (_fp) {
			fclose(_fp);
			_fp = 0;
		}
	}
	uint32_t size() {
		uint32_t sz = 0;
		if (_fp) {
			int pos = ftell(_fp);
			fseek(_fp, 0, SEEK_END);
			sz = ftell(_fp);
			fseek(_fp, pos, SEEK_SET);
		}
		return sz;
	}
	void seek(int32_t off) {
		if (_fp) {
			fseek(_fp, off, SEEK_SET);
		}
	}
	uint32_t read(void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = fread(ptr, 1, len, _fp);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
	uint32_t write(void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = fwrite(ptr, 1, len, _fp);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
};

#ifdef USE_ZLIB
struct zlibFile : File_impl {
	gzFile _fp;
	zlibFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = gzopen(path, mode);
		return (_fp != 0);
	}
	void close() {
		if (_fp) {
			gzclose(_fp);
			_fp = 0;
		}
	}
	uint32_t size() {
		uint32_t sz = 0;
		if (_fp) {
			int pos = gztell(_fp);
			gzseek(_fp, 0, SEEK_END);
			sz = gztell(_fp);
			gzseek(_fp, pos, SEEK_SET);
		}
		return sz;
	}
	void seek(int32_t off) {
		if (_fp) {
			gzseek(_fp, off, SEEK_SET);
		}
	}
	uint32_t read(void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = gzread(_fp, ptr, len);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
	uint32_t write(void *ptr, uint32_t len) {
		if (_fp) {
			uint32_t r = gzwrite(_fp, ptr, len);
			if (r != len) {
				_ioErr = true;
			}
			return r;
		}
		return 0;
	}
};
#endif


File::File()
	: _impl(0) {
}

File::~File() {
	if (_impl) {
		_impl->close();
		delete _impl;
	}
}

bool File::open(const char *filename, const char *mode, FileSystem *fs) {
	if (_impl) {
		_impl->close();
		delete _impl;
		_impl = 0;
	}
	assert(mode[0] != 'z');
	_impl = new stdFile;
	char *path = fs->findPath(filename);
	if (path) {
		debug(DBG_FILE, "Open file name '%s' mode '%s' path '%s'", filename, mode, path);
		bool ret = _impl->open(path, mode);
		free(path);
		return ret;
	}
	return false;
}

bool File::open(const char *filename, const char *mode, const char *directory) {
	if (_impl) {
		_impl->close();
		delete _impl;
		_impl = 0;
	}
#ifdef USE_ZLIB
	if (mode[0] == 'z') {
		_impl = new zlibFile;
		++mode;
	}
#endif
	if (!_impl) {
		_impl = new stdFile;
	}
	char path[512];
	snprintf(path, sizeof(path), "%s/%s", directory, filename);
	debug(DBG_FILE, "Open file name '%s' mode '%s' path '%s'", filename, mode, path);
	return _impl->open(path, mode);
}

void File::close() {
	if (_impl) {
		_impl->close();
	}
}

bool File::ioErr() const {
	return _impl->_ioErr;
}

uint32_t File::size() {
	return _impl->size();
}

void File::seek(int32_t off) {
	_impl->seek(off);
}

uint32_t File::read(void *ptr, uint32_t len) {
	return _impl->read(ptr, len);
}

uint8_t File::readByte() {
	uint8_t b;
	read(&b, 1);
	return b;
}

uint16_t File::readUint16LE() {
	uint8_t lo = readByte();
	uint8_t hi = readByte();
	return (hi << 8) | lo;
}

uint32_t File::readUint32LE() {
	uint16_t lo = readUint16LE();
	uint16_t hi = readUint16LE();
	return (hi << 16) | lo;
}

uint16_t File::readUint16BE() {
	uint8_t hi = readByte();
	uint8_t lo = readByte();
	return (hi << 8) | lo;
}

uint32_t File::readUint32BE() {
	uint16_t hi = readUint16BE();
	uint16_t lo = readUint16BE();
	return (hi << 16) | lo;
}

uint32_t File::write(void *ptr, uint32_t len) {
	return _impl->write(ptr, len);
}

void File::writeByte(uint8_t b) {
	write(&b, 1);
}

void File::writeUint16BE(uint16_t n) {
	writeByte(n >> 8);
	writeByte(n & 0xFF);
}

void File::writeUint32BE(uint32_t n) {
	writeUint16BE(n >> 16);
	writeUint16BE(n & 0xFFFF);
}
