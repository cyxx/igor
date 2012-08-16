/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef STUB_H__
#define STUB_H__

struct StubMixProc {
	void (*proc)(void *data, uint8_t *buf, int size);
	void *data;
};

struct StubBackBuffer {
	int w, h;
	uint8_t *ptr; // w*h
	uint8_t *pal; // 256*3
	bool *cursor;
};

enum {
	kKeyMouseLeftButton,
	kKeyMouseRightButton,
	kKeyEscape,
	kKeySpace,
	kKeyEnter,
	kKeyP,
	kKeyM
};

struct GameStub {
	virtual int init(int argc, const char *argv[], char *errBuf) = 0;
	virtual void quit() = 0;
	virtual StubBackBuffer getBackBuffer() = 0;
	virtual StubMixProc getMixProc(int rate, int fmt, void (*lock)(int)) = 0;
	virtual void queueMousePos(int x, int y) = 0;
	virtual void queueKey(int keycode, int pressed) = 0;
	virtual int doTick() = 0;
	virtual void draw() = 0;
};

extern "C" {
	GameStub *GameStub_create();
}

#endif // STUB_H__
