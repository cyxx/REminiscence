
#ifndef RESOURCE_PAQ_H__
#define RESOURCE_PAQ_H__

#include "file.h"

struct FileSystem;

struct ResourcePaqEntry {
	uint32_t hash;
	uint8_t paq;
	uint8_t num;
};

struct ResourcePaq: ResourceArchive {

	static const char *const _names[];
	static const int _namesCount;
	static const ResourcePaqEntry _entries[];
	static const int _entriesCount;

	FileSystem *_fs;
	File _f[16];
	int _filesCount;
	uint8_t *_readBuffer;
	uint32_t _readBufferSize;

	ResourcePaq(FileSystem *fs);
	virtual ~ResourcePaq();

	bool open();

	const ResourcePaqEntry *findEntry(const char *name) const;

	virtual bool hasEntry(const char *name) const { return hasEntry(name) != 0; }
	virtual uint8_t *loadEntry(const char *name, uint32_t *size = 0);
};

#endif // RESOURCE_PAQ_H__
