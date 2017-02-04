/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <sys/param.h>
#include "file.h"

#ifndef USE_ASSETFILE_IMPL
struct AssetFileImpl {
	FILE *_fp;

	bool open(const char *path) {
		_fp = fopen(path, "rb");
		return _fp != 0;
	}
	void close() {
		if (_fp) {
			fclose(_fp);
			_fp = 0;
		}
	}
	int seek(int offset) {
		assert(_fp);
		return fseek(_fp, offset, SEEK_SET);
	}
	int size() {
		assert(_fp);
		const long pos = ftell(_fp);
		fseek(_fp, 0, SEEK_END);
		const int filesize = ftell(_fp);
		fseek(_fp, pos, SEEK_SET);
		return filesize;
	}
	int read(void *p, int len) {
		assert(_fp);
		return fread(p, 1, len, _fp);
	}
};

AssetFile::AssetFile() {
	_impl = new AssetFileImpl();
}

AssetFile::~AssetFile() {
	_impl->close();
	delete _impl;
}

bool AssetFile::open(const char *filename, const char *path) {
	_impl->close();
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%s/%s", path, filename);
	return _impl->open(filepath);
}

void AssetFile::close() {
	_impl->close();
}

int AssetFile::seek(int offset) {
	return _impl->seek(offset);
}

int AssetFile::size() {
	return _impl->size();
}

int AssetFile::read(void *p, int len) {
	return _impl->read(p, len);
}
#endif

StateFile::StateFile()
	: _fp(0) {
}

StateFile::~StateFile() {
	if (_fp) {
		fclose(_fp);
		_fp = 0;
	}
}

bool StateFile::open(const char *filename, const char *path, const char *mode) {
	assert(_fp == 0);
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%s/%s", path, filename);
	_fp = fopen(filepath, mode);
	return _fp != 0;
}

void StateFile::close() {
	if (_fp) {
		fclose(_fp);
		_fp = 0;
	}
}

int StateFile::read(void *p, int len) {
	assert(_fp);
	return fread(p, 1, len, _fp);
}

int StateFile::write(void *p, int len) {
	assert(_fp);
	return fwrite(p, 1, len, _fp);
}
