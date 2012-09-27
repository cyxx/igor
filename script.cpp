/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disasm.h"
#include "memory.h"
#include "script.h"
#include "tools/compile_igor/opcodes.h"
#include "util.h"

static const bool _debug = false;

Script::Script() {
	memset(_regs, 0, sizeof(_regs));
	_regs[reg_sp].val.w = sizeof(_mem->_stack) / sizeof(uint16_t);
	memset(_segs, 0, sizeof(_segs));
	_segs[seg_ss] = STACK_SEG;
	_callLocals = 0;
	_callSegPtr = 0;
	_waitTicks = 0;
	_f = 0;
	memset(&_out, 0, sizeof(_out));
	memset(_calls, 0, sizeof(_calls));
	_callsCount = 0;
}

int Script::ea(const uint8_t *code, int pos, uint16_t &offset) {
	const uint8_t mask = code[pos - 2];
	offset = 0;
	if (mask & t_reg) {
		const int i = code[pos];
		offset = _regs[i].get(code[pos + 1]);
		pos += 2;
	}
	if (mask & t_imm) {
		offset += (int16_t)READ_LE_UINT16(code + pos);
		pos += 2;
	}
	if (mask & t_reg2) {
		const int i = code[pos];
		offset += _regs[i].get(code[pos + 1]);
		pos += 2;
	}
	return pos;
}

int Script::assign(int b, const uint8_t *code, int pos, uint16_t value) {
	int i;
	uint16_t offset;
	const uint8_t mask = code[pos++];
	switch (mask) {
	case t_reg:
		i = code[pos];
		_regs[i].set(code[pos + 1], value);
		pos += 2;
		break;
	case t_seg:
		i = code[pos++];
		_segs[i] = value;
		break;
	default:
		if ((mask & t_seg) == 0) {
			fprintf(stderr, "Script::assign() invalid operand mask %d\n", mask);
			exit(1);
		}
		i = code[pos++];
		pos = ea(code, pos, offset);
		if (b) {
			_mem->writeByte(_segs[i], offset, value);
		} else {
			_mem->writeUint16(_segs[i], offset, value);
		}
		break;
	}
	return pos;
}

int Script::operand(int b, const uint8_t *code, int pos, uint16_t &value) {
	int i;
	uint16_t offset;
	const uint8_t mask = code[pos++];
	switch (mask) {
	case t_imm:
		value = READ_LE_UINT16(code + pos);
		pos += 2;
		break;
	case t_reg:
		i = code[pos];
		value = _regs[i].get(code[pos + 1]);
		pos += 2;
		break;
	case t_seg:
		i = code[pos++];
		value = _segs[i];
		break;
	default:
		if ((mask & t_seg) == 0) {
			fprintf(stderr, "Script::operand() invalid operand mask %d\n", mask);
			exit(1);
		}
		i = code[pos++];
		pos = ea(code, pos, offset);
		if (b) {
			value = _mem->readByte(_segs[i], offset);
		} else {
			value = _mem->readUint16(_segs[i], offset);
		}
		break;
	}
	return pos;
}

void Script::push(uint16_t value) {
	_regs[reg_sp].val.w -= 2;
	_mem->writeUint16(_segs[seg_ss], _regs[reg_sp].val.w, value);
if (_debug) fprintf(stdout, "Script::push value %d offset %d\n", value, sp() );
}

void Script::pop(uint16_t &value) {
	value = _mem->readUint16(_segs[seg_ss], _regs[reg_sp].val.w);
	_regs[reg_sp].val.w += 2;
if (_debug) fprintf(stdout, "Script::pop value %d offset %d\n", value, sp() );
}

void Script::enter(int pos, uint32_t addr) {
	if (_callsCount >= MAX_CALL_STACK) {
		fprintf(stderr, "Script::enter() calls count overflow\n");
		exit(1);
	}
	_regs[reg_sp].val.w -= _callLocals;
	_calls[_callsCount].pos = pos;
	_calls[_callsCount].cseg = _segs[seg_cs];
	_calls[_callsCount].addr = addr;
	++_callsCount;
	_segs[seg_cs] = addr >> 16;
}

int Script::leave(int count) {
	assert((count & 1) == 0);
	_regs[reg_sp].val.w += count;
	assert(_callsCount > 0);
	--_callsCount;
	_segs[seg_cs] = _calls[_callsCount].cseg;
	return _calls[_callsCount].pos;
}

bool Script::condition(bool no, char cmp, bool zero) {
	bool res = false;
	switch (cmp) {
	case 'a':
		res = (_f & k_jb) == 0;
		break;
	case 'b':
		res = (_f & k_jb) != 0;
		break;
	case 'g':
		res = (_f & k_jl) == 0;
		break;
	case 'l':
		res = (_f & k_jl) != 0;
		break;
	case 's':
		res = (_f & k_sf) != 0;
		break;
	case ' ':
		res = false;
		break;
	default:
		fprintf(stderr, "Script::condition() unimplemented cond '%d %c %d'\n", no, cmp, zero);
		exit(1);
	}
	if (zero && !res) {
		res = (_f & k_zf) != 0;
	}
	if (!zero && res) {
		res = (_f & k_zf) == 0;
	}
	if (no) {
		res = !res;
	}
	return res;
}

int Script::arith(uint16_t valL, uint16_t valR, int op) {
	int res = 0;
	switch (op) {
	case op_add:
		res = valL + valR;
		break;
	case op_adc:
		res = valL + valR;
		if ((_f & k_cf) != 0) {
			++res;
		}
		break;
	case op_sub:
		res = valL - valR;
		break;
	case op_sbb:
		res = valL - valR;
		if ((_f & k_cf) != 0) {
			--res;
		}
		break;
	case op_or:
		res = valL | valR;
		break;
	case op_xor:
		res = valL ^ valR;
		break;
	case op_shl:
		res = valL << valR;
		break;
	case op_shr:
		res = valL >> valR;
		break;
	default:
		fprintf(stderr, "Script::arith() unimplemented op %d\n", op);
		exit(1);
	}
	return res;
}

void Script::lodstr(int w) {
	if (!w) {
		_regs[reg_ax].val.b[0] = _mem->readByte(_segs[seg_ds], _regs[reg_si].val.w);
		++_regs[reg_si].val.w;
	} else {
		_regs[reg_ax].val.w = _mem->readUint16(_segs[seg_ds], _regs[reg_si].val.w);
		_regs[reg_si].val.w += 2;
	}
}

void Script::movstr(int w) {
	const uint8_t *src = (const uint8_t *)_mem->getPtr(_segs[seg_ds], _regs[reg_si].val.w);
	uint8_t *dst = (uint8_t *)_mem->getPtr(_segs[seg_es], _regs[reg_di].val.w);
	if (!w) {
		*dst = *src;
		++_regs[reg_si].val.w;
		++_regs[reg_di].val.w;
	} else {
		memcpy(dst, src, 2);
		_regs[reg_si].val.w += 2;
		_regs[reg_di].val.w += 2;
	}
}

void Script::stostr(int w) {
	uint8_t *dst = (uint8_t *)_mem->getPtr(_segs[seg_es], _regs[reg_di].val.w);
	if (!w) {
		*dst = _regs[reg_ax].val.b[0];
		++_regs[reg_di].val.w;
	} else {
		WRITE_LE_UINT16(dst, _regs[reg_ax].val.w);
		_regs[reg_di].val.w += 2;
	}
}

void Script::dump() {
	for (int i = 0; i < 8; ++i) {
		fprintf(stdout, "r%d.w=0x%04X ", i, _regs[i].val.w);
	}
	for (int i = 0; i < 4; ++i) {
		fprintf(stdout, "s%d=0x%04X ", i, _segs[i]);
	}
	fprintf(stdout, "_f=0x%02X\n", _f);
}

void Script::setflags(int res, uint16_t opL, uint16_t opR) {
	_f = 0;
	if (res == 0) {
		_f |= k_zf;
	} else if (((uint16_t)res) & 0x8000) {
		_f |= k_sf;
	}
	if ((uint32_t)(res >> 16) != 0) {
		_f |= k_cf;
	}
	if (opL < opR) {
		_f |= k_jb;
	}
	if ((int16_t)opL < (int16_t)opR) {
		_f |= k_jl;
	}
}

int Script::executeOpcode(const uint8_t *code, int pos) {
if (_debug) {
	dump();
	disasmCode(code, pos, stdout);
}
	const int size = code[pos];
	uint8_t opcode = code[pos + 1];
	const int b = (opcode & 0x80) == 0;
	opcode &= 0x7F;
	switch (opcode) {
	case op_nop: {
		}
		break;
	case op_jmp: {
			const int16_t offset = READ_LE_UINT16(code + pos + 2);
			return pos + offset;
		}
		break;
	case op_call: {
			const uint16_t seg = READ_LE_UINT16(code + pos + 2);
			const uint16_t ptr = READ_LE_UINT16(code + pos + 4);
			_callLocals = code[pos + 6];
			_callSegPtr = (seg << 16) | ptr;
		}
		break;
	case op_ret: {
			_regs[reg_sp].val.w = _regs[reg_bp].val.w;
			pop(_regs[reg_bp].val.w);
			const uint16_t size = READ_LE_UINT16(code + pos + 2);
			return leave(size);
		}
		break;
	case op_push: {
			uint16_t value;
			operand(b, code, pos + 2, value);
			push(value);
		}
		break;
	case op_pop: {
			uint16_t value;
			pop(value);
			assign(b, code, pos + 2, value);
		}
		break;
	case op_add:
	case op_adc:
	case op_sub:
	case op_sbb:
	case op_mov:
	case op_or:
	case op_xor:
	case op_shl:
	case op_shr: {
			int res;
			uint16_t opL = 0, opR;
			const int pos2 = operand(b, code, pos + 2, opR);
			if (opcode == op_mov) {
				res = opR;
			} else {
				operand(b, code, pos2, opL);
				res = arith(opL, opR, opcode);
			}
			assign(b, code, pos2, res);
			setflags(res, opL, opR);
		}
		break;
	case op_mul:
	case op_div: {
			uint16_t opL;
			const int pos2 = operand(b, code, pos + 2, opL);
			if (code[pos2] != t_reg) {
				fprintf(stderr, "invalid op_mul/op_div mask %d\n", code[pos2]);
				exit(1);
			}
			const int i = code[pos2 + 1];
			switch (opcode) {
			case op_mul:
				if (b) {
					const uint16_t res = _regs[i].val.b[0] * opL;
					_regs[i].val.b[0] = res & 255;
				} else {
					const uint32_t res = _regs[i].val.w * opL;
					_regs[i].val.w = res & 0xFFFF;
				}
				break;
			case op_div:
				if (opL == 0) {
					fprintf(stderr, "op_div by 0");
					exit(1);
				}
				if (b) {
					_regs[i].val.b[0] /= opL;
				} else {
					_regs[i].val.w /= opL;
				}
				break;
			}
		}
		break;
	case op_mul2:
	case op_div2: {
			uint16_t opL;
			operand(b, code, pos + 2, opL);
			switch (opcode) {
			case op_mul2:
				if (b) {
					const uint16_t res = _regs[reg_ax].val.b[0] * opL;
					_regs[reg_ax].val.w = res;
				} else {
					const uint32_t res = _regs[reg_ax].val.w * opL;
					_regs[reg_dx].val.w = res >> 16;
					_regs[reg_ax].val.w = res & 0xFFFF;
				}
				break;
			case op_div2:
				if (opL == 0) {
					fprintf(stderr, "op_div2 by 0");
					exit(1);
				}
				if (b) {
					_regs[reg_ax].val.b[1] = _regs[reg_ax].val.b[0] % opL;
					_regs[reg_ax].val.b[0] /= opL;
				} else {
					_regs[reg_dx].val.w = _regs[reg_ax].val.w % opL;
					_regs[reg_ax].val.w /= opL;
				}
				break;
			}
		}
		break;
	case op_cmp: {
			uint16_t opL, opR;
			const int pos2 = operand(b, code, pos + 2, opR);
			operand(b, code, pos2, opL);
			const int res = opL - opR;
			setflags(res, opL, opR);
if (_debug) fprintf(stdout, "op_cmp(%d,%d) _f %d\n", opL, opR, _f);
		}
		break;
	case op_test: {
			uint16_t opL, opR;
			const int pos2 = operand(b, code, pos + 2, opR);
			operand(b, code, pos2, opL);
			const int res = opL & opR;
			setflags(res, opL, opR);
if (_debug) fprintf(stdout, "op_test(%d,%d) _f %d\n", opL, opR, _f);
		}
		break;
	case op_wait: {
			_waitTicks = code[pos + 2];
		}
		break;
	case op_out: {
			_out.port = _regs[reg_dx].val.w;
			_out.value = _regs[reg_ax].val.w;
		}
		break;
	case op_les:
	case op_lds: {
			static const int segs[] = { seg_es, seg_ds };
			if ((code[pos + 2] & t_seg) == 0) {
				fprintf(stderr, "invalid op_les mask %d\n", code[pos + 2]);
				exit(1);
			}
			const int i = code[pos + 3];
			uint16_t offset;
			const int pos2 = ea(code, pos + 4, offset);
			const uint8_t *ptr = (const uint8_t *)_mem->getPtr(_segs[i], offset);
			assign(b, code, pos2, READ_LE_UINT16(ptr));
			_segs[segs[opcode - op_les]] = READ_LE_UINT16(ptr + 2);
		}
		break;
	case op_lea: {
			if ((code[pos + 2] & t_seg) == 0) {
				fprintf(stderr, "invalid op_lea mask %d\n", code[pos + 2]);
				exit(1);
			}
			uint16_t offset;
			const int pos2 = ea(code, pos + 4, offset);
			assign(b, code, pos2, offset);
		}
		break;
	case op_not:
	case op_neg: {
			uint16_t res = 0, value;
			operand(b, code, pos + 2, value);
			switch (opcode) {
			case op_not:
				res = !value;
				break;
			case op_neg:
				res = -(int16_t)value;
				break;
			}
			assign(b, code, pos + 2, res);
		}
		break;
	case op_cbw: {
			const int16_t res = (int8_t)_regs[reg_ax].val.b[0];
			_regs[reg_ax].val.w = res;
		}
		break;
	case op_cwd: {
			const int32_t res = (int16_t)_regs[reg_ax].val.w;
			_regs[reg_dx].val.w = res >> 16;
		}
		break;
	case op_swap: {
			uint16_t opL, opR;
			const int pos2 = operand(b, code, pos + 2, opR);
			operand(b, code, pos2, opL);
			assign(b, code, pos + 2, opL);
			assign(b, code, pos2, opR);
		}
		break;
	case op_str: {
			const int repeat = code[pos + 4];
			void (Script::*strOp)(int w) = 0;
			const int w = (code[pos + 3] == 'w');
			switch (code[pos + 2]) {
			case 'l':
				strOp = &Script::lodstr;
				break;
			case 'm':
				strOp = &Script::movstr;
				break;
			case 's':
				strOp = &Script::stostr;
				break;
			default:
				fprintf(stderr, "unhandled op_str '%c'\n", code[pos + 2]);
				exit(1);
			}
			if (!repeat) {
				(this->*strOp)(w);
			} else {
				while (_regs[reg_cx].val.w != 0) {
					(this->*strOp)(w);
					--_regs[reg_cx].val.w;
				}
			}
		}
		break;
	case op_jmp2: {
			const int16_t offset = READ_LE_UINT16(code + pos + 5);
			if (condition(code[pos + 2] != 0, code[pos + 3], code[pos + 4] != 0)) {
				return pos + offset;
			}
		}
		break;
	case op_call2: {
			if ((code[pos + 2] & t_seg) == 0) {
				fprintf(stderr, "invalid op_call2 mask %d\n", code[pos + 2]);
				exit(1);
			}
			const int i = code[pos + 3];
			uint16_t ptr;
			const int pos2 = ea(code, pos + 4, ptr);
			_callLocals = code[pos2];
			const uint8_t *p = (const uint8_t *)_mem->getPtr(_segs[i], ptr);
			_callSegPtr = READ_LE_UINT32(p);
if (_debug) fprintf(stdout, "op_call2 %d:%x locals %d %d:%x\n", _segs[i], ptr, _callLocals, _callSegPtr>>16, _callSegPtr&0xFFFF);
		}
		break;
	case op_wait2: {
			uint16_t value;
			operand(b, code, pos + 2, value);
			_waitTicks = value;
		}
		break;
	default:
		fprintf(stderr, "Script::executeOpcode() unimplemented opcode %d\n", opcode);
		exit(1);
	}
	return pos + 1 + size;
}
