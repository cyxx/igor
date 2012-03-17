
#ifndef DECODE_H__
#define DECODE_H__

#include "opcodes.h"

struct Ptr16 {
	int seg, ptr;
	int size;
};

struct PtrOffset {
	Ptr16 p;
	int offset;
};

enum {
	res_pal = 0,
	res_pal2,
	res_img,
	res_box,
	res_msk,
	res_txt,

	res_count
};

extern Ptr16 _data[res_count];
extern PtrOffset _anim[32];
extern int _animSize;
extern uint32_t _partCalls[128];
extern int _partCallsCount;
extern uint32_t _actions[128];
extern int _actionsCount;
extern PtrOffset _roomDat;

bool detectCode_LoadRoomData(Instruction *insn, uint32_t addr);
bool detectCode_LoadAnimData(Instruction *insn, uint32_t addr);
bool detectCode_PartCall(Instruction *insn, int count);
bool detectCode_Action(Instruction *insn, int count);

#endif
