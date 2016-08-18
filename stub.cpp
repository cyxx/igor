/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef _WIN32
#include <windows.h>
#define DYNLIB_SYMBOL __declspec(dllexport)
#else
#define DYNLIB_SYMBOL
#endif
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "mixer.h"
#include "stub.h"
#include "util.h"

char *g_err;

struct GameStub_Igor : GameStub {

	Game *_g;

	virtual int init(int argc, const char *argv[], char *errBuf) {
		if (argc < 2) {
			return -1;
		}
		g_err = errBuf;
		_g = new Game(argv[0]);
		if (argv[1]) {
			const int part = atoi(argv[1]);
			_g->init(part);
		} else {
			_g->init(900);
			_g->loadState(0);
		}
		return 0;
	}
	virtual void quit() {
		_g->saveState(0);
		delete _g;
	}
	virtual StubBackBuffer getBackBuffer() {
		StubBackBuffer buf;
		buf.w = 320;
		buf.h = 200;
		buf.ptr = (uint8_t *)_g->_mem._vga;
		buf.palPtr = _g->_palBuf;
		buf.palDirty = &_g->_palDirty;
		buf.cursor = &_g->_cursorVisible;
		return buf;
	}
#ifdef USE_MIXER_IMPL
	virtual void setMixerImpl(MixerImpl *m) {
		_g->_mix._impl = m;
	}
#else
	virtual StubMixProc getMixProc(int rate, int fmt, void (*lock)(int)) {
		StubMixProc mix;
		memset(&mix, 0, sizeof(mix));
		mix.proc = &Mixer::mixCb;
		mix.data = &_g->_mix;
		_g->_mix.setFormat(rate, fmt);
		_g->_mix._lock = lock;
		return mix;
	}
#endif
	virtual void queueMousePos(int x, int y) {
		_g->setMousePos(x, y);
	}
	virtual void queueKey(int keycode, int pressed) {
		switch (keycode) {
		case kKeyMouseLeftButton:
			_g->setMouseButton(0, pressed);
			break;
		case kKeyMouseRightButton:
			_g->setMouseButton(1, pressed);
			break;
		case kKeyEscape:
			_g->setKeyPressed(1, pressed);
			break;
		case kKeySpace:
			_g->setKeyPressed(25, pressed);
			break;
		case kKeyEnter:
			_g->setKeyPressed(28, pressed);
			break;
		case kKeyP:
			_g->setKeyPressed(57, pressed);
			break;
		case kKeyM:
			if (pressed == 0) {
				_g->cheat_exitMaze();
			}
			break;
		}
	}
	virtual int doTick() {
		for (int i = 0; i < 2; ++i) {
			_g->runTick();
		}
		return _g->_quit;
	}
	virtual void draw() {
		// no-op
	}
};

extern "C" {
	DYNLIB_SYMBOL GameStub *GameStub_create() {
		return new GameStub_Igor;
	}
};
