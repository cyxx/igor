/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef GAME_H__
#define GAME_H__

#include "memory.h"
#include "mixer.h"
#include "script.h"
#include "segment_exe.h"
#include "util.h"

enum {
	kCode_main = -2,
	kCode_trap = -1,
	kCode_room = 0,
	kCode_anim,
	kCode_code,

	kCode_count
};

struct CodeOffset {
	uint16_t seg, ptr;
	uint32_t offset;
	int type;

	void set(uint16_t seg, uint16_t ptr, uint32_t offset, int type) {
		this->seg = seg;
		this->ptr = ptr;
		this->offset = offset;
		this->type = type;
	}
	uint32_t addr() const {
		return (seg << 16) | ptr;
	}
};

struct PatchOffset {
	uint16_t seg, ptr;
	uint32_t offset;
	int size;
};

enum {
	kTalkModeSpeechOnly = 0,
	kTalkModeSpeechAndText = 1,
	kTalkModeTextOnly = 2
};

#define MAX_PART_OFFSETS 256
#define MAX_MAIN_OFFSETS 384
#define MAX_PATCH_OFFSETS 128

struct Game {

	bool _quit;
	SegmentExecutable _exe;
	File _patch;
	const char *_dataPath;
	Memory _mem;
	Mixer _mix;
	Script _script;
	uint8_t *_buffers[kCode_count];
	int _trapsCount;
	CodeOffset _partOffsets[MAX_PART_OFFSETS];
	int _partOffsetsCount;
	CodeOffset _mainOffsets[MAX_MAIN_OFFSETS];
	int _mainOffsetsCount;
	PatchOffset _patchOffsets[MAX_PATCH_OFFSETS];
	int _patchOffsetsCount;
	int _codePos, _codeSize;
	int _mainPos;
	int _partPos;
	int _partNum;
	uint8_t _palBuf[256 * 3];
	int _palNum;
	uint8_t _sBuf[256];
	uint32_t _randSeed;
	int _input;
	int _yield;
	int _yMinCursor, _yMaxCursor;
	bool _cursorVisible;
	bool _pollSoundPlaying;

	Game(const char *dataPath);
	~Game();

	void init(int num);
	void fixUpData();
	void loadState(int num);
	void saveState(int num);
	void loadInit();
	void loadInventoryFrames();
	void loadIgorFrames();
	void loadVerbs();
	void loadTexts();
	void loadData();
	void addCodeOffset(int seg, int ptr, int offset, int type);
	void unloadPart();
	void loadPart(int num);
	void registerTraps();
	void readPatchData(void *dst, int seg, int ptr, int size);
	void readData(void *dst, int seg, int ptr, int size);
	void seekData(int seg, int ptr);
	void loadRoomData(int num);
	void loadAnimData(int num);
	void runFuncCode(int seg, int ptr);
	void runTrap(int num);
	void dumpCall();
	const CodeOffset *findCodeOffset(uint32_t addr, const CodeOffset *offsets, int offsetsCount) const;
	void sortCodeOffsets(CodeOffset *offsets, int offsetsCount);
	const CodeOffset *getCodeOffset(uint32_t addr, const CodeOffset *offsets, int offsetsCount) const;
	void call();
	void out();
	void runTick();
	void readString(int seg, int ptr);
	void setMousePos(int x, int y);
	void setMouseButton(int num, int pressed);
	void setKeyPressed(int code, int pressed);

	void cheat_exitMaze();

	void sub_209_0002();
	void sub_221_273B();
	void sub_224_0002(int seg, int ptr);
	void sub_224_0123();

	void trap_setPalette_240_16(int argc, int *argv);
	void trap_playMusic(int argc, int *argv);
	void trap_handleOptionsMenu(int argc, int *argv);
	void trap_getMusicState(int argc, int *argv);
	void trap_playSound(int argc, int *argv);
	void trap_stopSound(int argc, int *argv);
	void trap_setPaletteRange(int argc, int *argv);
	void trap_setPalette(int argc, int *argv);
	void trap_quitGame(int argc, int *argv);
	void trap_showCursor(int argc, int *argv);
	void trap_hideCursor(int argc, int *argv);
	void trap_setPalette_208_32(int argc, int *argv);
	void trap_checkStack(int argc, int *argv);
	void trap_copyAny(int argc, int *argv);
	void trap_mulLongInt(int argc, int *argv);
	void trap_loadString(int argc, int *argv);
	void trap_copyString(int argc, int *argv);
	void trap_concatString(int argc, int *argv);
	void trap_loadBitSet(int argc, int *argv);
	void trap_addBitSet(int argc, int *argv);
	void trap_getBitSetOffset(int argc, int *argv);
	void trap_addReal(int argc, int *argv);
	void trap_divReal(int argc, int *argv);
	void trap_cmpReal(int argc, int *argv);
	void trap_realInt(int argc, int *argv);
	void trap_truncReal(int argc, int *argv);
	void trap_roundReal(int argc, int *argv);
	void trap_getRandomNumber(int argc, int *argv);
	void trap_memcpy(int argc, int *argv);
	void trap_memset(int argc, int *argv);
	void trap_fixVgaPtr(int argc, int *argv);
	void trap_loadActionData(int argc, int *argv);
	void trap_setPaletteColor(int argc, int *argv);
	void trap_loadImageData(int argc, int *argv);
	void trap_setActionData(int argc, int *argv);
	void trap_setDialogueData(int argc, int *argv);
	void trap_loadDialogueData(int argc, int *argv);
	void trap_setPaletteData(int argc, int *argv);
	void trap_waitInput(int argc, int *argv);
	void trap_setMouseRange(int argc, int *argv);
	void trap_waitSound(int argc, int *argv);
};

//  4 : college map
//  5 : bridge
//  6 : bridge (rock)
// 12 : outside church
// 13 : inside church
// 14 : church puzzle
// 15 : tobias office
// 16 : laboratory
// 17 : outside college
// 18 : men toilets
// 19 : women toilets
// 21 : college corridor margaret
// 22 : bell church
// 23 : college corridor lucas
// 24 : college corridor sharon michael
// 25 : college corridor announcement board
// 26 : college corridor miss barrymore
// 27 : college lockers
// 28 : college corridor caroline
// 30 : college corridor stairs first floor
// 31 : college corridor stairs second floor
// 33 : library
// 35 : park
// 36 : chemistry classroom
// 37 : physics classroom
// 50 : maze exit
// 51..67 : maze
// 75 : philip vodka cutscene
// 76 : plane
// 85 : introduction cutscene
// 90 : startup screens
// 91..97 : ending screens

#endif
