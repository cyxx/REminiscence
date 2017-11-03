
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <sys/param.h>
#include "dynlib.h"

#ifdef WIN32
struct DynLib_impl {
	HINSTANCE _dl;
	DynLib_impl(const char *name) {
		char dllname[MAXPATHLEN];
		snprintf(dllname, sizeof(dllname), "%s.dll", name);
		_dl = LoadLibrary(dllname);
	}
	~DynLib_impl() {
		if (_dl) {
			FreeLibrary(_dl);
			_dl = 0;
		}
	}
	void *getSymbol(const char *name) {
		return (void *)GetProcAddress(_dl, name);
	}
};
#else
struct DynLib_impl {
	void *_dl;
	DynLib_impl(const char *name) {
		char soname[MAXPATHLEN];
		snprintf(soname, sizeof(soname), "%s.so", name);
		_dl = dlopen(soname, RTLD_LAZY);
	}
	~DynLib_impl() {
		if (_dl) {
			dlclose(_dl);
		}
	}
	void *getSymbol(const char *name) {
		return dlsym(_dl, name);
	}
};
#endif

DynLib::DynLib(const char *name) {
	_impl = new DynLib_impl(name);
}

DynLib::~DynLib() {
	delete _impl;
}

void *DynLib::getSymbol(const char *name) {
	return _impl->getSymbol(name);
}
