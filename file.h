/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef FILE_H__
#define FILE_H__

#include <stdint.h>

struct File_impl {
	bool _ioErr;
	File_impl() : _ioErr(false) {}

	virtual ~File_impl() {}
	virtual bool open(const char *path, const char *mode) = 0;
	virtual void close() = 0;
	virtual uint32_t size() = 0;
	virtual void seek(int off) = 0;
	virtual int read(void *ptr, uint32_t len) = 0;
	virtual int write(void *ptr, uint32_t len) = 0;
};

struct File {
	File();
	~File();

	File_impl *_impl;

	bool open(const char *filename, const char *path, const char *mode = "rb");
	void close();
	bool ioErr() const;
	uint32_t size();
	void seek(int off);
	int read(void *ptr, uint32_t len);
	uint8_t readByte();
	uint16_t readUint16LE();
	uint32_t readUint32LE();
	uint16_t readUint16BE();
	uint32_t readUint32BE();
	int write(void *ptr, uint32_t size);
	void writeByte(uint8_t b);
	void writeUint16LE(uint16_t n);
	void writeUint32LE(uint32_t n);
	void writeUint16BE(uint16_t n);
	void writeUint32BE(uint32_t n);
};

#endif
