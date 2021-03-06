/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef USE_GLES
#include <GLES/gl.h>
#else
#include <SDL_opengl.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl_texture.h"

static bool _npotTex = false;

static bool hasExtension(const char *exts, const char *name) {
	const char *p = strstr(exts, name);
	if (p) {
		p += strlen(name);
		return *p == ' ' || *p == 0;
	}
	return false;
}

void Texture::init() {
	const char *exts = (const char *)glGetString(GL_EXTENSIONS);
	if (hasExtension(exts, "GL_ARB_texture_non_power_of_two")) {
		_npotTex = true;
	}
}

Texture::Texture() {
	_id = -1;
	memset(_8to565, 0, sizeof(_8to565));
	_buf = 0;
	_buf2 = 0;
}

Texture::~Texture() {
	if (_id != (GLuint)-1) {
		glDeleteTextures(1, &_id);
	}
	free(_buf);
	free(_buf2);
}

static int roundPow2(int sz) {
	if (sz != 0 && (sz & (sz - 1)) == 0) {
		return sz;
	}
	int textureSize = 1;
	while (textureSize < sz) {
		textureSize <<= 1;
	}
	return textureSize;
}

static void convertTexture(const uint8_t *src, int w, int h, const uint16_t *clut, uint16_t *dst, int dstPitch) {
	assert((w & 1) == 0);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; x += 2) {
			*(uint32_t *)&dst[x] = clut[src[x]] | (clut[src[x + 1]] << 16);
		}
		dst += dstPitch;
		src += w;
	}
}

void Texture::uploadData(const uint8_t *data, int w, int h) {
	if (!_buf) {
		_w = _npotTex ? w : roundPow2(w);
		_h = _npotTex ? h : roundPow2(h);
		_buf = (uint16_t *)calloc(_w * _h, sizeof(uint16_t));
		if (!_buf) {
			return;
		}
		_buf2 = (uint16_t *)calloc(_w * _h, sizeof(uint16_t));
		_u = w / (float)_w;
		_v = h / (float)_h;
		convertTexture(data, w, h, _8to565, _buf, _w);
		if (_buf2) {
			memcpy(_buf2, _buf, _w * _h * sizeof(uint16_t));
		}
		glGenTextures(1, &_id);
		glBindTexture(GL_TEXTURE_2D, _id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _w, _h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, _buf);
	} else {
		convertTexture(data, w, h, _8to565, _buf, _w);
		glBindTexture(GL_TEXTURE_2D, _id);
		if (_buf2) {
			static const int H = 8;
			assert((h & (H - 1)) == 0);
			for (int y = 0; y < h; y += H) {
				const int offset = y * _w;
				if (memcmp(_buf + offset, _buf2 + offset, _w * H * sizeof(uint16_t)) != 0) {
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, _w, H, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, _buf + offset);
				}
			}
			memcpy(_buf2, _buf, _w * _h * sizeof(uint16_t));
			return;
		}
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _w, _h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, _buf);
	}
}

void Texture::setPalette(const uint8_t *data) {
	const uint8_t *p = data;
	for (int i = 0; i < 256; ++i) {
		int r = *p++;
		r = (r << 2) | (r & 3);
		int g = *p++;
		g = (g << 2) | (g & 3);
		int b = *p++;
		b = (b << 2) | (b & 3);
//		r = g = b = i;
		_8to565[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
	}
}

#ifndef USE_GLES
static void emitQuadTex(int x1, int y1, int x2, int y2, GLfloat u1, GLfloat v1, GLfloat u2, GLfloat v2) {
	glBegin(GL_QUADS);
		glTexCoord2f(u1, v1);
		glVertex2i(x1, y1);
		glTexCoord2f(u2, v1);
		glVertex2i(x2, y1);
		glTexCoord2f(u2, v2);
		glVertex2i(x2, y2);
		glTexCoord2f(u1, v2);
		glVertex2i(x1, y2);
	glEnd();
}
#endif

void Texture::draw(int w, int h) {
	const int x1 = 0;
	const int y1 = 0;
	const int x2 = x1 + w;
	const int y2 = y1 + h;
	const float u1 = 0.;
	const float v1 = 0.;
	const float u2 = _u;
	const float v2 = _v;
#ifdef USE_GLES
	const GLfloat vertices[] = {
		x1, y2, 0,
		x1, y1, 0,
		x2, y1, 0,
		x2, y2, 0
	};
	const GLfloat uv[] = {
		u1, v1,
		u1, v2,
		u2, v2,
		u2, v1
	};
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, uv);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#else
	emitQuadTex(x1, y1, x2, y2, u1, v1, u2, v2);
#endif
}
