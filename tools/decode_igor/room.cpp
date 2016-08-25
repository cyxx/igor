
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "room.h"
extern "C" {
#include "tga.h"
}
#include "util.h"

bool checkRoomString(const uint8_t *src, int sz) {
	if (sz > 96) {
		return false;
	}
#if 0
	for (int i = 0; i < sz; ++i) {
		const uint8_t code = *src++;
		if ((code >= 0xAE && code <= 0xC7) || (code >= 0xCE && code <= 0xE7)) {
			continue;
		} else if (code >= 0xE8 && code <= 0xEE) {
			continue;
		} else if (code == 0x9A) {
			continue;
		}
		return false;
	}
#endif
	return true;
}

void decodeRoomString(const uint8_t *src, char *dst, int sz) {
	for (int i = 0; i < sz; ++i) {
		uint8_t code = *src++;
		if ((code >= 0xAE && code <= 0xC7) || (code >= 0xCE && code <= 0xE7)) {
			code -= 0x6D;
		} else if (code > 0xE7) {
			switch (code) {
			case 0xE8:
				code = 0xA0;
				break;
			case 0xE9:
				code = 0x82;
				break;
			case 0xEA:
				code = 0xA1;
				break;
			case 0xEB:
				code = 0xA2;
				break;
			case 0xEC:
				code = 0xA3;
				break;
			case 0xED:
				code = 0xA4;
				break;
			case 0xEE:
				code = 0xA5;
				break;
			}
		}
		*dst++ = (char)code;
	}
}

static void decodeMask(const uint8_t *src, uint8_t *dst) {
	int sz = 320 * 144;
	while (sz != 0) {
		uint8_t b = *src++;
		int len = READ_LE_UINT16(src); src += 2;
		if (len > sz) {
			len = sz;
		}
		memset(dst, b, len);
		dst += len;
		sz -= len;
	}
}

static const int kMinRoomDataSize = 320 * 144 + 320 + 432;

void decodeRoomData(int num, const uint8_t *src, int size) {
	char name[32];
	if (size < kMinRoomDataSize) {
		fprintf(stdout, "skipping room %d: size is too small %d\n", num, size);
		return;
	}
	if (num == 10 || num == 12 || num == 15 || num == 146) {
		fprintf(stdout, "skipping room %d: no assets\n", num);
		return;
	}
	const uint8_t *end = src + size;
	// .WLK
	src += 320 + 432; // _walkXScaleRoom + _walkYScaleRoom
	// .TXT
	const uint8_t *txt = src;
	for (int i = 0; i < 2; ++i) {
		while (*src != 0xF6 && src < end) {
			++src;
		}
		++src;
	}
	if (src > end) {
		return;
	}
	const uint8_t *bmp[2];
	// .IMG
	if (src + 320 * 144 > end) {
		return;
	}
	bmp[0] = src;
	src += 320 * 144;
	// .PAL
	if (src + 240 * 3 > end) {
		return;
	}
	bmp[1] = src;
	src += 240 * 3;
	// .MSK

	// .BOX

	// dump background
	snprintf(name, sizeof(name), "assets/room%03d.tga", num);
	TgaFile *tga = tgaOpen(name, 320, 144, 24);
	tgaSetLookupColorTable(tga, bmp[1]);
	tgaWritePixelsData(tga, bmp[0], 320 * 144);
	tgaClose(tga);
	// dump texts
	dumpRoomString(num, txt, end);
}

void dumpRoomString(int num, const uint8_t *txt, const uint8_t *end) {
	char name[32];
	snprintf(name, sizeof(name), "room%03d.txt", num);
	File f;
	if (!f.open(name, "assets", "w")) {
		fprintf(stderr, "Unable to open '%s'\n", name);
		return;
	}
	static const int indexes[] = { -1, 200 };
	for (int i = 0; i < 2; ++i) {
		uint8_t code = *txt++;
		int index = indexes[i];
		while (code != 0xF6 && txt < end) {
			if (code == 0xF4) {
				++index;
			}
			int len = *txt++;
			if (len != 0) {
				char buf[512];
				int count = snprintf(buf, sizeof(buf), "%03d: ", index);
				decodeRoomString(txt, buf + count, len);
				strcpy(buf + count + len, "\n");
				f.write(buf, strlen(buf));
				txt += len;
			}
			code = *txt++;
		}
	}
}

void dumpRoomImage(int num, const uint8_t *src, int size) {
	const int h = (size - 768) / 320;
	char name[32];
	snprintf(name, sizeof(name), "assets/room%03d.tga", num);
	TgaFile *tga = tgaOpen(name, 320, h, 24);
	if (tga) {
		tgaSetLookupColorTable(tga, src);
		tgaWritePixelsData(tga, src + 768, 320 * h);
		tgaClose(tga);
	}
}
