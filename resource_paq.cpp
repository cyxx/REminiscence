
#include "resource_paq.h"
#include "unpack.h"
#include "util.h"

static const int DEFAULT_READ_BUFFER_SIZE = 8192;

ResourcePaq::ResourcePaq(FileSystem *fs)
	: _fs(fs), _filesCount(0) {
	_readBufferSize = DEFAULT_READ_BUFFER_SIZE;
	_readBuffer = (uint8_t *)malloc(_readBufferSize);
	if (!_readBuffer) {
		warning("Failed to allocate %d bytes for PAQ read buffer", _readBufferSize);
	}
}

ResourcePaq::~ResourcePaq() {
	free(_readBuffer);
}

bool ResourcePaq::open() {
	for (int i = 0; i < _namesCount; ++i) {
		assert(_filesCount < 16);
		char name[16];
		snprintf(name, sizeof(name), "%s.PAQ", _names[i]);
		if (!_f[_filesCount].open(name, "rb", _fs)) {
			warning("Fail to open '%s'", name);
		} else {
			++_filesCount;
		}
	}
	return _filesCount == _namesCount;
}

static uint32_t djb2(const char *s) {
	uint32_t hash = 5381;
	for (int i = 0; s[i]; ++i) {
		char c = s[i];
		if (c >= 'a' && c <= 'z') {
			c += 'A' - 'a';
		}
		hash = (hash * 33) ^ ((uint8_t)c);
	}
	return hash;
}

static int comparePaqHashEntry(const void *a, const void *b) {
	const uint32_t hash1 = *(const uint32_t *)a;
	const uint32_t hash2 = ((const ResourcePaqEntry *)b)->hash;
	if (hash1 == hash2) {
		return 0;
	}
	return (hash1 < hash2) ? -1 : 1;
}

const ResourcePaqEntry *ResourcePaq::findEntry(const char *name) const {
	const uint32_t hash = djb2(name);
	debug(DBG_PAQ, "0x%08x", hash);
	return (const ResourcePaqEntry *)bsearch(&hash, _entries, _entriesCount, sizeof(ResourcePaqEntry), comparePaqHashEntry);
}

uint8_t *ResourcePaq::loadEntry(const char *name, uint32_t *size) {
	const ResourcePaqEntry *e = findEntry(name);
	if (!e) {
		warning("PAQ entry '%s' not found", name);
		return 0;
	}
	File &f = _f[e->paq];
	debug(DBG_PAQ, "seeking to entry %d file %s", e->num, _names[e->paq]);
	f.seek(e->num * 12);
	const uint32_t offset       = f.readUint32LE();
	const uint32_t compressed   = f.readUint32LE();
	const uint32_t uncompressed = f.readUint32LE();
	debug(DBG_PAQ, "0x%x compressed %d uncompressed %d", offset, compressed, uncompressed);
	if (size) {
		*size = uncompressed;
	}
	uint8_t *dst = (uint8_t *)malloc(uncompressed);
	if (!dst) {
		warning("Failed to allocate %d bytes to uncompress PAQ entry", uncompressed);
		return 0;
	}
	f.seek(offset);
	if (compressed == uncompressed) {
		f.read(dst, uncompressed);
	} else {
		if (compressed > _readBufferSize) {
			uint8_t *buffer = (uint8_t *)realloc(_readBuffer, compressed);
			if (!buffer) {
				warning("Failed to reallocate PAQ read buffer");
				return 0;
			}
			_readBuffer = buffer;
			_readBufferSize = compressed;
		}
		f.read(_readBuffer, compressed);
		pc98_unpack(dst, uncompressed, _readBuffer, compressed);
		const char *ext = strrchr(name, '.');
		if (ext && strcasecmp(ext + 1, "PGE") == 0) {
			// PGE files contain x86 code to decompresses themselves.
			// Skip that x86 section and decompress the payload directly.
			const uint16_t pge_size = dst[10] | (dst[11] << 8);
			const uint16_t pge_offs = dst[17] | (dst[18] << 8);
			debug(DBG_PAQ, "PGE data size %d offset 0x%x", pge_size, pge_offs);
			uint8_t *pge_dst = (uint8_t *)malloc(pge_size);
			if (!pge_dst) {
				warning("Failed to allocate %d bytes to uncompress PGE entry", pge_size);
				free(dst);
				return 0;
			}
			pc98_unpack(pge_dst, pge_size, dst + pge_offs, uncompressed - pge_offs);
			assert(pge_size > 0x5f);
			memmove(pge_dst, pge_dst + 0x5f, pge_size - 0x5f);
			*size = pge_size - 0x5f;
			free(dst);
			dst = pge_dst;
		} else if (strcasecmp(name, "GLOBAL.ICN") == 0) {
			// DEMO entry number 3 contains the icons, with self decompressing x86 code,
			// data being compressed 4 times...
			int bufsize = uncompressed;
			static const int kBufferSize = 16 * 1024;
			uint8_t *buf = dst;
			uint8_t *tmp = (uint8_t *)malloc(kBufferSize);
			if (!tmp) {
				warning("Failed to allocate %d bytes to uncompress GLOBAL.ICN", kBufferSize);
				free(dst);
				return 0;
			}
			while (1) {
				const uint16_t icn_size   = (buf[8]  << 8) | buf[7];
				const uint16_t icn_offset = (buf[15] << 8) | buf[14];
				if (icn_offset != 0x112) {
					memmove(tmp, tmp + 0x7D0, kBufferSize - 0x7D0);
					*size = kBufferSize - 0x7D0;
					free(dst);
					return tmp;
				}
				assert(bufsize <= kBufferSize);
				pc98_unpack(tmp, icn_size, buf + icn_offset, bufsize - icn_offset);
				assert(_readBufferSize >= icn_size);
				memcpy(_readBuffer, tmp, icn_size);
				buf = _readBuffer;
				bufsize = icn_size;
			}
		}
	}
	return dst;
}
