/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "memory.h"
#include "util.h"

static const bool _debug = false;

struct NativePtr {
	uint16_t offset;
	int size;
};

static const NativePtr _nativePtrs[kNativePtrsCount + 1] = {
	{ 0xD14A, 320 * 144 }, // _screenLayer1
	{ 0xD14E, 320 * 144 }, // _screenLayer2
	{ 0xD152, 320 * 144 }, // _screenLayer3,
	{ 0xD156, 320 * 144 }, // _screenTextLayer
	{ 0xD162,     65535 }, // _animFramesBuffer
	{ 0xD13A,     13500 }, // _facingBackIgorFrames
	{ 0xD13E,     13500 }, // _facingRightIgorFrames
	{ 0xD142,     13500 }, // _facingFrontIgorFrames
	{ 0xD146,     13500 }, // _facingLeftIgorFrames
	{ 0xD15A,      3696 }, // _igorHeadFrames
	{ 0xD15E,      3000 }, // _igorTempFrames
	{ 0x5EDA,     19200 }, // _inventoryPanelBuffer
	{ 0x5EDE,      3840 }, // _verbsPanelBuffer
	{ 0, 0 }
};

void Memory::setupPtrs() {
	for (int i = 0; i < kNativePtrsCount; ++i) {
		_ptrs[i] = malloc(_nativePtrs[i].size);
		WRITE_LE_UINT32(_dataSeg + _nativePtrs[i].offset, (256 + i) << 16);
	}
	_vga = malloc(320 * 200);
	_vgaOffset = 0;
	_actionData = (uint8_t *)malloc(65535);
	_dialogueData = (uint8_t *)malloc(65535);
	_fontData = (uint8_t *)malloc(94 * 99);
	_inventoryData = (uint8_t *)malloc(58146);
}

void *Memory::getPtr(int seg, uint16_t offset) {
if (_debug) fprintf(stdout, "Memory::getPtr seg %d offset %04X\n", seg, offset);
	switch (seg) {
	case 0xA000:
		return (uint8_t *)_vga + _vgaOffset + offset;
	case STACK_SEG:
		assert(offset < sizeof(_stack));
		return _stack + offset;
	case ACTION_SEG:
		return _actionData + offset;
	case INVENTORY_SEG:
		assert(offset >= 0x698);
		return _inventoryData + offset - 0x698;
	case DIALOGUE_SEG:
		return _dialogueData + offset;
	case DATA_SEG:
		assert(offset < (1 << 16));
		return _dataSeg + offset;
	case 222:
		assert(offset >= 0x3FC6);
		return _fontData + offset - 0x3FC6;
	case 182:
		assert(offset >= 0x6753);
		return _codeSeg182 + offset - 0x6753;
	case 180:
		assert(offset >= 0x6FA8);
		return _codeSeg180 + offset - 0x6FA8;
	case 176:
		assert(offset >= 0x661D);
		return _codeSeg176 + offset - 0x661D;
	case 175:
		assert(offset >= 0x672E);
		return _codeSeg175 + offset - 0x672E;
	case 145:
		assert(offset >= 0xB1E);
		return _codeSeg145 + offset - 0xB1E;
	case 118:
		if (offset >= 0x378C) {
			return _codeSeg118 + offset - 0x378C;
		}
		if (offset >= 0x359F) { // duplicated strings
			return _codeSeg118 + offset - 0x359F;
		}
		break;
	case 37:
		assert(offset >= 0xAAC); // duplicated font
		return _fontData + offset - 0xAAC;
	case 24:
		assert(offset >= 0x1404);
		return _codeSeg024 + offset - 0x1404;
	case 14:
		assert(offset >= 0x7BD);
		return _codeSeg014 + offset - 0x7BD;
	case 1:
		assert(offset >= 0x2A14);
		return _codeSeg001 + offset - 0x2A14;
	default:
		if (seg >= 256) {
			return (uint8_t *)_ptrs[seg - 256] + offset;
		}
	}
	snprintf(g_err, sizeof__g_err, "Unimplemented Memory::getPtr %d 0x%X", seg, offset);
	exit(1);
	return 0;
}

uint16_t Memory::readUint16(int seg, uint16_t offset) {
	const uint8_t *p = (const uint8_t *)getPtr(seg, offset);
if (_debug) fprintf(stdout, "Memory::readUint16 %X\n", READ_LE_UINT16(p) );
	return READ_LE_UINT16(p);
}

void Memory::writeUint16(int seg, uint16_t offset, uint16_t value) {
	uint8_t *p = (uint8_t *)getPtr(seg, offset);
if (_debug) fprintf(stdout, "Memory::writeUint16 %X\n", value);
	WRITE_LE_UINT16(p, value);
}

uint8_t Memory::readByte(int seg, uint16_t offset) {
	const uint8_t *p = (const uint8_t *)getPtr(seg, offset);
if (_debug) fprintf(stdout, "Memory::readByte %X\n", *p);
	return *p;
}

void Memory::writeByte(int seg, uint16_t offset, uint8_t value) {
	uint8_t *p = (uint8_t *)getPtr(seg, offset);
if (_debug) fprintf(stdout, "Memory::writeByte %X\n", value);
	*p = value;
}
