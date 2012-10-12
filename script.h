/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SCRIPT_H__
#define SCRIPT_H__

#include <stdint.h>
#include "memory.h"

#define reg_ax 0
#define reg_cx 1
#define reg_dx 2
#define reg_bx 3
#define reg_sp 4
#define reg_bp 5
#define reg_si 6
#define reg_di 7

#define seg_es 0
#define seg_cs 1
#define seg_ss 2
#define seg_ds 3

struct Out {
	int port;
	int value;
};

struct Reg {
	union {
		uint16_t w;
		uint8_t b[2];
	} val;

	uint16_t get(char s) {
		switch (s) {
		case 'l':
			return val.b[0];
		case 'h':
			return val.b[1];
		case 'w':
			return val.w;
		}
		return 0;
	}
	void set(char s, uint16_t value) {
		switch (s) {
		case 'l':
			val.b[0] = value & 255;
			break;
		case 'h':
			val.b[1] = value & 255;
			break;
		case 'w':
			val.w = value;
			break;
		}
	}

};

#define MAX_CALL_STACK 16

struct Memory;

enum {
	k_zf = 1 << 0,
	k_sf = 1 << 1,
	k_cf = 1 << 2,
	k_jb = 1 << 3,
	k_jl = 1 << 4
};

struct CallStack {
	int pos;
	uint16_t cseg;
	uint32_t addr;
};

struct Script {

	Memory *_mem;

	Reg _regs[8];
	uint16_t _segs[8];
	uint32_t _callLocals;
	uint32_t _callSegPtr;
	int _waitTicks;
	uint8_t _f;
	Out _out;
	int _callsCount;
	CallStack _calls[MAX_CALL_STACK];

	Script();

	void push(uint16_t value);
	void pop(uint16_t &value);
	int sp() const { return _regs[reg_sp].val.w; }
	int ea(const uint8_t *code, int pos, uint16_t &offset);
	int assign(int b, const uint8_t *code, int pos, uint16_t value);
	int operand(int b, const uint8_t *code, int pos, uint16_t &value);
	void enter(int pos, uint32_t addr);
	int leave(int count);
	bool condition(bool no, char cmp, bool zero);
	int arith(uint16_t op1, uint16_t op2, int op);
	void lodstr(int w);
	void movstr(int w);
	void stostr(int w);
	void dump();
	void setflags(int b, int res, uint16_t opL, uint16_t opR);
	int executeOpcode(const uint8_t *code, int pos);
};

#endif
