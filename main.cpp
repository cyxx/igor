/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "gl_cursor.h"
#include "gl_texture.h"
#include "stub.h"

static const char *g_caption = "Igor: Objetivo Uikokahonia";
static char g_errBuf[512];
static int g_w, g_h;
static float g_u, g_v;

#ifdef WIN32
struct Alert_impl {
	void print(const char *err) {
		MessageBox(0, err, g_caption, MB_ICONERROR);
	}
};
static const char *g_soName = "igor.dll";
static const char *g_soSym = "GameStub_create";
struct GetStub_impl {
	HINSTANCE _dlso;
	GetStub_impl()
		: _dlso(0) {
	}
	~GetStub_impl() {
		if (_dlso) {
			FreeLibrary(_dlso);
			_dlso = 0;
		}
	}
	void *open(const char *name) {
		_dlso = LoadLibrary(name);
		return _dlso;
	}
	void *getSymbol(const char *name) {
		return (void *)GetProcAddress(_dlso, name);
	}
	GameStub *getGameStub() {
		if (open(g_soName)) {
			void *proc = getSymbol(g_soSym);
			if (proc) {
				typedef GameStub *(*createProc)();
				return ((createProc)proc)();
			}
		}
		return 0;
	}
};
#else
struct Alert_impl {
	void print(const char *err) {
		fprintf(stderr, "%s\n", err);
	}
};
struct GetStub_impl {
	GameStub *getGameStub() {
		return GameStub_create();
	}
};
#endif

static void onExit() {
	if (g_errBuf[0]) {
		Alert_impl().print(g_errBuf);
	}
}

static void lockAudio(int lock) {
	if (lock) {
		SDL_LockAudio();
	} else {
		SDL_UnlockAudio();
	}
}

static void setupAudio(GameStub *stub) {
#ifndef USE_MIXER_IMPL
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = 22050;
	desired.format = AUDIO_S16SYS;
	desired.channels = 1;
	desired.samples = 4096;
	StubMixProc mix = stub->getMixProc(desired.freq, desired.format, lockAudio);
	if (mix.proc) {
		desired.callback = mix.proc;
		desired.userdata = mix.data;
		if (SDL_OpenAudio(&desired, 0) == 0) {
			SDL_PauseAudio(0);
		}
	}
#endif
}

static void queueMouseButton(GameStub *stub, int button, int pressed) {
	switch (button) {
	case SDL_BUTTON_LEFT:
		stub->queueKey(kKeyMouseLeftButton, pressed);
		break;
	case SDL_BUTTON_RIGHT:
		stub->queueKey(kKeyMouseRightButton, pressed);
		break;
	}
}

static void queueKey(GameStub *stub, int key, int pressed) {
	switch (key) {
	case SDLK_ESCAPE:
		stub->queueKey(kKeyEscape, pressed);
		break;
	case SDLK_SPACE:
		stub->queueKey(kKeySpace, pressed);
		break;
	case SDLK_RETURN:
		stub->queueKey(kKeyEnter, pressed);
		break;
	case SDLK_p:
		stub->queueKey(kKeyP, pressed);
		break;
	case SDLK_m:
		stub->queueKey(kKeyM, pressed);
		break;
	}
}

static void drawGL(Texture &tex) {
	glEnable(GL_TEXTURE_2D);
	glViewport(0, 0, g_w, g_h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, g_w, g_h, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);
	if (tex._id != (GLuint)-1) {
		glBindTexture(GL_TEXTURE_2D, tex._id);
		tex.draw(g_w, g_h);
	}
}

static void drawCursor(Cursor &c) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	c.draw();
}

int main(int argc, char *argv[]) {
	GetStub_impl gs;
	GameStub *stub = gs.getGameStub();
	if (!stub) {
		return 0;
	}
	atexit(onExit);
	const char *opts[4];
	opts[0] = (argc >= 2) ? argv[1] : ".";
	opts[1] = (argc >= 3) ? argv[2] : 0;
	opts[2] = (argc >= 4) ? argv[3] : ".";
	opts[3] = (argc >= 5) ? argv[4] : 0;
	if (stub->init(4, opts, g_errBuf) != 0) {
		return 1;
	}
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	setupAudio(stub);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(g_caption, 0);
	StubBackBuffer buf = stub->getBackBuffer();
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	g_w = buf.w * 2;
	g_h = buf.h * 2;
	g_h = g_w * 3 / 4;
	SDL_SetVideoMode(g_w, g_h, 0, SDL_OPENGL | SDL_RESIZABLE);
	g_u = buf.w / (float)g_w;
	g_v = buf.h / (float)g_h;
	Cursor cursor;
	cursor.initCursors();
	cursor.setPosition(g_w / 2, g_h / 2);
	cursor.setScale(g_u, g_v);
	Texture tex;
	Texture::init();
	bool quit = false;
	while (stub->doTick() == 0) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_VIDEORESIZE:
				g_w = ev.resize.w;
				g_h = ev.resize.h;
				SDL_SetVideoMode(g_w, g_h, 0, SDL_OPENGL | SDL_RESIZABLE);
				g_u = buf.w / (float)g_w;
				g_v = buf.h / (float)g_h;
				cursor.setScale(g_u, g_v);
				break;
			case SDL_MOUSEMOTION:
				stub->queueMousePos((int)(ev.motion.x * g_u), (int)(ev.motion.y * g_v));
				cursor.setPosition(ev.motion.x, ev.motion.y);
				break;
			case SDL_MOUSEBUTTONDOWN:
				queueMouseButton(stub, ev.button.button, 1);
				break;
			case SDL_MOUSEBUTTONUP:
				queueMouseButton(stub, ev.button.button, 0);
				break;
			case SDL_KEYDOWN:
				queueKey(stub, ev.key.keysym.sym, 1);
				break;
			case SDL_KEYUP:
				queueKey(stub, ev.key.keysym.sym, 0);
				break;
			default:
				break;
			}
		}
		if (quit) {
			break;
		}
		stub->draw();
		if (*buf.palDirty) {
			tex.setPalette(buf.palPtr);
			*buf.palDirty = false;
		}
		tex.uploadData(buf.ptr, buf.w, buf.h);
		drawGL(tex);
		if (*buf.cursor) {
			drawCursor(cursor);
		}
		SDL_GL_SwapBuffers();
		SDL_Delay(10);
	}
	SDL_PauseAudio(1);
	stub->quit();
	SDL_Quit();
	return 0;
}
