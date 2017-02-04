/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef FILE_H__
#define FILE_H__

#include <stdint.h>
#include <stdio.h>

struct AssetFileImpl;

struct AssetFile {
	AssetFileImpl *_impl;

	AssetFile();
	~AssetFile();

	bool open(const char *filename, const char *path);
	void close();
	int seek(int offset);
	int size();
	int read(void *p, int len);

	uint8_t readByte() {
		uint8_t b = 0;
		read(&b, sizeof(b));
		return b;
	}
	uint16_t readUint16LE() {
		uint8_t lo = readByte();
		uint8_t hi = readByte();
		return (hi << 8) | lo;
	}
	uint32_t readUint32LE() {
		uint16_t lo = readUint16LE();
		uint16_t hi = readUint16LE();
		return (hi << 16) | lo;
	}
};

struct StateFile {
	FILE *_fp;

	StateFile();
	~StateFile();

	bool open(const char *filename, const char *path, const char *mode);
	void close();
	int read(void *p, int len);
	int write(void *p, int size);

	uint32_t readUint32LE() {
		uint8_t buf[4];
		read(buf, sizeof(buf));
		return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	}
	void writeUint32LE(uint32_t n) {
		uint8_t buf[4] = { uint8_t(n), uint8_t(n >> 8), uint8_t(n >> 16), uint8_t(n >> 24) };
		write(buf, sizeof(buf));
	}
};

#endif
