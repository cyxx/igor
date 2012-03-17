
#include <assert.h>
#include <string.h>
#include "game.h"
#include "memory.h"

void Game::sub_209_0002() {
	uint8_t buf[206 * 13];
	readData(buf, 209, 0x13BE, sizeof(buf));
	uint8_t *p = (uint8_t *)_mem._ptrs[kPtrScreenLayer3];
	const int offset = 41657;
	for (int y = 0; y < 13; ++y) {
		for (int x = 0; x < 206; ++x) {
			if (buf[y * 206 + x] != 255) {
				p[y * 320 + x + offset] = (buf[y * 206 + x] == 0xF1) ? 0 : 255;
			}
		}
	}
	memcpy((uint8_t *)_mem._vga + _mem._vgaOffset + 41600, p + 41600, 4160);
}

void Game::sub_221_273B() {
	_quit = true;
}

void Game::sub_224_0002(int seg, int ptr) {
	uint8_t *dst = (uint8_t *)_mem._vga + _mem._vgaOffset;
	seekData(seg, ptr);
	int yOffset = _exe._f.readUint16LE() * 320;
	const int h = _exe._f.readUint16LE();
	for (int y = 0; y < h; ++y) {
		const int w = _exe._f.readByte();
		if (w != 0) {
			int offset = yOffset;
			for (int x = 0; x < w; ++x) {
				offset += _exe._f.readByte();
				const int code = _exe._f.readByte();
				if (code <= 0x7F) {
					for (int j = 0; j < code; ++j) {
						const uint8_t color = _exe._f.readByte();
						if (dst[offset] != 0xF0 && dst[offset] != 0xF1) {
							dst[offset] = color;
						}
						++offset;
					}
				} else {
					const uint8_t color = _exe._f.readByte();
					int j = 255;
					do {
						--j;
						if (dst[offset] != 0xF0 && dst[offset] != 0xF1) {
							dst[offset] = color;
						}
						++offset;
					} while (j != code - 1);
				}
			}
		}
		yOffset += 320;
	}
}

void Game::sub_224_0123() {
	uint8_t buf[40];
	readData(buf, 224, 0xC9AC, sizeof(buf));
	if (_yield == -1) {
		_yield = 19;
	}
	const int offset = READ_LE_UINT16(buf + (19 - _yield) * 2) - 1;
	sub_224_0002(224, 0x1D1 + offset);
	_script._waitTicks = READ_LE_UINT16(_mem._stack + _script.sp());
	if (_yield == 0) {
		_script._regs[reg_sp].val.w += 2;
	}
}
