
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2018 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <sys/param.h>
#include "file.h"
#include "fs.h"
#include "util.h"
#ifdef USE_ZLIB
#include "zlib.h"
#endif
#ifdef USE_RWOPS
#include <SDL_filesystem.h>
#include <SDL_rwops.h>
#endif

struct File_impl {
	bool _ioErr;
	File_impl() : _ioErr(false) {}
	virtual ~File_impl() {}
	virtual bool open(const char *path, const char *mode) = 0;
	virtual void close() = 0;
	virtual uint32_t size() = 0;
	virtual void seek(int32_t off) = 0;
	virtual uint32_t read(void *ptr, uint32_t len) = 0;
	virtual uint32_t write(const void *ptr, uint32_t len) = 0;
};

struct StdioFile : File_impl {
	FILE *_fp;
	StdioFile() : _fp(0) {}
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
	uint32_t write(const void *ptr, uint32_t len) {
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
struct GzipFile : File_impl {
	gzFile _fp;
	GzipFile() : _fp(0) {}
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
	uint32_t write(const void *ptr, uint32_t len) {
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

#ifdef USE_RWOPS
struct AssetFile: File_impl {
	SDL_RWops *_rw;
	AssetFile() : _rw(0) {}
	bool prefixedOpen(const char *prefix, const char *name) {
		char path[MAXPATHLEN];
		snprintf(path, sizeof(path), "%s%s", prefix, name);
		_rw = SDL_RWFromFile(path, "rb");
		if (!_rw) {
			// try uppercase
			char fixedPath[MAXPATHLEN];
			{
				int i = 0;
				for (; path[i] && i < MAXPATHLEN - 1; ++i) {
					fixedPath[i] = path[i];
					if (i < strlen(prefix)) {
						continue;
					}
					if (fixedPath[i] >= 'a' && fixedPath[i] <= 'z') {
						fixedPath[i] += 'A' - 'a';
					}
				}
				fixedPath[i] = 0;
			}
			_rw = SDL_RWFromFile(fixedPath, "rb");
		}
		return _rw != 0;
	}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		return prefixedOpen("", path) || prefixedOpen("/sdcard/flashback/", path);
	}
	void close() {
		if (_rw) {
			SDL_RWclose(_rw);
			_rw = 0;
		}
	}
	uint32_t size() {
		if (_rw) {
			return SDL_RWsize(_rw);
		}
		return 0;
	}
	void seek(int32_t off) {
		if (_rw) {
			SDL_RWseek(_rw, off, RW_SEEK_SET);
		}
	}
	uint32_t read(void *ptr, uint32_t len) {
		if (_rw) {
			const int count = SDL_RWread(_rw, ptr, 1, len);
			if (count != len) {
				_ioErr = true;
			}
		}
		return 0;
	}
	uint32_t write(const void *ptr, uint32_t len) {
		_ioErr = true;
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
	_impl = new StdioFile;
	char *path = fs->findPath(filename);
	if (path) {
		debug(DBG_FILE, "Open file name '%s' mode '%s' path '%s'", filename, mode, path);
		bool ret = _impl->open(path, mode);
		free(path);
		return ret;
	}
#ifdef USE_RWOPS
	if (mode[0] == 'r') {
		_impl = new AssetFile;
		return _impl->open(filename, mode);
	} else if (mode[0] == 'w') {
		bool ret = false;
		char *prefPath = SDL_GetPrefPath("org.cyxdown", "fb");
		if (prefPath) {
			char path[MAXPATHLEN];
			snprintf(path, sizeof(path), "%s/%s", prefPath, filename);
			_impl = new StdioFile;
			ret = _impl->open(path, mode);
			SDL_free(prefPath);
		}
		return ret;
	}
#endif
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
		_impl = new GzipFile;
		++mode;
	}
#endif
	if (!_impl) {
		_impl = new StdioFile;
	}
	char path[MAXPATHLEN];
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

uint32_t File::write(const void *ptr, uint32_t len) {
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
