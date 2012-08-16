
#ifndef GL_CURSOR_H__
#define GL_CURSOR_H__

#include <stdint.h>

struct Cursor {

	GLuint _tex;
	int _xPos, _yPos;
	float _xScale, _yScale;
	int _currentCursor;
	int _drawCounter;

	Cursor();
	~Cursor();

	void initCursors();
	void setPosition(int x, int y);
	void setScale(float sx, float sy);
	void draw();
};

#endif
