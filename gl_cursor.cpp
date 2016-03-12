
#ifdef USE_GLES
#include <GLES/gl.h>
#else
#include <SDL_opengl.h>
#endif
#include <string.h>
#include "gl_cursor.h"

#define MAX_CURSORS 4

Cursor::Cursor() {
	_tex = -1;
	_xPos = 0;
	_yPos = 0;
	_xScale = 1.;
	_yScale = 1.;
	_currentCursor = 0;
	_drawCounter = 0;
}

Cursor::~Cursor() {
	if (_tex != (GLuint)-1) {
		glDeleteTextures(1, &_tex);
	}
}

static const uint8_t kCursorMask[] = {
	0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00,
	0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00
};

static const uint8_t kCursorData[] = {
	0x00, 0xFC, 0x00, 0x04, 0xFD, 0x00, 0x03, 0xFE, 0x02, 0xFB, 0xFC, 0xFD, 0x03, 0x04, 0x05, 0xFE,
	0x02, 0xFD, 0x00, 0x03, 0xFC, 0x00, 0x04, 0x00, 0xFB, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFD, 0xFE,
	0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x05
};

static int getCursorOffset(int i) {
	return (int8_t)kCursorData[i] + 8;
}

void Cursor::initCursors() {
	uint8_t buf[16 * MAX_CURSORS * 16];
	memset(buf, 0, sizeof(buf));
	for (int num = 0; num < MAX_CURSORS; ++num) {
		for (int i = 0; i < 24; ++i) {
			if (kCursorMask[num * 24 + i]) {
				const int y = getCursorOffset(i + 24);
				const int x = getCursorOffset(i) + num * 16;
				buf[y * 16 * MAX_CURSORS + x] = 255;
			}
		}
	}
	glGenTextures(1, &_tex);
	glBindTexture(GL_TEXTURE_2D, _tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	const int w = 16 * MAX_CURSORS;
	const int h = 16;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buf);
}

void Cursor::setPosition(int x, int y) {
	_xPos = x;
	_yPos = y;
}

void Cursor::setScale(float sx, float sy) {
	_xScale = 1. / sx;
	_yScale = 1. / sy;
}

void Cursor::draw() {
	if (_tex != (GLuint)-1) {
		glColor4f(1., 1., 1., 1.);
		glBindTexture(GL_TEXTURE_2D, _tex);
		const int x1 = _xPos - 8 * _xScale;
		const int x2 = _xPos + 8 * _xScale;
		const int y1 = _yPos - 8 * _yScale;
		const int y2 = _yPos + 8 * _yScale;
		const float u1 =  _currentCursor      / (float)MAX_CURSORS;
		const float u2 = (_currentCursor + 1) / (float)MAX_CURSORS;
		const float v1 = 0.;
		const float v2 = 1.;
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
#endif
	}
	++_drawCounter;
	if (_drawCounter == 8) {
		_drawCounter = 0;
		++_currentCursor;
		if (_currentCursor >= MAX_CURSORS) {
			_currentCursor = 0;
		}
	}
}
