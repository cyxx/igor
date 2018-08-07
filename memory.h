/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MEMORY_H__
#define MEMORY_H__

#include <stdint.h>
#include "util.h"

static const int kNativePtrsCount = 13;

enum {
	kPtrScreenLayer1 = 0,
	kPtrScreenLayer2 = 1,
	kPtrScreenLayer3 = 2,
	kPtrAnimBuffer   = 4,
	kPtrIgorBuffer   = 5,
	kPtrVerbsPanel   = 12
};

#define DATA_SEG       231
#define STACK_SEG     1000
#define ACTION_SEG    1001
#define INVENTORY_SEG 1002
#define DIALOGUE_SEG  1003

struct Memory {

	void *_ptrs[kNativePtrsCount];
	void *_vga;
	uint16_t _vgaOffset;
	int _actionSeg;
	uint8_t *_actionData;
	int _dialogueSeg;
	uint8_t *_dialogueData;
	int _paletteSeg;
	uint8_t *_fontData;
	uint8_t *_inventoryData;
	uint8_t _dataSeg[1 << 16];
	uint8_t _stack[16384];
	uint8_t _codeSeg001[6522];
	uint8_t _codeSeg014[28086]; // sub_014_00A7
	uint8_t _codeSeg024[24062]; // sub_024_06E2
	uint8_t _codeSeg118[22];
	uint8_t _codeSeg145[9306];
	uint8_t _codeSeg175[120];
	uint8_t _codeSeg176[120];
	uint8_t _codeSeg180[2680];
	uint8_t _codeSeg182[2680];

	void setupPtrs();
	void *getPtr(int seg, uint16_t offset);

	int getPart() const { return READ_LE_UINT16(_dataSeg + 0x321A); }
	void setPart(int num) { WRITE_LE_UINT16(_dataSeg + 0x321A, num); }
	int getSoundHandle() const { return READ_LE_UINT16(_dataSeg + 0x5E94); }
	void setSoundHandle(int value) { WRITE_LE_UINT16(_dataSeg + 0x5E94, value); }

	uint16_t readUint16(int seg, uint16_t offset);
	void writeUint16(int seg, uint16_t offset, uint16_t value);
	uint8_t readByte(int seg, uint16_t offset);
	void writeByte(int seg, uint16_t offset, uint8_t value);
};

#endif
