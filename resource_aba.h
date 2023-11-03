
#ifndef RESOURCE_ABA_H__
#define RESOURCE_ABA_H__

#include "file.h"

struct FileSystem;

struct ResourceAbaEntry {
	char name[14];
	uint32_t offset;
	uint32_t compressedSize;
	uint32_t size;
	int fileIndex;
};

struct ResourceAba: ResourceArchive {

	static const int TAG = 0x442E4D2E;

	FileSystem *_fs;
	File _f[3];
	int _filesCount;
	ResourceAbaEntry *_entries;
	int _entriesCount;

	ResourceAba(FileSystem *fs);
	~ResourceAba();

	void readEntries(const char *aba);
	const ResourceAbaEntry *findEntry(const char *name) const;

	virtual bool hasEntry(const char *name) const { return findEntry(name) != 0; }
	virtual uint8_t *loadEntry(const char *name, uint32_t *size = 0);
};

#endif // RESOURCE_ABA_H__
