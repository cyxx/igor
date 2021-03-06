/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "game.h"
#include "real.h"

static const int kSaveVersion = 0;
static const int kTextVersion = 1;

static const bool _debug = false;
static const bool _sortCodeOffsets = true;

Game::Game(const char *dataPath, const char *savePath)
	: _dataPath(dataPath), _savePath(savePath), _mix(dataPath) {
	_quit = false;
	memset(_buffers, 0, sizeof(_buffers));
	_trapsCount = 0;
	memset(_partOffsets, 0, sizeof(_partOffsets));
	_partOffsetsCount = 0;
	memset(_mainOffsets, 0, sizeof(_mainOffsets));
	_mainOffsetsCount = 0;
	_randSeed = time(0);
	_codePos = 0;
	_codeSize = 0;
	_mainPos = 0;
	_partPos = 0;
	_partNum = 0;
	memset(_palBuf, 0, sizeof(_palBuf));
	_palNum = -1;
	_palDirty = true;
	_input = 0;
	_yield = -1;
	_yMinCursor = 0;
	_yMaxCursor = 199;
	_cursorVisible = false;
	_pollSoundPlaying = false;
	_useIgorTxt = false;
}

Game::~Game() {
}

void Game::init(int num) {
	_mem.setupPtrs();
	_script._mem = &_mem;
	registerTraps();
	loadInit();
	if (_sortCodeOffsets) {
		sortCodeOffsets(_mainOffsets, _mainOffsetsCount);
	}
	readData(_mem._fontData,   222, 0x3FC6, 9306);
	readData(_mem._codeSeg001,   1, 0x11C2, sizeof(_mem._codeSeg001));
	readData(_mem._codeSeg014,  14,  0x7BD, sizeof(_mem._codeSeg014));
	readData(_mem._codeSeg024,  24, 0x1404, sizeof(_mem._codeSeg024));
	readData(_mem._codeSeg118, 118, 0x378C, sizeof(_mem._codeSeg118));
	readData(_mem._codeSeg145, 145, 0x0B1E, sizeof(_mem._codeSeg145));
	readData(_mem._codeSeg175, 175, 0x672E, sizeof(_mem._codeSeg175));
	readData(_mem._codeSeg176, 176, 0x661D, sizeof(_mem._codeSeg176));
	readData(_mem._codeSeg180, 180, 0x6FA8, sizeof(_mem._codeSeg180));
	readData(_mem._codeSeg182, 182, 0x6753, sizeof(_mem._codeSeg182));
	loadInventoryFrames();
	loadIgorFrames();
	loadVerbs();
	loadTexts();
	clearPalette();
	runFuncCode(222, 0x3C2B); // set_main_actions
	fixUpData();
	_mem.setPart(num);
	trap_setPalette_208_32(0, 0);
	_codePos = _mainPos;
}

void Game::useTxt(const char *language) {
	_useIgorTxt = _txt.load(_dataPath, language);
}

void Game::fixUpData() {
	WRITE_LE_UINT16(_mem._dataSeg + 0x176E, 0xA000);
	WRITE_LE_UINT32(_mem._dataSeg + 0x5E90, 0x5E94 | (_script._segs[seg_ds] << 16));
	_mem._dataSeg[0xED2B] = 3; // _talkSpeed
	_mem._dataSeg[0xED2C] = kTalkModeSpeechAndText; // _talkMode
	_mem._dataSeg[0x5B2C] = 5; // _talkSpeechCounter
	_mem._dataSeg[0xED29] = 1; // _musicOn
	_mem._dataSeg[0xED2A] = 1; // _sfxOn
	_mem._dataSeg[0xFD0C] = 2; // _soundCardCapabilities (2 == sfx+music)
	WRITE_LE_UINT16(_mem._dataSeg + 0xFD16, INVENTORY_SEG); // _inventoryDataSeg
	WRITE_LE_UINT16(_mem._dataSeg + 0xFD18, ACTION_SEG); // _roomActionSeg
//	_mem._dataSeg[0xED40] = 1; // force inventory redraw
}

void Game::loadState(int num) {
	char name[32];
	snprintf(name, sizeof(name), "igor.s%02d", num);
	StateFile f;
	if (f.open(name, _savePath, "rb")) {
		info("Loading state %d", num);
		const int version = f.readUint32LE();
		if (version == kSaveVersion) {
			f.read(_mem._dataSeg, sizeof(_mem._dataSeg));
			f.read(_mem._vga, 64000);
			trap_setPalette_208_32(0, 0);
			_codePos = _mainPos;
		}
	}
}

void Game::saveState(int num) {
	char name[32];
	snprintf(name, sizeof(name), "igor.s%02d", num);
	StateFile f;
	if (f.open(name, _savePath, "wb")) {
		info("Saving state %d", num);
		f.writeUint32LE(kSaveVersion);
		f.write(_mem._dataSeg, sizeof(_mem._dataSeg));
		f.write(_mem._vga, 64000);
	}
}

static const uint32_t _crc[] = {
	0x1B3D6A56, // spa_cd
	0
};

static void checkCRC(AssetFile &f) {
	uint8_t buf[1024];
	f.read(buf, sizeof(buf));
	uint32_t crc = crc32(0, Z_NULL, 0);
	crc = crc32(crc, buf, sizeof(buf));
	for (int i = 0; _crc[i] != 0; ++i) {
		if (crc == _crc[i]) {
			return;
		}
	}
	error("Unexpected CRC32 (%08x) for 'IGOR.EXE'", crc);
}

void Game::loadInit() {
	if (!_exe._f.open("IGOR.EXE", _dataPath)) {
		error("Unable to open '%s'", "IGOR.EXE");
		return;
	}
	checkCRC(_exe._f);
	_exe.parseSegmentsInfo();
	_exe.readSegment(DATA_SEG, _mem._dataSeg);
	_script._segs[seg_es] = _script._segs[seg_ds] = DATA_SEG;
	AssetFile f;
		if (!f.open("igor.bin", _dataPath)) {
			error("Unable to open '%s'", "igor.bin");
			return;
		}
		const int binOffset = 4;
		f.seek(binOffset);
		const uint32_t offset = f.readUint32LE();
		f.seek(offset);
	_mainPos = f.readUint32LE();
	const int funcsCount = f.readUint16LE();
	debug(DBG_GAME, "main funcs count %d", funcsCount);
	for (int i = 0; i < funcsCount; ++i) {
		const int seg = f.readUint16LE();
		const int ptr = f.readUint16LE();
		const int offset = f.readUint32LE();
		if (!findCodeOffset((seg << 16) | ptr, _mainOffsets, _mainOffsetsCount)) {
			assert(_mainOffsetsCount < MAX_MAIN_OFFSETS);
			_mainOffsets[_mainOffsetsCount++].set(seg, ptr, offset, kCode_code);
		}
	}
	const int size = f.readUint32LE();
	debug(DBG_GAME, "main code size %d", size);
	_buffers[kCode_code] = (uint8_t *)malloc(size);
	f.read(_buffers[kCode_code], size);
	const int partsCount = f.readByte();
	debug(DBG_GAME, "parts count %d", partsCount);
	for (int i = 0; i < partsCount; ++i) {
		const int seg = f.readUint16LE();
		const int ptr = f.readUint16LE();
		const int num = f.readByte();
		if (!findCodeOffset((seg << 16) | ptr, _mainOffsets, _mainOffsetsCount)) {
			assert(_mainOffsetsCount < MAX_MAIN_OFFSETS);
			_mainOffsets[_mainOffsetsCount++].set(seg, ptr, num, kCode_main);
		}
	}
	_codeSize = size;
}

void Game::loadInventoryFrames() {
	readData(_mem._inventoryData, 228, 0x698, 58146);
	uint8_t *p = (uint8_t *)_mem.getPtr(INVENTORY_SEG, 0xE7F8);
	for (int i = 0; i < 30; ++i) {
		memcpy(p, _mem._inventoryData + i * 320, 15);
		p += 15;
	}
}

void Game::loadIgorFrames() {
	static const uint16_t t[] = {
		0x01C3, 10500,
		0x2AC7, 13500,
		0x5F83, 10500,
		0x8887, 13500,
		0xBD43,  3696,
		0, 0
	};
	int j = kPtrIgorBuffer;
	for (int i = 0; t[i]; i += 2) {
		readData(_mem._ptrs[j], 218, t[i], t[i + 1]);
		++j;
	}
	readData(_mem.getPtr(DATA_SEG, 0xEA5E), 231, 0x13B6, 48);
}

void Game::loadVerbs() {
	static const struct {
		uint16_t x;
		const char *nameSp;
		const char *nameEn;
	} verbs[] = {
		{  21, "Hablar", "Talk"  },
		{  67, "Coger",  "Take"  },
		{ 113, "Mirar",  "Look"  },
		{ 159, "Usar",   "Use"   },
		{ 205, "Abrir",  "Open"  },
		{ 251, "Cerrar", "Close" },
		{ 297, "Dar",    "Give"  },
		{ 0, 0, 0 }
	};
	uint8_t *p = (uint8_t *)_mem._vga + 156 * 320;
	readData(p, 219, 0x1E0, 3840);
	for (int i = 0; verbs[i].x != 0; ++i) {
		_script.push(verbs[i].x);
		_script.push(156);
		_script.push(0xFFF2);
		const char *name = verbs[i].nameSp;
		if (_useIgorTxt) {
			name = verbs[i].nameEn;
		}
		const int count = strlen(name);
		_mem._dataSeg[16] = count;
		memcpy(_mem._dataSeg + 17, name, count);
		_script.push(_script._segs[3]);
		_script.push(16);
		_script.push(0xA000);
		_script.push(4800);
		_script.push(1);
		runFuncCode(222, 0x123E); // draw_string
	}
	memcpy(_mem._ptrs[kPtrVerbsPanel], p, 3840);
}

static void dumpPascalString(const uint8_t *str) {
	const int len = *str;
	char buf[257];
	memcpy(buf, str + 1, len);
	buf[len] = 0;
	debug(DBG_GAME, "dumpPascalString (%3d) ", *str, buf);
}

void Game::loadTexts() {
	static const struct {
		int offset;
		int count;
		int len;
	} offsets[] = {
		{      0,   3,  7 * 2 },
		{   0x2A,  35, 31 * 2 },
		{  0x8BA, 250, 51 * 2 },
		{ 0x6CA4,   9, 12 * 2 },
		{     -1,   0,      0 }
	};
	uint8_t *p = (uint8_t *)_mem.getPtr(DATA_SEG, 0x5EE6);
	if (_useIgorTxt) {
		for (int j = 0; offsets[j].offset != -1; ++j) {
			for (int i = 0; i < offsets[j].count; ++i) {
				uint8_t *dst = p + offsets[j].offset + i * offsets[j].len;
				switch (j) {
				case 0:
					_txt.readPreposition(i, dst);
					break;
				case 1:
					_txt.readItem(i, dst);
					break;
				case 2:
					_txt.readResponse(i, dst);
					break;
				case 3:
					_txt.readVerb(i, dst);
					break;
				}
			}
		}
	} else {
		readData(p, 219, 0x10E0, 28028);
		for (int i = 0; i < 28028; ++i) {
			p[i] -= 0x6D;
		}
	}
	if (_debug) {
		for (int j = 0; offsets[j].offset != -1; ++j) {
			const uint8_t *src = p + offsets[j].offset;
			for (int i = 0; i < offsets[j].count; ++i) {
				dumpPascalString(src);
				src += offsets[j].len;
			}
		}
	}
}

void Game::unloadPart() {
	free(_buffers[kCode_room]);
	_buffers[kCode_room] = 0;
	free(_buffers[kCode_anim]);
	_buffers[kCode_anim] = 0;
	_partOffsetsCount = 0;
}

void Game::loadPart(int num) {
	debug(DBG_GAME, "load part %d", num);
	AssetFile f;
	if (!f.open("igor.bin", _dataPath)) {
		error("Unable to open '%s'", "igor.bin");
		return;
	}
	int binOffset = 8 + (num - 1) * 12;
	if (1) {
		f.seek(binOffset);
		binOffset += 4;
		const uint32_t offset = f.readUint32LE();
		f.seek(offset);
	}
	const int count = f.readUint32LE();
	_buffers[kCode_room] = (uint8_t *)malloc(count * 48);
	for (int i = 0; i < count; ++i) {
		const int seg = f.readUint16LE();
		const int ptr = f.readUint16LE();
		assert(_partOffsetsCount < MAX_PART_OFFSETS);
		_partOffsets[_partOffsetsCount++].set(seg, ptr, i * 48, kCode_room);
		f.read(_buffers[kCode_room] + i * 48, 48);
	}
	debug(DBG_GAME, "room size %d", count);
	if (1) {
		f.seek(binOffset);
		binOffset += 4;
		const uint32_t offset = f.readUint32LE();
		f.seek(offset);
	}
	const int animCount = f.readUint32LE();
	const int bufSize = f.readUint32LE();
	_buffers[kCode_anim] = (uint8_t *)malloc(bufSize);
	int offset = 0;
	for (int i = 0; i < animCount; ++i) {
		const int seg = f.readUint16LE();
		const int ptr = f.readUint16LE();
		const int size = f.readUint32LE();
		assert(_partOffsetsCount < MAX_PART_OFFSETS);
		_partOffsets[_partOffsetsCount++].set(seg, ptr, offset, kCode_anim);
		f.read(_buffers[kCode_anim] + offset, size);
		offset += size;
	}
	debug(DBG_GAME, "anim size %d", animCount);
	if (1) {
		f.seek(binOffset);
		binOffset += 4;
		const uint32_t offset = f.readUint32LE();
		f.seek(offset);
	}
	_script._segs[seg_cs] = f.readUint16LE();
	f.readUint16LE();
	_partPos = _codeSize + f.readUint32LE();
	const int funcCount = f.readUint32LE();
	for (int i = 0; i < funcCount; ++i) {
		const int seg = f.readUint16LE();
		const int ptr = f.readUint16LE();
		const int offset = f.readUint32LE();
		assert(_partOffsetsCount < MAX_PART_OFFSETS);
		_partOffsets[_partOffsetsCount++].set(seg, ptr, _codeSize + offset, kCode_code);
	}
	const int size = f.readUint32LE();
	_buffers[kCode_code] = (uint8_t *)realloc(_buffers[kCode_code], _codeSize + size);
	f.read(_buffers[kCode_code] + _codeSize, size);
	debug(DBG_GAME, "code size %d (%d)", funcCount, size);
	if (_sortCodeOffsets) {
		sortCodeOffsets(_partOffsets, _partOffsetsCount);
	}
}

static void decodeRoomString(AssetFile &f, uint8_t *dst, int sz) {
	*dst++ = sz;
	for (int i = 0; i < sz; ++i) {
		uint8_t code = f.readByte();
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
		*dst++ = code;
	}
}

void Game::readData(void *dst, int seg, int ptr, int size) {
	_exe._f.seek(_exe.getSegmentInfo(seg)->offset + ptr);
	_exe._f.read(dst, size);
}

void Game::seekData(int seg, int ptr) {
	_exe._f.seek(_exe.getSegmentInfo(seg)->offset + ptr);
}

void Game::loadRoomData(int num) {
	const int part = _mem.getPart();
	debug(DBG_GAME, "room_data offset=%d part=%d cseg%02d:%04X", num, part, _script._callSegPtr >> 16, _script._callSegPtr & 0xFFFF);
	const uint8_t *ptr = _buffers[kCode_room] + num;
	// pal
	int size = READ_LE_UINT16(ptr + 4);
	if (size != 0) {
		seekData(READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2));
		uint8_t *pal = (uint8_t *)_mem.getPtr(DATA_SEG, 0xE15E);
		if ((size & 0x8000) != 0) {
			_exe._f.read(pal, 0x240);
			memcpy(pal + 0x240, _mem.getPtr(DATA_SEG, 0xEA5E), 0x30);
		} else {
			_exe._f.read(pal, size);
		}
	}
	ptr += 8;
	// pal2
	size = READ_LE_UINT16(ptr + 4);
	if (size != 0) {
		if (part != 12 && part != 13) {
			seekData(READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2));
			_exe._f.read(_mem.getPtr(DATA_SEG, 0xE15E + 768 - size), size);
		}
	}
	ptr += 8;
	// img
	size = READ_LE_UINT16(ptr + 4);
	if (size != 0) {
		seekData(READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2));
		if (_mem.getPart() / 100 == 9) {
			_exe._f.read(_mem._vga, size);
		} else if (_script._callSegPtr == ((40 << 16) | 0x0002)) {
			_exe._f.read(_mem._vga, size);
		} else {
			uint8_t *img = (uint8_t *)_mem._ptrs[kPtrScreenLayer1];
			if (_script._callSegPtr == ((162 << 16) | 0x0002)) {
				_exe._f.read(img + 1, size);
				readData(img + 0x8520, 162, 0x8B6D, 26);
			} else {
				_exe._f.read(img, size);
			}
		}
	}
	ptr += 8;
	// box
	size = READ_LE_UINT16(ptr + 4);
	if (size != 0) {
		assert((size % 5) == 0);
		seekData(READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2));
		uint8_t *p = (uint8_t *)_mem.getPtr(DATA_SEG, 0xDC56);
		_exe._f.read(p, size);
	}
	ptr += 8;
	// msk
	size = READ_LE_UINT16(ptr + 4);
	if (size != 0) {
		seekData(READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2));
		uint8_t *p = (uint8_t *)_mem._ptrs[kPtrScreenLayer2];
		int i = 0;
		while (i < 320 * 144) {
			const uint8_t code = _exe._f.readByte();
			const int len = _exe._f.readUint16LE();
			assert(i + len <= 320 * 144);
			memset(p + i, code, len);
			i += len;
		}
	}
	ptr += 8;
	// txt
	size = READ_LE_UINT16(ptr + 4);
	if (size != 0) {
		int txtseg = READ_LE_UINT16(ptr);
		int txtptr = READ_LE_UINT16(ptr + 2);
		seekData(txtseg, txtptr);
		const bool skipObjectNames = (size & 0x8000) != 0;
		size &= ~0x8000;
		if (skipObjectNames) {
			warning("Skipping room object names");
		} else {
			_exe._f.read(_mem.getPtr(DATA_SEG, 0xD966), 320);
			txtptr += 320;
			_exe._f.read(_mem.getPtr(DATA_SEG, 0xDAA6), 432);
			txtptr += 432;
			uint8_t *_roomObjectNames = (uint8_t *)_mem.getPtr(DATA_SEG, 0xCC62);
			for (int i = 0; i < 20; ++i) {
				_roomObjectNames[i * 62] = 0;
			}
			uint8_t code = _exe._f.readByte();
			int index = -1;
			while (code != 0xF6) {
				if (code == 0xF4) {
					++index;
				} // else if (code == 0xF5) { // english
				const int len = _exe._f.readByte();
				if (len != 0) {
					assert(index >= 0);
					decodeRoomString(_exe._f, _roomObjectNames + index * 62, len);
					if (g_debugMask & DBG_GAME) {
						debug(DBG_GAME, "object %03d (%04X): ", index, 0xCC62 + index * 62);
						dumpPascalString(_roomObjectNames + index * 62);
					}
				}
				code = _exe._f.readByte();
			}
			if (_useIgorTxt) {
				for (int i = 0; i < 20; ++i) {
					_txt.readRoomItem(_script._callSegPtr >> 16, i, _roomObjectNames + i * 62);
				}
			}
		}
		uint8_t *_globalDialogueTexts = (uint8_t *)_mem.getPtr(DATA_SEG, 0x67A0);
		for (int i = 200; i < 250; ++i) {
			_globalDialogueTexts[i * 102] = 0;
		}
		uint8_t code = _exe._f.readByte();
		int index = 200;
		while (code != 0xF6) {
			if (code == 0xF4) {
				++index;
			} // else if (code == 0xF5) { // english
			const int len = _exe._f.readByte();
			if (len != 0) {
				assert(index >= 0);
				decodeRoomString(_exe._f, _globalDialogueTexts + index * 102, len);
				if (g_debugMask & DBG_GAME) {
					debug(DBG_GAME, "dialogue %03d (%04X): ", index, 0x67A0 + index * 102);
					dumpPascalString(_globalDialogueTexts + index * 102);
				}
			}
			code = _exe._f.readByte();
		}
		if (_useIgorTxt) {
			for (int i = 200; i < 250; ++i) {
				_txt.readRoomDialogue(_script._callSegPtr >> 16, i, _globalDialogueTexts + i * 102);
			}
		}
	}
}

void Game::loadAnimData(int num) {
	debug(DBG_GAME, "anim_data offset=%d", num);
	const uint8_t *ptr = _buffers[kCode_anim] + num;

	const int count = READ_LE_UINT16(ptr); ptr += 2;
	for (int i = 0; i < count; ++i) {
		const int size = READ_LE_UINT16(ptr + 4);
	        if (size != 0) {
			const int offset = READ_LE_UINT16(ptr + 6);
			uint8_t *p = (uint8_t *)_mem._ptrs[kPtrAnimBuffer];
			readData(p + offset, READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2), size);
		}
		ptr += 8;
	}
}

struct Trap {
	const uint16_t seg, ptr;
	void (Game::*proc)(int argc, int argv[]);
	const char *sig;
	int retsp;
};

static const Trap _traps[] = {
	{ 220, 0x0002, &Game::trap_setPalette_240_16, "", 0 },
	{ 220, 0x1D48, &Game::trap_handleOptionsMenu, "", 0 },
	{ 221, 0x00BD, &Game::trap_playMusic, "ii", 2 },
	{ 221, 0x0597, &Game::trap_getMusicState, "i", 1 },
	{ 221, 0x082F, &Game::trap_playSound, "ii", 2 },
	{ 221, 0x094A, &Game::trap_stopSound, "", 0 },
	{ 221, 0x2315, &Game::trap_setPaletteRange, "ii", 2 },
	{ 221, 0x21DA, &Game::trap_setPalette, "", 0 },
	{ 221, 0x273B, &Game::trap_quitGame, "", 0 },
	{ 221, 0x2813, &Game::trap_setCursorPos, "", 0 },
	{ 222, 0x2264, &Game::trap_showCursor, "", 0 },
	{ 222, 0x229F, &Game::trap_hideCursor, "", 0 },
	{ 228, 0x0002, &Game::trap_setPalette_208_32, "", 0 },
	{ 230, 0x05CD, &Game::trap_checkStack, "", 0 },
	{ 230, 0x0C6C, &Game::trap_copyAny, "sisii", 5 },
	{ 230, 0x0C84, &Game::trap_mulLongInt, "", 0 },
	{ 230, 0x0D99, &Game::trap_loadString, "sisi", 2 },
	{ 230, 0x0DB3, &Game::trap_copyString, "sisii", 5 },
	{ 230, 0x0E18, &Game::trap_concatString, "sisi", 2 },
	{ 230, 0x0FDA, &Game::trap_loadBitSet, "sib", 1 },
	{ 230, 0x1011, &Game::trap_addBitSet, "sibb", 2 },
	{ 230, 0x1065, &Game::trap_getBitSetOffset, "", 0 },
	{ 230, 0x150C, &Game::trap_addReal, "", 0 },
	{ 230, 0x1524, &Game::trap_divReal, "", 0 },
	{ 230, 0x152E, &Game::trap_cmpReal, "", 0 },
	{ 230, 0x1532, &Game::trap_realInt, "", 0 },
	{ 230, 0x1536, &Game::trap_truncReal, "", 0 },
	{ 230, 0x153E, &Game::trap_roundReal, "", 0 },
	{ 230, 0x1558, &Game::trap_getRandomNumber, "i", 1 },
	{ 230, 0x1686, &Game::trap_memcpy, "sisii", 5 },
	{ 230, 0x16AA, &Game::trap_memset, "siii", 4 },
	{ 901, 0x00BB, &Game::trap_fixVgaPtr, "sii", 3 },
	{ 999, 0x0001, &Game::trap_loadActionData, "siii", 4 },
	{ 999, 0x0002, &Game::trap_setPaletteColor, "iiii", 4 },
	{ 999, 0x0003, &Game::trap_loadImageData, "sisii", 5 },
	{ 999, 0x0004, &Game::trap_setActionData, "i", 1 },
	{ 999, 0x0005, &Game::trap_setDialogueData, "i", 1 },
	{ 999, 0x0006, &Game::trap_loadDialogueData, "siii", 4 },
	{ 999, 0x0007, &Game::trap_setPaletteData, "i", 1 },
	{ 999, 0x0009, &Game::trap_waitInput, "", 0 },
	{ 999, 0x000A, &Game::trap_setMouseRange, "ii", 2 },
	{ 999, 0x000B, &Game::trap_waitSound, "", 0 },
	{ 0, 0, 0, 0 }
};

void Game::registerTraps() {
	for (int i = 0; _traps[i].proc; ++i) {
		_mainOffsets[_mainOffsetsCount++].set(_traps[i].seg, _traps[i].ptr, i, kCode_trap);
	}
	_trapsCount = _mainOffsetsCount;
}

void Game::runFuncCode(int seg, int ptr) {
	const uint32_t addr = (seg << 16) | ptr;
	const CodeOffset *offset = getCodeOffset(addr, _mainOffsets, _mainOffsetsCount);
	if (offset && offset->type == kCode_code) {
		int pos = offset->offset;
		_script._callLocals = 4;
		_script.enter(_codePos, addr);
		while (_script._callsCount != 0) {
			pos = _script.executeOpcode(_buffers[kCode_code], pos);
		}
	} else {
		error("No code for cseg%02d:%04X", seg, ptr);
	}
}

void Game::runTrap(int num) {
	const int argc = strlen(_traps[num].sig);
	int argv[8];
	int offset = _script.sp() + (argc - 1) * 2;
	for (int i = 0; i < argc; ++i) {
		argv[i] = READ_LE_UINT16(_mem._stack + offset);
		offset -= 2;
	}
	_script.enter(_codePos, (_traps[num].seg << 16) | _traps[num].ptr);
	(this->*_traps[num].proc)(argc, argv);
	_script.leave(_script._callLocals + _traps[num].retsp * 2);
}

void Game::runTick() {
	while (_script._waitTicks == 0 && !_quit) {
		const int pos = _codePos;
		_codePos = _script.executeOpcode(_buffers[kCode_code], _codePos);
		if (_script._callSegPtr != 0) {
			call();
			_script._callSegPtr = 0;
		}
		if (_script._out.port != 0) {
			out();
			_script._out.port = 0;
		}
		if (_yield >= 0) {
			--_yield;
			if (_yield != -1) {
				_codePos = pos;
			}
		}
		if (_pollSoundPlaying && !_mix.isSoundPlaying()) {
			_mem.setSoundHandle(0);
			_pollSoundPlaying = false;
		}
	}
	--_script._waitTicks;
}

const CodeOffset *Game::findCodeOffset(const uint32_t addr, const CodeOffset *offsets, int offsetsCount) const {
	for (int i = 0; i < offsetsCount; ++i) {
		if (offsets[i].addr() == addr) {
			return &offsets[i];
		}
	}
	return 0;
}

static int compareCodeOffset(const void *p1, const void *p2) {
	const CodeOffset *c1 = (const CodeOffset *)p1;
	const CodeOffset *c2 = (const CodeOffset *)p2;
	return (int)c1->addr() - (int)c2->addr();
}

void Game::sortCodeOffsets(CodeOffset *offsets, int offsetsCount) {
	for (int i = 0; i < offsetsCount; ++i) {
		const uint32_t addr = offsets[i].addr();
		for (int j = i + 1; j < offsetsCount; ++j) {
			if (offsets[j].addr() == addr) {
				warning("offset %d duplicate of %d (cseg%02d:%04X)", j, i, offsets[i].seg, offsets[i].ptr);
			}
		}
	}
	qsort(offsets, offsetsCount, sizeof(CodeOffset), compareCodeOffset);
}

const CodeOffset *Game::getCodeOffset(const uint32_t addr, const CodeOffset *offsets, int offsetsCount) const {
	if (!_sortCodeOffsets) {
		return findCodeOffset(addr, offsets, offsetsCount);
	}
	CodeOffset co;
	co.seg = addr >> 16;
	co.ptr = addr & 0xFFFF;
	return (const CodeOffset *)bsearch(&co, offsets, offsetsCount, sizeof(CodeOffset), compareCodeOffset);
}

struct Func {
	const uint16_t seg, ptr;
	void (Game::*proc)();
};

static const Func _funcs[] = {
	{ 209, 0x0002, &Game::sub_209_0002 },
	{ 224, 0x0123, &Game::sub_224_0123 },
	{ 0, 0, 0 }
};

void Game::call() {
	const CodeOffset *offset = getCodeOffset(_script._callSegPtr, _partOffsets, _partOffsetsCount);
	if (!offset) {
		offset = getCodeOffset(_script._callSegPtr, _mainOffsets, _mainOffsetsCount);
	}
	if (!offset) {
		error("Unimplemented call to cseg%03d:%04X", _script._callSegPtr >> 16, _script._callSegPtr & 0xFFFF);
		return;
	}
	debug(DBG_GAME, "call to cseg%03d:%04X type %d", _script._callSegPtr >> 16, _script._callSegPtr & 0xFFFF, offset->type);
	switch (offset->type) {
	case kCode_main:
		debug(DBG_GAME, "change part %d (cseg%02d:%04X)", _mem.getPart(), _script._callSegPtr >> 16, _script._callSegPtr & 0xFFFF);
		if (_partNum != (int)offset->offset) {
			unloadPart();
			_partNum = offset->offset;
			loadPart(_partNum);
		}
		_script.enter(_codePos, _script._callSegPtr);
		_codePos = _partPos;
		break;
	case kCode_trap:
		runTrap(offset->offset);
		break;
	case kCode_room:
		loadRoomData(offset->offset);
		break;
	case kCode_anim:
		loadAnimData(offset->offset);
		break;
	case kCode_code:
		for (int i = 0; _funcs[i].proc; ++i) {
			const uint32_t addr = (_funcs[i].seg << 16) | _funcs[i].ptr;
			if (_script._callSegPtr == addr) {
				(this->*_funcs[i].proc)();
				return;
			}
		}
		_script.enter(_codePos, _script._callSegPtr);
		_codePos = offset->offset;
		break;
	}
}

void Game::out() {
	switch (_script._out.port) {
	case 0x3C8:
		_palNum = (_script._out.value & 255) * 3;
		break;
	case 0x3C9:
		assert(_palNum >= 0 && _palNum < 256 * 3);
		_palBuf[_palNum] = _script._out.value & 255;
		++_palNum;
		_palDirty = true;
		break;
	}
}

void Game::readString(int seg, int ptr) {
        if (seg >= DATA_SEG) { // data or stack
		const uint8_t *src = (const uint8_t *)_mem.getPtr(seg, ptr);
		const int count = *src++;
		_sBuf[0] = count;
		memcpy(_sBuf + 1, src, count);
        } else {
		seekData(seg, ptr);
		const int count = _exe._f.readByte();
		_sBuf[0] = count;
		_exe._f.read(_sBuf + 1, count);
        }
	if (g_debugMask & DBG_GAME) {
		debug(DBG_GAME, "Game::readString %d:%04X", seg, ptr);
		dumpPascalString(_sBuf);
	}
}

void Game::setMousePos(int x, int y) {
	WRITE_LE_UINT16(_mem._dataSeg + 0xEAAC, x);
	if (y < _yMinCursor) {
		y = _yMinCursor;
	} else if (y > _yMaxCursor) {
		y = _yMaxCursor;
	}
	WRITE_LE_UINT16(_mem._dataSeg + 0xEAAE, y);
}

void Game::setMouseButton(int num, int pressed) {
	if (num != 0) {
		return;
	}
	if (pressed == 1) {
		_mem._dataSeg[0xEAB4] = 1;
		_input = 1;
	} else {
		_input = 0;
	}
}

void Game::setKeyPressed(int code, int pressed) {
	switch (code) {
	case 1:
		_mem._dataSeg[0xEA8E] = pressed;
		break;
	case 25:
		_mem._dataSeg[0xEA90] = pressed;
		break;
	case 28:
		_mem._dataSeg[0xEA91] = pressed;
		break;
	case 29:
		_mem._dataSeg[0xEA9E] = pressed;
		break;
	case 57:
		_mem._dataSeg[0xEA8F] = pressed;
		break;
	}
}

void Game::cheat_exitMaze() {
	debug(DBG_GAME, "Game::cheat_exitMaze part %d", _mem.getPart());
	if (_mem.getPart() != 500) {
		// exit maze
		_mem.setPart(500);
	} else {
		// enter maze
		_mem.setPart(670);
	}
	_codePos = _mainPos;
}
