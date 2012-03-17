
#ifndef GL_TEXTURE_H__
#define GL_TEXTURE_H__

#include <stdint.h>

struct Texture {

	GLuint _id;
	int _w, _h;
	GLfloat _u, _v;
	uint16_t _8to565[256];
	uint16_t *_buf;

	Texture();
	~Texture();

	void uploadData(const uint8_t *data, int w, int h);
	void setPalette(const uint8_t *data);

	static void init();
};

#endif
