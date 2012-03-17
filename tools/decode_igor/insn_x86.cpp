
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "insn_x86.h"
#include "util.h"

void Insn_x86::decode(const uint8_t *code) {
	_code = code;
	_pos = 0;
	_opcode = _code[_pos++];
	_call_addr = 0;
	_seg = -1;
}

void Insn_x86::mod_rm() {
	_mod = (_code[_pos] >> 6) & 3;
	_reg = (_code[_pos] >> 3) & 7;
	_rm  =  _code[_pos] & 7;
	_imm = 0;
	++_pos;
	switch (_mod) {
	case 0:
		if (_rm == 6) {
			_imm = (int16_t)READ_LE_UINT16(_code + _pos); _pos += 2;
		}
		break;
	case 1:
		_imm = (int8_t)_code[_pos++];
		break;
	case 2:
		_imm = (int16_t)READ_LE_UINT16(_code + _pos); _pos += 2;
		break;
	}
}

#define reg_ax 0
#define reg_cx 1
#define reg_dx 2
#define reg_bx 3
#define reg_sp 4
#define reg_bp 5
#define reg_si 6
#define reg_di 7

#define reg_al 0
#define reg_cl 1
#define reg_dl 2
#define reg_bl 3
#define reg_ah 4
#define reg_ch 5
#define reg_dh 6
#define reg_bh 7

#define seg_es 0
#define seg_cs 1
#define seg_ss 2
#define seg_ds 3

const char *Insn_x86::reg8(int reg) {
	static char buf[16];
	snprintf(buf, sizeof(buf), "r%d.b.%c", (reg & 3), ((reg >> 2) & 1) ? 'h' : 'l');
	return buf;
}

const char *Insn_x86::operand(const char *arg) {
	static char buf[32];
	if (_mod == 3) {
		if (arg && arg[1] == 'b') {
			snprintf(buf, sizeof(buf), "r%d.b.%c", (_rm & 3), ((_rm >> 2) & 1) ? 'h' : 'l');
		} else {
			snprintf(buf, sizeof(buf), "r%d.w", _rm);
		}
		return buf;
	}
	int seg = -1, reg = -1;
	switch (_rm) {
	case 3:
		seg = seg_ss;
		reg = reg_bp;
		break;
	case 4:
		seg = seg_ds;
		reg = reg_si;
		break;
	case 5:
		seg = seg_ds;
		reg = reg_di;
		break;
	case 6:
		if (_mod == 0) {
			snprintf(buf, sizeof(buf), "s%d:0x%X", _seg < 0 ? seg_ds : _seg, (uint16_t)_imm);
			return buf;
		}
		seg = seg_ss;
		reg = reg_bp;
		break;
	case 7:
		seg = seg_ds;
		reg = reg_bx;
		break;
	default:
		buf[0] = 0;
		fprintf(stderr, "unimplemented Insn_x86::operand %d\n", _rm);
		exit(1);
	}
	int count = snprintf(buf, sizeof(buf), "s%d:r%d.w", _seg < 0 ? seg : _seg, reg);
	switch (_rm) {
	case 3:
		count += snprintf(buf + count, sizeof(buf) - count, "+r%d.w", reg_di);
		break;
	}
	if (_imm < 0) {
		count += snprintf(buf + count, sizeof(buf) - count, "%d", _imm);
	} else if (_imm > 0) {
		count += snprintf(buf + count, sizeof(buf) - count, "+%d", _imm);
	}
	return buf;
}

int Insn_x86::imm(const char *arg) {
	if (strcmp(arg, "1") == 0) {
		return 1;
	}
	int imm = 0;
	assert(arg[0] == 'I');
	switch (arg[1]) {
	case 'v':
	case 'w':
		imm = READ_LE_UINT16(_code + _pos); _pos += 2;
		break;
	case 'b':
		imm = _code[_pos++];
		break;
	default:
		fprintf(stderr, "unimplemented Insn_x86::imm %c\n", arg[1]);
		exit(1);
	}
	return imm;
}

int Insn_x86::op_grp1(char *buf, const char *arg1, const char *arg2) {
	int count = 0;
	mod_rm();
	switch (_reg) {
	case 0: { // add
			const int arg = imm(arg2);
			count = sprintf(buf, "add.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	case 2: { // adc
			const int arg = imm(arg2);
			count = sprintf(buf, "adc.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	case 3: { // sbb
			const int arg = imm(arg2);
			count = sprintf(buf, "sbb.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	case 5: { // sub
			const int arg = imm(arg2);
			count = sprintf(buf, "sub.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	case 7: { // cmp
			const int arg = imm(arg2);
			count = sprintf(buf, "cmp.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	default:
		fprintf(stderr, "unimplemented Insn_x86::op_grp1 %d\n", _reg);
		exit(1);
	}
	return count;
}

int Insn_x86::op_grp2(char *buf, const char *arg1, const char *arg2) {
	int count = 0;
	mod_rm();
	switch (_reg) {
	case 4: { // shl
			const int arg = imm(arg2);
			count = sprintf(buf, "shl.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	case 5: { // shr
			const int arg = imm(arg2);
			count = sprintf(buf, "shr.%c     %s, 0x%X", arg1[1] == 'b' ? 'b' : 'w', operand(arg1), arg);
		}
		break;
	default:
		fprintf(stdout, "unimplemented Insn_x86::op_grp2 %d\n", _reg);
		exit(1);
	}
	return count;
}

int Insn_x86::op_grp3b(char *buf, const char *arg) {
	int count = 0;
	mod_rm();
	switch (_reg) {
	case 2: { // not
			count = sprintf(buf, "not       %s", operand(arg));
		}
		break;
	case 3: { // neg
			count = sprintf(buf, "neg       %s", operand(arg));
		}
		break;
	case 4: { // mul
			count = sprintf(buf, "mul.w     %s", operand(arg));
		}
		break;
	case 6: { // div
			count = sprintf(buf, "div.w     %s", operand(arg));
		}
		break;
	case 7: { // idiv
			count = sprintf(buf, "idiv.w    %s", operand(arg));
		}
		break;
	default:
		fprintf(stderr, "unimplemented Insn_x86::op_grp3b %d\n", _reg);
		exit(1);
	}
	return count;
}

int Insn_x86::op_grp4(char *buf, const char *arg) {
	int count = 0;
	mod_rm();
	switch (_reg) {
	case 0: { // inc
			count = sprintf(buf, "inc.%c     %s", arg[1] == 'b' ? 'b' : 'w', operand(arg));
		}
		break;
	case 1: { // dec
			count = sprintf(buf, "dec.%c     %s", arg[1] == 'b' ? 'b' : 'w', operand(arg));
		}
		break;
	default:
		fprintf(stderr, "unimplemented Insn_x86::op_grp4 %d\n", _reg);
		exit(1);
	}
	return count;
}

int Insn_x86::op_grp5(char *buf, const char *arg) {
	int count = 0;
	mod_rm();
	switch (_reg) {
	case 0: { // inc
			count = sprintf(buf, "inc.%c     %s", arg[1] == 'b' ? 'b' : 'w', operand(arg));
		}
		break;
	case 1: { // dec
			count = sprintf(buf, "dec.%c     %s", arg[1] == 'b' ? 'b' : 'w', operand(arg));
		}
		break;
	case 3: { // call
			count = sprintf(buf, "call       %s", operand(arg));
		}
		break;
	case 6: { // push
			assert(arg[0] == 'E');
			count = sprintf(buf, "push.%c    %s", arg[1] == 'b' ? 'b' : 'w', operand(arg));
		}
		break;
	default:
		fprintf(stderr, "unimplemented Insn_x86::op_grp5 %d\n", _reg);
		exit(1);
	}
	return count;
}

int Insn_x86::dump(char *buf) {
	int count = 0;
	switch (_opcode) {
	case 0x01: { // add Ev Gv
			mod_rm();
			count = sprintf(buf, "add.w     %s, r%d.w", operand("Ev"), _reg);
		}
		break;
	case 0x03: { // add Gv Ev
			mod_rm();
			count = sprintf(buf, "add.w     r%d.w, %s", _reg, operand("Ev"));
		}
		break;
	case 0x05: { // add ax Iv
			const uint16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "add.w     r%d.w, 0x%X", reg_ax, arg);
		}
		break;
	case 0x06: { // push es
			count = sprintf(buf, "push      s%d", seg_es);
		}
		break;
	case 0x07: { // pop es
			count = sprintf(buf, "pop       s%d", seg_es);
		}
		break;
	case 0x08: { // or Eb Gb
			mod_rm();
			count = sprintf(buf, "or.b      %s, %s", operand("Eb"), reg8(_reg));
		}
		break;
	case 0x09: { // or Ev Gv
			mod_rm();
			count = sprintf(buf, "or.w      %s, r%d.w", operand("Ev"), _reg);
		}
		break;
	case 0x0E: { // push cs
			count = sprintf(buf, "push      s%d", seg_cs);
		}
		break;
	case 0x13: { // adc Gv Ev
			mod_rm();
			count = sprintf(buf, "adc.w     r%d.w, %s", _reg, operand("Ev"));
		}
		break;
	case 0x16: { // push ss
			count = sprintf(buf, "push      s%d", seg_ss);
		}
		break;
	case 0x1B: { // sbb Gv Ev
			mod_rm();
			count = sprintf(buf, "sbb.w     r%d.w, %s", _reg, operand("Ev"));
		}
		break;
	case 0x1E: { // push ds
			count = sprintf(buf, "push      s%d", seg_ds);
		}
		break;
	case 0x1F: { // pop ds
			count = sprintf(buf, "pop       s%d", seg_ds);
		}
		break;
	case 0x26: { // es:
			_seg = seg_es;
			_opcode = _code[_pos++];
			count = dump(buf + count);
			count += sprintf(buf + count, " (s%d)", seg_es);
			_seg = -1;
		}
		break;
	case 0x28: { // sub Eb Gb
			mod_rm();
			count = sprintf(buf, "sub.b     %s, %s", operand("Eb"), reg8(_reg));
		}
		break;
	case 0x29: { // sub Ev Gv
			mod_rm();
			count = sprintf(buf, "sub.w     %s, r%d.w", operand("Ev"), _reg);
		}
		break;
	case 0x2B: { // sub Gv Ev
			mod_rm();
			count = sprintf(buf, "sub.w     r%d.w, %s", _reg, operand("Ev"));
		}
		break;
	case 0x2D: { // sub ax Iv
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "sub.w     r%d.w, 0x%X", reg_ax, imm);
		}
		break;
	case 0x30: { // xor Eb Gb
			mod_rm();
			count = sprintf(buf, "xor.b     %s, %s", operand("Eb"), reg8(_reg));
		}
		break;
	case 0x31: { // xor Ev Gv
			mod_rm();
			count = sprintf(buf, "xor.w     %s, r%d.w", operand("Ev"), _reg);
		}
		break;
	case 0x32: { // xor Gb Eb
			mod_rm();
			count = sprintf(buf, "xor.b     %s, %s", reg8(_reg), operand("Eb"));
		}
		break;
	case 0x38: { // cmp Eb, Gb
			mod_rm();
			count = sprintf(buf, "cmp.b     %s, %s", operand("Eb"), reg8(_reg));
		}
		break;
	case 0x3A: { // cmp Gb, Eb
			mod_rm();
			count = sprintf(buf, "cmp.b     %s, %s", reg8(_reg), operand("Eb"));
		}
		break;
	case 0x3B: { // cmp Gv Ev
			mod_rm();
			count = sprintf(buf, "cmp.w     r%d.w, %s", _reg, operand("Ev"));
		}
		break;
	case 0x3C: { // cmp al Ib
			const uint8_t arg = _code[_pos++];
			count = sprintf(buf, "cmp.b     %s, 0x%X", reg8(reg_al), arg);
		}
		break;
	case 0x3D: { // cmp ax Iv
			const uint16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "cmp.w     r%d.w, 0x%X", reg_ax, arg);
		}
		break;
	case 0x40 ... 0x47: { // inc ax
			count = sprintf(buf, "inc.w     r%d.w", _opcode - 0x40);
		}
		break;
	case 0x48 ... 0x4F: { // dec ax
			count = sprintf(buf, "dec.w     r%d.w", _opcode - 0x48);
		}
		break;
	case 0x50 ... 0x57: { // push ax
			count = sprintf(buf, "push.w    r%d.w", _opcode - 0x50);
		}
		break;
	case 0x58 ... 0x5F: { // pop ax
			count = sprintf(buf, "pop.w     r%d.w", _opcode - 0x58);
		}
		break;
	case 0x68: {
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "push.w    0x%X", imm);
		}
		break;
	case 0x69: { // 69 /r iw	imulw r/m16,r16	9-22/12-25 	word register := r/m16 * immediate word
			mod_rm();
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "imul.w    r%d.w, %s, 0x%X", _reg, operand(), imm);
		}
		break;
	case 0x6A: {
			const uint8_t imm = _code[_pos++];
			count = sprintf(buf, "push.b    0x%X", imm);
			break;
		}
		break;
	case 0x6B: { // 6B /r ib	imulw r/m16,r16	9-14/12-17 	word register := r/m16 * sign-extended immediate byte
			mod_rm();
			const uint16_t imm = (int16_t)_code[_pos++];
			count = sprintf(buf, "imul.w    r%d.w, %s, 0x%X", _reg, operand(), imm);
		}
		break;
	case 0x72: { // jb Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jb.r      %d", arg);
		}
		break;
	case 0x73: { // jnb Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jnb.r     %d", arg);
		}
		break;
	case 0x74: { // jz Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jz.r      %d", arg);
		}
		break;
	case 0x75: { // jnz Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jnz.r     %d", arg);
		}
		break;
	case 0x76: { // jbe Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jbe.r     %d", arg);
		}
		break;
	case 0x77: { // ja Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "ja.r      %d", arg);
		}
		break;
	case 0x79: { // jns Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jns.r     %d", arg);
		}
		break;
	case 0x7C: { // jl Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jl.r      %d", arg);
		}
		break;
	case 0x7D: { // jge Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jge.r     %d", arg);
		}
		break;
	case 0x7E: { // jle Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jle.r     %d", arg);
		}
		break;
	case 0x7F: { // jg Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jg.r      %d", arg);
		}
		break;
	case 0x80: { // grp1 Eb Ib
			count = op_grp1(buf, "Eb", "Ib");
		}
		break;
	case 0x81: { // grp1 Ev Iv
			count = op_grp1(buf, "Ev", "Iv");
		}
		break;
	case 0x83: { // grp1 Ev Ib
			count = op_grp1(buf, "Ev", "Ib");
		}
		break;
	case 0x84: { // test Gb Eb
			mod_rm();
			count = sprintf(buf, "test.b    %s, %s", reg8(_reg), operand("Eb"));
		}
		break;
	case 0x88: { // mov Eb Gb
			mod_rm();
			count = sprintf(buf, "mov.b     %s, %s", operand("Eb"), reg8(_reg));
		}
		break;
	case 0x89: { // mov Ev Gv
			mod_rm();
			count = sprintf(buf, "mov.w     %s, r%d.w", operand("Ev"), _reg);
		}
		break;
	case 0x8A: { // mov Gb Eb
			mod_rm();
			count = sprintf(buf, "mov.b     %s, %s", reg8(_reg), operand("Eb"));
		}
		break;
	case 0x8B: { // mov Gv Ev
			mod_rm();
			count = sprintf(buf, "mov.w     r%d.w, %s", _reg, operand("Ev"));
		}
		break;
	case 0x8C: { // mov Ew Sw
			mod_rm();
			count = sprintf(buf, "mov.w     %s, s%d", operand("Ew"), _reg);
		}
		break;
	case 0x8D: { // lea Gv m
			mod_rm();
			count = sprintf(buf, "lea.w     r%d.w, %s", _reg, operand("M"));
		}
		break;
	case 0x8E: { // mov Sw Ew
			mod_rm();
			count = sprintf(buf, "mov.w     s%d, %s", _reg, operand("Ew"));
		}
		break;
	case 0x91: { // xchg cx, ax
			count = sprintf(buf, "xchg.w    r%d.w, r%d.w", reg_cx, reg_ax);
		}
		break;
	case 0x92: { // xchg dx, ax
			count = sprintf(buf, "xchg.w    r%d.w, r%d.w", reg_dx, reg_ax);
		}
		break;
	case 0x98: { // cbw
			count = sprintf(buf, "cbw");
		}
		break;
	case 0x99: { // cwd
			count = sprintf(buf, "cwd");
		}
		break;
	case 0x9A: { // call Ap
			const uint16_t ip = READ_LE_UINT16(_code + _pos); _pos += 2;
			const uint16_t cs = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "call      cseg%03d:0x%04X", cs, ip);
			_call_addr = (cs << 16) | ip;
		}
		break;
	case 0xA0: { // mov al Ob
			const uint16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "mov.b     %s, s%d:0x%X", reg8(reg_al), _seg < 0 ? seg_ds : _seg, arg);
		}
		break;
	case 0xA1: { // mov ax Ov
			const uint16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "mov.w     r%d.w, s%d:0x%X", reg_ax, _seg < 0 ? seg_ds : _seg, arg);
		}
		break;
	case 0xA2: { // mov Ob al
			const uint16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "mov.b     s%d:0x%X, %s", _seg < 0 ? seg_ds : _seg, arg, reg8(reg_al));
		}
		break;
	case 0xA3: { // mov Ov ax
			const uint16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "mov.w     s%d:0x%X, r%d.w", _seg < 0 ? seg_ds : _seg, arg, reg_ax);
		}
		break;
	case 0xA4: { // movsb
			count = sprintf(buf, "movsb");
		}
		break;
	case 0xA5: { // movsw
			count = sprintf(buf, "movsw");
		}
		break;
	case 0xAA: { // stosb
			count = sprintf(buf, "stosb");
		}
		break;
	case 0xAC: { // lodsb
			count = sprintf(buf, "lodsb");
		}
		break;
	case 0xB0 ... 0xB7: { // mov al Ib
			const uint8_t imm = _code[_pos++];
			count = sprintf(buf, "mov.w     %s, 0x%X", reg8(_opcode - 0xB0), imm);
		}
		break;
	case 0xB8 ... 0xBF: { // mov ax Iv
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "mov.w     r%d.w, 0x%X", _opcode - 0xB8, imm);
		}
		break;
	case 0xC1: { // grp2 Ev Ib
			count = op_grp2(buf, "Ev", "Ib");
		}
		break;
	case 0xC2: { // ret Iw
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			assert((imm & 1) == 0);
			count = sprintf(buf, "ret       %d", imm);
		}
		break;
	case 0xC4: { // les Gv Mp
			mod_rm();
			count = sprintf(buf, "les.w     r%d.w, %s", _reg, operand());
		}
		break;
	case 0xC5: { // lds Gv Mp
			mod_rm();
			count = sprintf(buf, "lds.w     r%d.w, %s", _reg, operand());
		}
		break;
	case 0xC6: { // mov Eb Ib
			mod_rm();
			const uint8_t imm = _code[_pos++];
			count = sprintf(buf, "mov.b     %s, 0x%X", operand("Eb"), imm);
		}
		break;
	case 0xC7: { // mov Ev Iv
			mod_rm();
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "mov.w     %s, 0x%X", operand("Ev"), imm);
		}
		break;
	case 0xC9: { // leave
			count = sprintf(buf, "leave");
		}
		break;
	case 0xCA: { // retf Iw
			const uint16_t imm = READ_LE_UINT16(_code + _pos); _pos += 2;
			assert((imm & 1) == 0);
			count = sprintf(buf, "retf      %d", imm);
		}
		break;
	case 0xCB: { // retf
			count = sprintf(buf, "retf");
		}
		break;
	case 0xCD: { // int Ib
			const uint8_t imm = _code[_pos++];
			count = sprintf(buf, "int       0x%02X", imm);
		}
		break;
	case 0xD1: { // grp2 Ev 1
			count = op_grp2(buf, "Ev", "1");
		}
		break;
	case 0xE2: { // loop Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "loop.r    %d", arg);
		}
		break;
	case 0xE8: { // call Jv
			const int16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "call      %d", arg);
			_call_addr = (uint16_t)arg;
		}
		break;
	case 0xE9: { // jmp Jv
			const int16_t arg = READ_LE_UINT16(_code + _pos); _pos += 2;
			count = sprintf(buf, "jmp.r     %d", arg);
		}
		break;
	case 0xEB: { // jmp Jb
			const int8_t arg = _code[_pos++];
			count = sprintf(buf, "jmp.r     %d", arg);
		}
		break;
	case 0xEE: { // out dx, al
			count = sprintf(buf, "out       r%d.w, %s", reg_dx, reg8(reg_al));
		}
		break;
	case 0xF3: { // repz
			count = sprintf(buf, "repz ");
			_opcode = _code[_pos++];
			count += dump(buf + count);
		}
		break;
	case 0xF7: { // gpr3b Ev
			count = op_grp3b(buf, "Ev");
		}
		break;
	case 0xFC: { // cld
			count = sprintf(buf, "cld");
		}
		break;
	case 0xFE: { // grp4 Eb
			count = op_grp4(buf, "Eb");
		}
		break;
	case 0xFF: { // grp5 Ev
			count = op_grp5(buf, "Ev");
		}
		break;
	default:
		fprintf(stderr, "unimplemented Insn_x86::dump opcode 0x%02X\n", _opcode);
		exit(1);
	}
	return count;
}
