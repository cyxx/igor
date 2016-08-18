/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef STUB_H__
#define STUB_H__

#ifdef USE_MIXER_IMPL

struct MixerImpl;

#else

struct StubMixProc {
	void (*proc)(void *data, uint8_t *buf, int size);
	void *data;
};

#endif

struct StubBackBuffer {
	int w, h;
	uint8_t *ptr; // w*h
	uint8_t *palPtr; // 256*3
	bool *palDirty;
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
#ifdef USE_MIXER_IMPL
	virtual void setMixerImpl(MixerImpl *) = 0;
#else
	virtual StubMixProc getMixProc(int rate, int fmt, void (*lock)(int)) = 0;
#endif
	virtual void queueMousePos(int x, int y) = 0;
	virtual void queueKey(int keycode, int pressed) = 0;
	virtual int doTick() = 0;
	virtual void draw() = 0;
};

extern "C" {
	GameStub *GameStub_create();
}

#endif // STUB_H__
