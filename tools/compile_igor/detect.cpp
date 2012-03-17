
#include <stdio.h>
#include <string.h>
#include "detect.h"

Ptr16 _data[res_count];

PtrOffset _anim[32];
int _animSize;

uint32_t _partCalls[128];
int _partCallsCount;

uint32_t _actions[128];
int _actionsCount;

PtrOffset _roomDat;

static bool get_Imm(Instruction *insn, int pos, int &value) {
	if (insn[0].code[pos] != t_imm) return false;
	value = (uint16_t)(insn[0].code[pos + 1] | (insn[0].code[pos + 2] << 8));
	return true;
}

static int pat_Opcode(Instruction *insn, int op) {
	if ((insn[0].code[0] & 0x7F) != op) return 0;
	return 1;
}

static int pat_Imm(Instruction *insn, int pos, int value) {
	if (insn[0].code[pos]     != (value & 255)) return 0;
	if (insn[0].code[pos + 1] != (value >>  8)) return 0;
	return 1;
}

static int pat_Reg(Instruction *insn, int pos, int num, char s) {
	if (insn[0].code[pos] != t_reg) return false;
	if (insn[0].code[pos + 1] != num) return false;
	if (insn[0].code[pos + 2] != s) return false;
	return true;
}

static int pat_StackCheck(Instruction *insn) {
	if (pat_Opcode(&insn[0], op_mov)  == 0) return 0; // mov     ax, 0Eh
	if (pat_Opcode(&insn[1], op_call) == 0) return 0; // call    @__StackCheck$q4Word
	if (pat_Imm(&insn[1], 1,   230) == 0) return 0;
	if (pat_Imm(&insn[1], 3, 0x5CD) == 0) return 0;
	int size;
	if (!get_Imm(&insn[0], 1, size)) return 0;
	if (size != 0) {
		if (pat_Opcode(&insn[2], op_sub) == 0) return 0; // sub     sp, 0Eh
		return 3;
	}
	return 2;
}

static int pat_memcpy(Instruction *insn) {
	if (pat_Opcode(insn, op_call) == 0) return 0; // call    @Move$qm3Anyt14Word
	if (pat_Imm(insn, 1,    230) == 0) return 0;
	if (pat_Imm(insn, 3, 0x1686) == 0) return 0;
	return 1;
}

static int pat_createCodeSegment(Instruction *insn) {
	if (pat_Opcode(insn, op_call) == 0) return 0; // call    @Move$qm3Anyt14Word
	if (pat_Imm(insn, 1,      1) == 0) return 0;
	if (pat_Imm(insn, 3, 0x3E48) == 0) return 0;
	return 1;
}

static bool get_Offset(Instruction *insn, int &offset) {
	if (pat_Opcode(&insn[0], op_add) != 0) { // add     di, 7480h
		get_Imm(insn, 1, offset);
		return true;
	} else if (pat_Opcode(&insn[0], op_les) != 0) { // les di, dword ptr __animFramesBuffer.ptr
		offset = 0;
		return true;
	}
	return false;
}

static int pat_AnimBufferPtr(Instruction *insn) {
	for (int i = 0; i < 2; ++i) {
		if (pat_Opcode(&insn[-i], op_les) != 0) {
			static const uint8_t pat[] = { t_seg | t_imm, 3, 0x62, 0xD1 };
			if (memcmp(&insn[-i].code[1], pat, sizeof(pat)) == 0) {
				return 1;
			}
		}
	}
	return 0;
}

static Instruction *get_Ptr16(Instruction *insn, Ptr16 &ptr16) {
	int pos;

	if ((pos = pat_Opcode(insn, op_mov)) == 0 || !pat_Reg(insn, 4, 7, 'w')) return 0; // mov     di, offset IMG_SpringBridgeIntro
	if (!get_Imm(insn, 1, ptr16.ptr)) return 0;
	insn += pos;
	if ((pos = pat_Opcode(insn, op_mov)) == 0 || !pat_Reg(insn, 4, 0, 'w')) return 0; // mov     ax, seg cseg211
	get_Imm(insn, 1, ptr16.seg);
	insn += pos;

	return insn;
}

static Instruction *get_Ptr16_size(Instruction *insn, Ptr16 &ptr16) {
	static const int kMaxInsn = 64;
	int i, pos;

	if ((insn = get_Ptr16(insn, ptr16)) == 0) return 0;
	for (i = 0; i < kMaxInsn && !pat_memcpy(&insn[i]) && pat_Opcode(&insn[i], op_ret) == 0; ++i);
	if (i == kMaxInsn || pat_Opcode(&insn[i], op_ret) != 0) return 0;
	if ((pos = pat_Opcode(&insn[i - 1], op_push)) == 0) return 0; // push    0B400h
	get_Imm(&insn[i - 1], 1, ptr16.size);
	insn += i + 1;

	return insn;
}

static Instruction *skip_igorPal(Instruction *insn, uint32_t addr, Ptr16 *ptr) {
	static const uint8_t pat[] = { op_mov | 0x80, t_imm, 0x5E, 0xEA, t_reg, 7, 'w' };
	if (memcmp(insn->code, pat, sizeof(pat)) == 0) {
		if (pat_memcpy(&insn[7])) {
			ptr->size |= 0x8000;
			insn += 8;
		}
	}
	// part_01
	static const uint8_t pat1[] = { op_mov, t_seg | t_imm, 3, 0x1A, 0x32, t_reg, 0, 'l' };
	if (memcmp(insn->code, pat1, sizeof(pat1)) == 0) {
		static const uint8_t ops[] = { op_cmp, op_jmp2, op_cmp, op_jmp2 };
		for (int i = 0; i < 4; ++i) {
			if ((insn[1 + i].code[0] & 0x7F) != ops[i]) {
				return insn;
			}
		}
		insn += 5;
	}
	return insn;
}

bool detectCode_LoadRoomData(Instruction *insn, uint32_t addr) {
	memset(_data, 0, sizeof(_data));
	int i, pos = 0;

if (addr == ( (13 << 16) | 0x0002)) { // img + txt
	_data[res_img].seg = 13; _data[res_img].ptr = 0x644; _data[res_img].size = 0xB400;
	_data[res_txt].seg = 13; _data[res_txt].ptr = 0x265; _data[res_txt].size = 0x8000 | 991;
	goto out;
}
if (addr == ( (17 << 16) | 0x0002)) { // pal,img + txt
	_data[res_pal].seg = 17; _data[res_pal].ptr = 0xB87E; _data[res_pal].size =  0x300;
	_data[res_img].seg = 17; _data[res_img].ptr =  0x47E; _data[res_img].size = 0xB400;
	_data[res_txt].seg = 17; _data[res_txt].ptr =  0x293; _data[res_txt].size = 0x8000 | 491; // no objects name
	goto out;
}
if (addr == ( (40 << 16) | 0x0002)) { // pal,img + txt
	_data[res_pal].seg = 40; _data[res_pal].ptr = 0xBD8A; _data[res_pal].size =  0x270;
	_data[res_img].seg = 40; _data[res_img].ptr =  0x98A; _data[res_img].size = 0xB400;
	_data[res_txt].seg = 40; _data[res_txt].ptr =  0x4BE; _data[res_txt].size =   1228;
	goto out;
}
if (addr == ((162 << 16) | 0x0002)) { // pal,img + msk
	_data[res_pal].seg = 162; _data[res_pal].ptr = 0x8B87; _data[res_pal].size =  0x270;
	_data[res_img].seg = 162; _data[res_img].ptr =  0x64E; _data[res_img].size = 0x851F; // offset= 1
//	_data[res_img2].seg = 162; _data[res_img2].ptr = 0x8B6D; _data[res_img2].size = 26; // offset = 0x8520
	_data[res_txt].seg = 162; _data[res_txt].ptr = 0x2C9; _data[res_txt].size = 0x8000 | 901; // no objects name
	goto out;
}
if (addr == ((163 << 16) | 0x0002)) { // pal,img + msk
	_data[res_pal].seg = 163; _data[res_pal].ptr = 0xB552; _data[res_pal].size =  0x270;
	_data[res_img].seg = 163; _data[res_img].ptr =  0x152; _data[res_img].size = 0xB400;
	_data[res_msk].seg = 163; _data[res_msk].ptr = 0xB7C2; _data[res_msk].size =   1401;
	goto out;
}
if (addr == ((166 << 16) | 0x0002)) { // pal,img + msk
	_data[res_pal].seg = 166; _data[res_pal].ptr = 0xB552; _data[res_pal].size =  0x2D0;
	_data[res_img].seg = 166; _data[res_img].ptr =  0x152; _data[res_img].size = 0xB400;
	_data[res_msk].seg = 166; _data[res_msk].ptr = 0xB822; _data[res_msk].size =   2445;
	goto out;
}

	if ((pos = pat_Opcode(insn, op_push)) == 0) return false; // push    bp
	insn += pos;
	if ((pos = pat_Opcode(insn, op_mov))  == 0) return false; // mov     bp, sp
	insn += pos;
	if ((pos = pat_StackCheck(insn))      == 0) return false;
	insn += pos;

	if ((insn = get_Ptr16_size(insn, _data[res_pal])) == 0) return false;
	insn = skip_igorPal(insn, addr, &_data[res_pal]);
	if ((insn = get_Ptr16_size(insn, _data[res_img])) == 0) return false;
	if (_data[res_img].size == 320 * 200) {
		// pal and img only
		if ((pos = pat_Opcode(&insn[1], op_ret)) == 0) return false; // leave + retf
	} else {
		if (_data[res_img].size != 320 * 144) {
			_data[res_pal2] = _data[res_img];
			if ((insn = get_Ptr16_size(insn, _data[res_img])) == 0) return false;
		}
		if ((insn = get_Ptr16_size(insn, _data[res_box])) == 0) return false;
		if (_data[res_box].size == 320) {
			// no box and msk
			_data[res_txt] = _data[res_box];
			memset(&_data[res_box], 0, sizeof(Ptr16));
		} else {
			// msk
			for (i = 0; i < 3; ++i) {
				if ((pos = pat_Opcode(insn, op_mov)) == 0) return false; // xor     ax, ax
				insn += pos;
				if ((pos = pat_Opcode(insn, op_mov)) == 0) return false; // mov     [bp+var_2], ax
				insn += pos;
			}
			get_Ptr16(insn, _data[res_msk]);
			// fixup msk size
			if (_data[res_box].ptr > _data[res_msk].ptr) {
				_data[res_msk].size = _data[res_box].ptr - _data[res_msk].ptr;
			}
			// txt
			static const int kMaxInsn = 256;
			for (i = 0; i < kMaxInsn && !pat_memcpy(&insn[i]); ++i);
			if (i == kMaxInsn) return false;
			if ((pos = pat_Opcode(&insn[i - 1], op_push)) == 0) return false; // push  320
			get_Imm(&insn[i - 1], 1, _data[res_txt].size);
			if (_data[res_txt].size == 320) {
				insn += i + 1;
				get_Ptr16(insn, _data[res_txt]);
			}
		}
		// fixup txt size
		if (_data[res_txt].ptr != 0 && _data[res_img].ptr > _data[res_txt].ptr) {
			_data[res_txt].size = _data[res_img].ptr - _data[res_txt].ptr;
		}
		if ((_data[res_box].size % 5) != 0) {
			fprintf(stderr, "box size not aligned to 5 bytes\n");
		}
	}
out:
#if 1
	fprintf(stdout, "LoadRoomData (cseg%03d:%04X)\n", addr >> 16, addr & 0xFFFF);
	for (int i = 0; i < res_count; ++i) {
		fprintf(stdout, "{ %3d, 0x%04X, %5d },\n", _data[i].seg, _data[i].ptr, _data[i].size);
	}
#endif
	return true;
}

bool detectCode_LoadAnimData(Instruction *insn, uint32_t addr) {
	memset(_anim, 0, sizeof(_anim));
	_animSize = 0;

	int i, pos = 0;
	if ((pos = pat_Opcode(insn, op_push)) == 0) return false; // push    bp
	insn += pos;
	if ((pos = pat_Opcode(insn, op_mov))  == 0) return false; // mov     bp, sp
	insn += pos;
	if ((pos = pat_StackCheck(insn))      == 0) return false;
	insn += pos;

	for (i = 0; i < 16 && insn; ++i) {
		insn = get_Ptr16_size(insn, _anim[i].p);
		if (insn && (!get_Offset(&insn[-5], _anim[i].offset) || !pat_AnimBufferPtr(&insn[-5]))) {
			return false;
		}
		if (insn && (pat_Opcode(&insn[1], op_ret)) != 0) { // leave + retf
			break;
		}
	}
	if (!insn) return false;
	int count = i + 1;
#if 1
	fprintf(stdout, "LoadAnimData (cseg%03d:%04X)\n", addr >> 16, addr & 0xFFFF);
	for (int i = 0; i < count; ++i) {
		fprintf(stdout, "{ %3d, 0x%04X, %5d, 0x%04X },\n", _anim[i].p.seg, _anim[i].p.ptr, _anim[i].p.size, _anim[i].offset);
	}
#endif
	_animSize = count;
	return true;
}

bool detectCode_PartCall(Instruction *insn, int count) {
	memset(_partCalls, 0, sizeof(_partCalls));
	_partCallsCount = 0;

	for (int i = 0; i < count; ++i) {
		if (pat_Opcode(&insn[i], op_cmp) != 0) {
			continue;
		}
		if (pat_Opcode(&insn[i], op_call) != 0) {
			int pos = 1;
			uint16_t seg = insn[i].code[pos] | (insn[i].code[pos + 1] << 8);
			pos += 2;
			uint16_t ptr = insn[i].code[pos] | (insn[i].code[pos + 1] << 8);
			pos += 2;
			uint32_t addr = (seg << 16) | ptr;
			int j = 0;
			for (j = 0; j < _partCallsCount; ++j) {
				if (_partCalls[j] == addr) {
					break;
				}
			}
			if (j == _partCallsCount) {
				_partCalls[_partCallsCount] = (seg << 16) | ptr;
				++_partCallsCount;
			}
			continue;
		}
	}
	return true;
}

bool detectCode_Action(Instruction *insn, int count) {
	memset(_actions, 0, sizeof(_actions));
	_actionsCount = 0;

	int sz, pos = 0;
	while (pos < count) {
		_actions[_actionsCount] = pos;
		if ((sz = pat_Opcode(insn + pos, op_push)) == 0) return false; // push    bp
		pos += sz;
		if ((sz = pat_Opcode(insn + pos, op_mov)) == 0) return false; // mov     bp, sp
		pos += sz;
		if ((sz = pat_StackCheck(insn + pos)) == 0) return false;
		pos += sz;
		while (pat_Opcode(insn + pos, op_ret) == 0) {
			++pos;
			if (pos > count) {
				return false;
			}
		}
		++pos;
		++_actionsCount;
	}
	return true;
}
