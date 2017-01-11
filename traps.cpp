/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "real.h"

void Game::trap_setPalette_240_16(int argc, int *argv) {
	readData(_mem._dataSeg + 0xE15E + 240 * 3, 221, 0x2373, 16 * 3);
}

void Game::trap_handleOptionsMenu(int argc, int *argv) {
	// TODO:
}

void Game::trap_playMusic(int argc, int *argv) {
	const int num = argv[1];
	debug(DBG_TRAPS, "Game::trap_playMusic %d", num);
	_mix.playTrack(num + 1);
}

void Game::trap_getMusicState(int argc, int *argv) {
	const int status = _mix.isTrackPlaying() ? 0x300 : 0;
	_script._regs[reg_ax].val.w = status;
}

void Game::trap_playSound(int argc, int *argv) {
	const int num = argv[0];
	const int type = argv[1];
	debug(DBG_TRAPS, "Game::trap_playSound %d %d", type, num);
	int offset = 0;
	switch (type) {
	case 0: // speech
		seekData(221, 0xB52 + num * 4);
		offset = _exe._f.readUint32LE();
		break;
	case 1: // sfx
		seekData(221, 0x9C2 + (num - 1) * 4);
		offset = _exe._f.readUint32LE();
		break;
	}
	_mix.playSound(num, type, offset);
	_mem.setSoundHandle(1);
	_pollSoundPlaying = true;
}

void Game::trap_stopSound(int argc, int *argv) {
	_mix.stopSound();
	_mem.setSoundHandle(0);
	_pollSoundPlaying = false;
}

void Game::trap_setPaletteRange(int argc, int *argv) {
	const int start = argv[0] & 255;
	const int end = argv[1] & 255;
	assert(start >= 0 && start <= end && end < 256);
	memcpy(_palBuf + start * 3, _mem._dataSeg + 0xE45E + start * 3, (end - start + 1) * 3);
	_palDirty = true;
	_script._waitTicks = 1;
}

void Game::trap_setPalette(int argc, int *argv) {
	memcpy(_palBuf, _mem._dataSeg + 0xE45E, 256 * 3);
	_palDirty = true;
	_script._waitTicks = 1;
}

void Game::trap_quitGame(int argc, int *argv) {
	_quit = true;
}

void Game::trap_setCursorPos(int argc, int *argv) {
}

void Game::trap_showCursor(int argc, int *argv) {
	_mem._dataSeg[0x3209] = 1;
	_mem._dataSeg[0xEAB9] = 1;
	_cursorVisible = true;
}

void Game::trap_hideCursor(int argc, int *argv) {
	_mem._dataSeg[0x3209] = 0;
	_mem._dataSeg[0xEAB9] = 0;
	_cursorVisible = false;
}

void Game::trap_setPalette_208_32(int argc, int *argv) {
	readData(_mem._dataSeg + 0xE15E + 208 * 3, 228, 0xE798, 32 * 3);
}

void Game::trap_checkStack(int argc, int *argv) {
	const int size = _script._regs[reg_ax].val.w;
	if (size > _script.sp()) {
		error("stack overflow %d %04x", size, _script.sp());
	}
}

void Game::trap_copyAny(int argc, int *argv) {
	void *dst = _mem.getPtr(argv[2], argv[3]);
	const void *src = _mem.getPtr(argv[0], argv[1]);
	memmove(dst, src, argv[4]);
}

void Game::trap_mulLongInt(int argc, int *argv) {
	uint32_t x1 = (_script._regs[reg_dx].val.w << 16) | _script._regs[reg_ax].val.w;
	uint32_t x2 = (_script._regs[reg_bx].val.w << 16) | _script._regs[reg_cx].val.w;
	x1 *= x2;
	_script._regs[reg_dx].val.w = x1 >> 16;
	_script._regs[reg_ax].val.w = x1 & 0xFFFF;
}

void Game::trap_loadString(int argc, int *argv) {
	debug(DBG_TRAPS, "Game::trap_loadString %d:%x %d:%x", argv[0], argv[1], argv[2], argv[3]);
	uint8_t *dst = (uint8_t *)_mem.getPtr(argv[0], argv[1]);
	readString(argv[2], argv[3]);
	const int count = _sBuf[0];
	memcpy(dst, _sBuf, count + 1);
}

void Game::trap_copyString(int argc, int *argv) {
	debug(DBG_TRAPS, "Game::trap_copyString %d:%x %d:%x count %d", argv[0], argv[1], argv[2], argv[3], argv[4]);
	uint8_t *dst = (uint8_t *)_mem.getPtr(argv[2], argv[3]);
	readString(argv[0], argv[1]);
	int count = _sBuf[0];
	if (count > argv[4]) {
		count = argv[4];
	}
	memcpy(dst, _sBuf, count + 1);
}

void Game::trap_concatString(int argc, int *argv) {
	debug(DBG_TRAPS, "Game::trap_concatString %d:%x %d:%x", argv[0], argv[1], argv[2], argv[3]);
	uint8_t *dst = (uint8_t *)_mem.getPtr(argv[0], argv[1]);
	readString(argv[2], argv[3]);
	int count = _sBuf[0] + dst[0];
	if (count > 255) {
		count = 255;
	}
	memcpy(dst + 1 + dst[0], _sBuf + 1, count - dst[0]);
	dst[0] = count;
}

void Game::trap_loadBitSet(int argc, int *argv) {
	debug(DBG_TRAPS, "Game::trap_loadBitSet seg %d %04X value %d", argv[0], argv[1], argv[2]);
	uint8_t *dst = (uint8_t *)_mem.getPtr(argv[0], argv[1]);
	for (int i = 0; i < 16; ++i) {
		WRITE_LE_UINT16(dst + i * 2, argv[2]);
	}
}

void Game::trap_addBitSet(int argc, int *argv) {
	debug(DBG_TRAPS, "Game::trap_addBitSet seg %d %04X values %d,%d", argv[0], argv[1], argv[2], argv[3]);
	uint8_t *dst = (uint8_t *)_mem.getPtr(argv[0], argv[1]);
	const int b1 = argv[2] & 255;
	const int b2 = argv[3] & 255;
	for (int i = b1; i <= b2; ++i) {
		const int offset = i >> 3;
		dst[offset] |= 1 << (i & 7);
	}
}

void Game::trap_getBitSetOffset(int argc, int *argv) {
	const int value = _script._regs[reg_ax].val.b[0];
	const int b = value >> 3;
	assert(b < 0x20);
	_script._regs[reg_dx].val.w = b;
	_script._regs[reg_ax].val.w = 1 << (value & 7);
	debug(DBG_TRAPS, "Game::trap_getBitSetOffset %X", value);
}

void Game::trap_addReal(int argc, int *argv) {
	realOp2('+', _script._regs);
}

void Game::trap_divReal(int argc, int *argv) {
	realOp2('/', _script._regs);
}

void Game::trap_cmpReal(int argc, int *argv) {
	_script._f = realCmp(_script._regs);
}

void Game::trap_realInt(int argc, int *argv) {
	realInt(_script._regs);
}

void Game::trap_truncReal(int argc, int *argv) {
	realOp1('t', _script._regs);
}

void Game::trap_roundReal(int argc, int *argv) {
	realOp1('r', _script._regs);
}

void Game::trap_getRandomNumber(int argc, int *argv) {
	uint32_t n = _randSeed * 2;
	if (_randSeed > n) {
		n ^= 0x1D872B41;
	}
	_randSeed = n;
	_script._regs[reg_ax].val.w = n % argv[0];
}

void Game::trap_memcpy(int argc, int *argv) {
	debug(DBG_TRAPS, "Game::trap_memcpy %d:%04X %d:%04X count %d", argv[2], argv[3], argv[0], argv[1], argv[4]);
	void *dst = _mem.getPtr(argv[2], argv[3]);
	if (argv[0] < DATA_SEG) {
		readData(dst, argv[0], argv[1], argv[4]);
	} else {
		const void *src = _mem.getPtr(argv[0], argv[1]);
		memcpy(dst, src, argv[4]);
	}
}

void Game::trap_memset(int argc, int *argv) {
	void *dst = _mem.getPtr(argv[0], argv[1]);
	const int count = argv[2];
	memset(dst, argv[3], count);
}

void Game::trap_fixVgaPtr(int argc, int *argv) {
	_script._regs[reg_ax].val.w = argv[0];
	_mem._vgaOffset = argv[2];
	debug(DBG_TRAPS, "Game::trap_fixVgaPtr y=%d", _mem._vgaOffset / 320);
}

void Game::trap_loadActionData(int argc, int *argv) {
	int seg = argv[0];
	if (seg == 0) {
		seg = _mem._actionSeg;
	}
	readData(_mem._actionData + argv[3], seg, argv[1], argv[2]);
}

void Game::trap_setPaletteColor(int argc, int *argv) {
	const int num = (argv[0] & 255) * 3;
	assert(num >= 0 && num < 256 * 3);
	for (int i = 0; i < 3; ++i) {
		_palBuf[num + i] = argv[1 + i];
	}
	_palNum = num + 3;
	_palDirty = true;
}

void Game::trap_loadImageData(int argc, int *argv) {
	void *dst = _mem.getPtr(argv[2], argv[3]);
	readData(dst, argv[0], argv[1], argv[4]);
}

void Game::trap_setActionData(int argc, int *argv) {
	_mem._actionSeg = argv[0];
	_script._regs[reg_ax].val.w = ACTION_SEG;
	switch (_mem._actionSeg) {
	case 62: // part_67
		readData(_mem._actionData + 0x279B, _mem._actionSeg, 0x279B, 0x3D8C - 0x279B);
		break;
	case 63: // part_66
		readData(_mem._actionData + 0x2860, _mem._actionSeg, 0x2860, 0x3EF5 - 0x2860);
		break;
	case 65: // part_65
		readData(_mem._actionData + 0x25F2, _mem._actionSeg, 0x25F2, 0x3A6F - 0x25F2);
		break;
	case 67: // part_64
		readData(_mem._actionData + 0x2428, _mem._actionSeg, 0x2428, 0x388D - 0x2428);
		break;
	case 69: // part_63
		readData(_mem._actionData + 0x2D17, _mem._actionSeg, 0x2D17, 0x417C - 0x2D17);
		break;
	case 71: // part_62
		readData(_mem._actionData + 0x2D8C, _mem._actionSeg, 0x2D8C, 0x41F1 - 0x2D8C);
		break;
	case 73: // part_61
		readData(_mem._actionData + 0x2D05, _mem._actionSeg, 0x2D05, 0x416A - 0x2D05);
		break;
	case 75: // part_60
		readData(_mem._actionData + 0x2B87, _mem._actionSeg, 0x2B87, 0x3FD4 - 0x2B87);
		break;
	case 77: // part_59
		readData(_mem._actionData + 0x279C, _mem._actionSeg, 0x279C, 0x3EF5 - 0x279C);
		break;
	case 79: // part_58
		readData(_mem._actionData + 0x2B98, _mem._actionSeg, 0x2B98, 0x3FE5 - 0x2B98);
		break;
	case 81: // part_57
		readData(_mem._actionData + 0x2839, _mem._actionSeg, 0x2839, 0x3C86 - 0x2839);
		break;
	case 83: // part_56
		readData(_mem._actionData + 0x2BA6, _mem._actionSeg, 0x2BA6, 0x3FF3 - 0x2BA6);
		break;
	case 85: // part_55
		readData(_mem._actionData + 0x27C7, _mem._actionSeg, 0x27C7, 0x3C14 - 0x27C7);
		break;
	case 87: // part_54
		readData(_mem._actionData + 0x266D, _mem._actionSeg, 0x266D, 0x3AA2 - 0x266D);
		break;
	case 89: // part_53
		readData(_mem._actionData + 0x27EF, _mem._actionSeg, 0x27EF, 0x3C24 - 0x27EF);
		break;
	case 91: // part_52
		readData(_mem._actionData + 0x26C9, _mem._actionSeg, 0x26C9, 0x3AFE - 0x26C9);
		break;
	case 93: // part_51
		readData(_mem._actionData + 0x26C5, _mem._actionSeg, 0x26C5, 0x3AFA - 0x26C5);
		break;
	default:
		warning("Game::trap_setActionData _mem._actionSeg %d", _mem._actionSeg);
		break;
	}
}

void Game::trap_setDialogueData(int argc, int *argv) {
	_mem._dialogueSeg = argv[0];
	_script._regs[reg_ax].val.w = DIALOGUE_SEG;
}

void Game::trap_loadDialogueData(int argc, int *argv) {
	int seg = argv[0];
	if (seg == 0) {
		seg = _mem._dialogueSeg;
	}
	readData(_mem._dialogueData + argv[3], seg, argv[1], argv[2]);
}

void Game::trap_setPaletteData(int argc, int *argv) {
	_mem._paletteSeg = argv[0];
	_script._regs[reg_ax].val.w = _mem._paletteSeg;
}

void Game::trap_waitInput(int argc, int *argv) {
	_script._waitTicks = 4;
	_script._regs[reg_ax].val.b[0] = _input != 0;
}

void Game::trap_setMouseRange(int argc, int *argv) {
	_yMinCursor = argv[0];
	_yMaxCursor = argv[1];
}

void Game::trap_waitSound(int argc, int *argv) {
	if (_mem.getSoundHandle() != 0) {
		_script._waitTicks = 1;
		_yield = 1;
	}
}
