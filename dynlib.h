
#ifndef DYNLIB_H__
#define DYNLIB_H__

struct DynLib_impl;

struct DynLib {
	DynLib_impl *_impl;

	DynLib(const char *name);
	~DynLib();

	void *getSymbol(const char *name);
};

#endif // DYNLIB_H__
