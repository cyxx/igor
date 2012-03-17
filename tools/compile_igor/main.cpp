
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "detect.h"
#include "opcodes.h"

static const bool _printGeneratedCode = false;

#define MAX_INSTRUCTIONS (1 << 16)

static Instruction _instructionsBuf[MAX_INSTRUCTIONS];
static int _instructionsCount;
static int _instructionsSize;

#define MAX_FUNCS 256

static uint32_t _funcs[MAX_FUNCS];
static int _funcsCount;

#define MAX_DUP_FUNCS 4096

struct DupFunction {
	uint32_t addr;
	int stats;
};
static DupFunction _dupFuncs[MAX_DUP_FUNCS];
static int _dupFuncsCount;

#define KILLED_FUNCTION 0xFFFF0000

#define NUM_PART 0
#define NUM_MAIN 9999

#define trap_loadActionData    0x0001
#define trap_setPaletteColor   0x0002
#define trap_loadImageData     0x0003
#define trap_setActionData     0x0004
#define trap_setDialogueData   0x0005
#define trap_loadDialogueData  0x0006
#define trap_setPaletteData    0x0007
#define trap_loadPaletteData   0x0008
#define trap_waitInput         0x0009
#define trap_setMouseRange     0x000A
#define trap_waitSound         0x000B

static void emit(Instruction &insn) {
	if (_instructionsCount >= MAX_INSTRUCTIONS) {
		fprintf(stderr, "instructions overflow");
		exit(1);
	}
	insn.addr = _instructionsSize;
	if (_printGeneratedCode) {
		fprintf(stdout, "%06X (%d) ", insn.addr, insn.size);
		for (int i = 0; i < insn.size; ++i) {
			fprintf(stdout, "%02X ", insn.code[i]);
		}
		fprintf(stdout, "\n");
	}
	_instructionsSize += insn.size + 1;
	_instructionsBuf[_instructionsCount] = insn;
	++_instructionsCount;
}

static void encodeReg(uint8_t *p, const char *arg) {
	const int num = strtol(arg + 1, 0, 10);
	p[0] = num;
	const char *q = strchr(arg, '.');
	switch (q[1]) {
	case 'b':
		if (q[3] == 'l' || q[3] == 'h') {
			p[1] = q[3];
		} else {
			fprintf(stderr, "invalid register byte '%c'\n", q[3]);
			exit(1);
		}
		break;
	case 'w':
		p[1] = 'w';
		break;
	default:
		fprintf(stderr, "invalid register size '%c'\n", q[1]);
		exit(1);
	}
}

static void encodeImm(uint8_t *p, const char *arg) {
	int mul = 1;
	switch (*arg) {
	case '-':
		mul = -1;
		++arg;
		break;
	case '+':
		++arg;
		break;
	}
	if (strncmp(arg, "0x", 2) == 0) {
		const uint16_t value = strtol(arg + 2, 0, 16) * mul;
		p[0] = value & 255;
		p[1] = value >> 8;
	} else if (*arg >= '0' && *arg <= '9') {
		const uint16_t value = strtol(arg, 0, 10) * mul;
		p[0] = value & 255;
		p[1] = value >> 8;
	} else {
		fprintf(stderr, "invalid constant '%s'\n", arg);
		exit(1);
	}
}

static int encodeOperand(uint8_t *p, const char *arg) {
	const char *sep = strchr(arg, ':');
	if (sep) {
		const char *reg2 = 0;
		int count = 1;
		int type = 0;
		if (*arg == 's') {
			type |= t_seg;
			const int num = strtol(arg + 1, 0, 10);
			p[1] = num;
			++count;
			// imm or reg+reg2+imm reg+imm
			++sep;
			if (*sep == 'r') {
				type |= t_reg;
				encodeReg(p + count, sep);
				count += 2;
				sep = strpbrk(sep, "+-");
				if (sep && sep[1] == 'r') {
					++sep;
					reg2 = sep;
					sep = strpbrk(sep + 1, "+-");
				}
			}
			if (sep) {
				type |= t_imm;
				encodeImm(p + count, sep);
				count += 2;
			}
			if (reg2) {
				type |= t_reg2;
				encodeReg(p + count, reg2);
				count += 2;
			}
		} else if (strncmp(arg, "0x", 2) == 0) {
			type |= t_imm;
			encodeImm(p + count, arg);
			count += 2;
		} else {
			fprintf(stderr, "invalid operand segment '%s'\n", arg);
			exit(1);
		}
		p[0] = type;
		return count;
	} else {
		if (*arg == 'r') {
			p[0] = t_reg;
			encodeReg(p + 1, arg);
			return 3;
		} else if (*arg == 's') {
			p[0] = t_seg;
			const int num = strtol(arg + 1, 0, 10);
			p[1] = num;
			return 2;
		} else if (*arg >= '0' && *arg <= '9') {
			p[0] = t_imm;
			encodeImm(p + 1, arg);
			return 3;
		} else {
			fprintf(stderr, "invalid operand '%s'\n", arg);
			exit(1);
		}
	}
	return 0;
}

static void emitJmp(Instruction &insn, char t, const char *op, const char *arg) {
	if (strcmp(op, "loop") == 0) {
		// dec cx
		insn.code[0] = op_sub;
		insn.code[1] = t_imm;
		encodeImm(&insn.code[2], "1");
		insn.code[4] = t_reg;
		encodeReg(&insn.code[5], "r1.w");
		insn.size = 7;
		emit(insn);
		// jnz
		emitJmp(insn, 'w', "jnz", arg);
	} else {
		int pos = 0;
		if (strcmp(op, "jmp") != 0) {
			const int len = strlen(op);
			if (len != 2 && len != 3) {
				fprintf(stderr, "invalid jump condition '%s'\n", op);
				exit(1);
			}
			// jz, je - jnz, jne
			// jg, jge (s) - ja, jae (u)
			// jl, jle (s) - jb, jbe (u)
			insn.code[0] = op_jmp2;
			const char *p = op + 1;
			if (*p == 'n') {
				++p;
				insn.code[1] = 1;
			} else {
				insn.code[1] = 0;
			}
			switch (*p) {
			case 'g':
			case 'l':
			case 'a':
			case 'b':
			case 's':
				insn.code[2] = *p;
				++p;
				break;
			case 'e':
			case 'z':
				insn.code[2] = ' ';
				break;
			default:
				fprintf(stderr, "invalid jump condition '%s'\n", op);
				exit(1);
			}
			insn.code[3] = (*p == 'e' || *p == 'z') ? 1 : 0;
			pos = 4;
		} else {
			insn.code[0] = op_jmp;
			pos = 1;
		}
		const uint16_t offset = strtol(arg, 0, 10);
		insn.code[pos] = offset & 255;
		insn.code[pos + 1] = offset >> 8;
		insn.size = pos + 2;
		emit(insn);
	}
}

static void emitStr(Instruction &insn, const char *repeat, const char *op) {
	insn.code[0] = op_str;
	switch (op[0]) {
	case 'l': // lodsb
	case 'm': // movsb
	case 's': // stosb
		break;
	default:
		fprintf(stderr, "invalid string opcode type '%c' %s\n", op[0], op);
		exit(1);
	}
	insn.code[1] = op[0];
	switch (op[4]) {
	case 'b':
	case 'w':
		break;
	default:
		fprintf(stderr, "invalid string opcode size '%c' %s\n", op[4], op);
		exit(1);
	}
	insn.code[2] = op[4];
	insn.code[3] = repeat ? 1 : 0; // lods, movs, stos do not affect zf ; ignore repeat condition
	insn.size = 4;
	emit(insn);
}

static void emitOp(Instruction &insn, char s, int op, const char *opname, char *arg) {
	insn.code[0] = op;
	insn.size = 1;
	switch (s) {
	case 'w':
		insn.code[0] |= 1 << 7;
		break;
	case 'b':
	case 0:
		break;
	default:
		fprintf(stderr, "invalid opcode size '%c'\n", s);
		exit(1);
	}
	if (op == op_call) {
		int seg, ptr;
		if (strncmp("cseg", arg, 4) == 0) {
			if (sscanf(arg, "cseg%03d:0x%04X", &seg, &ptr) != 2) {
				fprintf(stderr, "invalid call address '%s'\n", arg);
				exit(1);
			}
			insn.code[1] = seg & 255;
			insn.code[2] = seg >> 8;
			insn.code[3] = ptr & 255;
			insn.code[4] = ptr >> 8;
			insn.code[5] = 4; // call far
			insn.size = 6;
		} else if (sscanf(arg, "%d", &ptr) == 1) { // call.r
			seg = insn.seg; // same seg as current
			insn.code[1] = seg & 255;
			insn.code[2] = seg >> 8;
			ptr += insn.ptr + 3; // relative
			assert(ptr >= 0);
			insn.code[3] = ptr & 255;
			insn.code[4] = ptr >> 8;
			insn.code[5] = 2; // call near
			insn.size = 6;
		} else {
			insn.code[0] = op_call2;
			const int size = encodeOperand(&insn.code[1], arg);
			insn.code[1 + size] = 4; // call far
			insn.size = 2 + size;
		}
		emit(insn);
		return;
	} else if (op == op_ret) {
		int size = 0;
		if (arg) {
			if (sscanf(arg, "%d", &size) != 1) {
				fprintf(stderr, "invalid ret size %d\n", size);
				exit(1);
			}
		}
		if (strcmp(opname, "retf") == 0) {
			size += 4; // ret far
		} else {
			size += 2; // ret near
		}
		insn.code[1] = size & 255;
		insn.code[2] = size >> 8;
		insn.size = 3;
		emit(insn);
		return;
	} else if (arg) {
		char *next = strchr(arg, ',');
		if (next) {
			do {
				*next++ = 0;
			} while (*next == ' ');
		}
		// TODO: move (op == ...) checks here
		int size = 0;
		if (next) {
			size = encodeOperand(&insn.code[1], next);
		}
		size += encodeOperand(&insn.code[1 + size], arg);
		insn.size = 1 + size;
		if (insn.size > 16) {
			fprintf(stderr, "opcode size overflow %d\n", insn.size);
			exit(1);
		}
		if (op == op_xor) {
			// if arg1 == arg2 -> mov reg1, 0
			if (strcmp(arg, next) == 0) {
				insn.code[0] = op_mov | (insn.code[0] & 0x80);
				insn.code[1] = t_imm;
				encodeImm(insn.code + 2, "0");
				insn.code[4] = t_reg;
				encodeReg(insn.code + 5, arg);
				insn.size = 7;
				emit(insn);
				return;
			}
		} else if (op == op_inc || op == op_dec) {
			insn.code[0] = ((op == op_inc) ? op_add : op_sub) | (insn.code[0] & 0x80);
			insn.code[1] = t_imm;
			encodeImm(insn.code + 2, "1");
			size = encodeOperand(&insn.code[4], arg);
			insn.size = 4 + size;
			emit(insn);
			return;
		} else if (op == op_mul || op == op_div) {
			if (!next) { // 1 operand
				insn.code[0] = ((op == op_mul) ? op_mul2 : op_div2) | (insn.code[0] & 0x80);
				// TODO: idiv signed
				emit(insn);
			} else {
				// 3 operands: mov dst, arg3 ; mul dst, arg2
				char *arg3 = strchr(next, ',');
				if (arg3) { // always signed
					do {
						*arg3++ = 0;
					} while (*arg3 == ' ');
					if (strcmp(arg, next) == 0) { // mul.w r0.w, r0.w, imm
						int size = encodeOperand(&insn.code[1], arg3);
						size += encodeOperand(&insn.code[1 + size], arg);
						insn.size = 1 + size;
					} else {
						// mov dst, arg3
						Instruction tmp_insn = insn;
						tmp_insn.code[0] = op_mov | (insn.code[0] & 0x80);
						int size = encodeOperand(&tmp_insn.code[1], arg3);
						size += encodeOperand(&tmp_insn.code[1 + size], arg);
						tmp_insn.size = 1 + size;
						emit(tmp_insn);
					}
					emit(insn);
				} else {
					fprintf(stderr, "op_mul/op_div must have 1 or 3 operands\n");
					exit(1);
				}
			}
			return;
		}
	}
	emit(insn);
}

struct Opcode {
	const char *name;
	int op;
};

static const Opcode _opcodes[] = {
	{ "push",   op_push },
	{ "pop",    op_pop },
	{ "call",   op_call },
	{ "mov",    op_mov },
	{ "xor",    op_xor },
	{ "cmp",    op_cmp },
	{ "test",   op_test },
	{ "les",    op_les },
	{ "lds",    op_lds },
	{ "lea",    op_lea },
	{ "add",    op_add },
	{ "adc",    op_adc },
	{ "sub",    op_sub },
	{ "sbb",    op_sbb },
	{ "inc",    op_inc },
	{ "dec",    op_dec },
	{ "imul",   op_mul },
	{ "mul",    op_mul },
	{ "idiv",   op_div },
	{ "div",    op_div },
	{ "shl",    op_shl },
	{ "shr",    op_shr },
	{ "leave",  op_nop }, // unused
	{ "retf",   op_ret },
	{ "ret",    op_ret },
	{ "cbw",    op_cbw },
	{ "cwd",    op_cwd },
	{ "not",    op_not },
	{ "neg",    op_neg },
	{ "xchg",   op_swap },
	{ "or",     op_or },
	{ "out",    op_out }, // use trap_
	{ "int",    op_int }, // use trap_
	{ "cld",    op_nop }, // ignored (no std)
	{ 0, -1 }
};

static const char *_jmpOps[] = {
	"jmp", "jz", "jnz", "js", "jns", "jb", "jbe", "jnb", "ja", "jg", "jge", "jl", "jle", "loop", 0
};

static const char *_strOps[] = {
	"lods", "movs", "stos", 0
};

static bool parseOpcode(char *p, Instruction &insn) {
	char *arg = strchr(p, ' ');
	if (arg) {
		while (*arg == ' ') *arg++ = 0;
	}
	if (p[0] == 'j' || strncmp(p, "loop", 4) == 0) {
		char *q = strchr(p, '.');
		if (!q || q[1] != 'r') {
			fprintf(stderr, "invalid jump type '%c'\n", q[1]);
			return false;
		}
		*q = 0;
		for (int i = 0; _jmpOps[i]; ++i) {
			if (strcmp(p, _jmpOps[i]) == 0) {
				emitJmp(insn, q[1], p, arg);
				return true;
			}
		}
	}
	for (int i = 0; _strOps[i]; ++i) {
		if (strncmp(p, _strOps[i], 4) == 0) {
			emitStr(insn, 0, p);
			return true;
		}
	}
	if (strncmp(p, "rep", 3) == 0) {
		if (arg) {
			for (int i = 0; _strOps[i]; ++i) {
				if (strncmp(arg, _strOps[i], 4) == 0) {
					emitStr(insn, p, arg);
					return true;
				}
			}
		}
		fprintf(stderr, "invalid string opcode '%s %s'\n", p, arg);
	}
	char *q = strchr(p, '.');
	if (q) {
		switch (q[1]) {
		case 'b':
			break;
		case 'w':
			break;
		default:
			fprintf(stderr, "invalid opcode size '%c'\n", q[1]);
			return false;
		}
		*q = 0;
	}
	for (int i = 0; _opcodes[i].name; ++i) {
		if (strcmp(_opcodes[i].name, p) == 0) {
			emitOp(insn, q ? q[1] : 0, _opcodes[i].op, _opcodes[i].name, arg);
			return true;
		}
	}
	return false;
}

static void fixJmpOffsets() {
	_instructionsSize = 0;
	for (int i = 0; i < _instructionsCount; ++i) {
		_instructionsBuf[i].addr = _instructionsSize;
		_instructionsSize += _instructionsBuf[i].size + 1;
	}
	for (int i = 0; i < _instructionsCount; ++i) {
		Instruction &insn = _instructionsBuf[i];
		int pos = -1;
		switch (insn.code[0] & 0x7F) {
		case op_jmp2:
			pos = 4;
			break;
		case op_jmp:
			pos = 1;
			break;
		}
		if (pos < 0) {
			continue;
		}
		int offset = (int16_t)(insn.code[pos] | (insn.code[pos + 1] << 8));
			uint16_t addr = _instructionsBuf[i + 1].ptr + offset;
			int j;
			if (offset > 0) {
				// find first
				for (j = i + 1; j < _instructionsCount; ++j) {
					if (_instructionsBuf[j].ptr == addr) {
						break;
					}
				}
			} else {
				// find first
				int k = -1;
				for (j = i - 1; j >= 0; --j) {
					if (_instructionsBuf[j].ptr == addr) {
						k = j;
					} else if (k != -1) {
						break;
					}
				}
				j = k;
			}
			if (j < 0 || j >= _instructionsCount) {
				fprintf(stderr, "no target for offset %d at instruction %d (cseg%02d:%04X)\n", offset, i, insn.seg, insn.ptr);
				exit(1);
			}
			offset = _instructionsBuf[j].addr - _instructionsBuf[i].addr;
			if ((uint16_t)offset >= (1 << 16)) {
				fprintf(stderr, "jmp offset %d cannot be encoded on 16bits\n", offset);
				exit(1);
			}
		insn.code[pos] = offset & 255;
		insn.code[pos + 1] = offset >> 8;
	}
}

static bool checkCodeOps(Instruction *insn, int count, const uint8_t *ops) {
	for (int j = 0; j < count; ++j) {
		if ((insn[j].code[0] & 0x7F) != ops[j]) {
			return false;
		}
	}
	return true;
}

static void killCode(Instruction *insn) {
	while ((insn->code[0] & 0x7F) != op_ret) {
		insn->code[0] = op_nop;
		insn->size = 1;
		++insn;
	}
}

static void fixUpCode() {

#define emit_pushimm(i, val) \
	i.code[0] = op_push | 0x80; \
	i.code[1] = t_imm; \
	i.code[2] = val & 255; i.code[3] = val >> 8; \
	i.size = 4;
#define emit_pushreg(i, reg, s) \
	i.code[0] = op_push; \
	i.code[1] = t_reg; \
	i.code[2] = reg; \
	i.code[3] = s; \
	i.size = 4;
#define emit_pushseg(i, seg) \
	i.code[0] = op_push; \
	i.code[1] = t_seg; \
	i.code[2] = seg; \
	i.size = 3;
#define emit_call(i, seg, ptr) \
	i.code[0] = op_call; \
	i.code[1] = seg & 255; i.code[2] = seg >> 8; \
	i.code[3] = ptr & 255; i.code[4] = ptr >> 8; \
	i.code[5] = 4; \
	i.size = 6;
#define emit_nop(i) \
	i.code[0] = op_nop; \
	i.size = 1;

	// cmp word ptr es:0, 3FCDh
	// jnz short loc_7F0BC5
	// inc ax
	// mov di, ax
	// les ax, es:[di]
	static const uint8_t ops_fix1[5] = { op_cmp, op_jmp2, op_add, op_mov, op_les };
	int i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 5; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix1[j]) {
				break;
			}
		}
		if (j == 5) {
			if (insn[0].code[1] == t_imm && insn[0].code[2] == 0xCD && insn[0].code[3] == 0x3F) {
				fprintf(stdout, "Fixing 'relocate' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				for (j = 0; j < 5; ++j) {
					insn[j].code[0] = op_nop;
					insn[j].size = 1;
				}
				i += 5;
				continue;
			}
		}
		++i;
	}
	// mov al, _gameTicksTimerCounter
	// cmp al, _gameTicksTimer2
	// jz short loc_76502
	// mov al, _gameTicksTimer2
	// mov _gameTicksTimerCounter, al
	static const uint8_t ops_fix2[5] = { op_mov, op_cmp, op_jmp2, op_mov, op_mov };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 5; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix2[j]) {
				break;
			}
		}
		if (j == 5) {
			static const uint8_t pat1[] = { (t_seg | t_imm), 3, 0xC9, 0xEA, t_reg, 0x00, 'l' };
			static const uint8_t pat2[] = { (t_seg | t_imm), 3, 0xC8, 0xEA, t_reg, 0x00, 'l' };
			if (memcmp(insn[0].code + 1, pat1, 7) == 0 && memcmp(insn[1].code + 1, pat2, 7) == 0 && memcmp(insn[3].code + 1, pat2, 7) == 0) {
				fprintf(stdout, "Fixing 'busy_wait' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				for (j = 0; j < 4; ++j) {
					insn[j].code[0] = op_nop;
					insn[j].size = 1;
				}
				insn[4].code[0] = op_wait;
				insn[4].code[1] = 1;
				insn[4].size = 2;
				i += 5;
				continue;
			}
		}
		++i;
	}
	// mov _gameTicksTimer2, 0
	// cmp _gameTicksTimer2, 250
	// jbe short loc_54B111
	static const uint8_t ops_fix3[3] = { op_mov, op_cmp, op_jmp2 };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 3; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix3[j]) {
				break;
			}
		}
		if (j == 3) {
			static const uint8_t pat1[] = { t_imm, 0, 0, (t_seg | t_imm), 3, 0xC8, 0xEA };
			if (memcmp(insn[0].code + 1, pat1, 7) == 0 && insn[1].code[1] == t_imm && memcmp(insn[1].code + 4, pat1 + 3, 4) == 0) {
				const int count = insn[1].code[2] | (insn[1].code[3] << 8);
				fprintf(stdout, "Fixing 'busy_wait2' (%d) at 0x%06X (cseg%02d:%04X)\n", count, insn[0].addr, insn[0].seg, insn[0].ptr);
				assert(count < 256);
				insn[0].code[0] = op_wait;
				insn[0].code[1] = count == 0 ? 1 : count;
				insn[0].size = 2;
				emit_nop(insn[1]);
				emit_nop(insn[2]);
				i += 3;
				continue;
			}
		}
		++i;
	}
	// cmp _gameTicksTimer2, 0
	// jbe short loc_E82
	static const uint8_t ops_fix8[] = { op_cmp, op_jmp2 };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 2; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix8[j]) {
				break;
			}
		}
		if (j == 2) {
			static const uint8_t pat1[] = { t_imm, 0, 0, (t_seg | t_imm), 3, 0xC8, 0xEA };
			if (memcmp(insn[0].code + 4, pat1 + 3, sizeof(pat1) - 3) == 0 && insn[0].code[1] == pat1[0]) {
				const int count = insn[0].code[2] | (insn[0].code[3] << 8);
				fprintf(stdout, "Fixing 'busy_wait3' (%d) at 0x%06X (cseg%02d:%04X)\n", count, insn[0].addr, insn[0].seg, insn[0].ptr);
				insn[0].code[0] = op_wait;
				insn[0].code[1] = count == 0 ? 1 : count;
				insn[0].size = 2;
				emit_nop(insn[1]);
				i += 2;
				continue;
			}
		}
		++i;
	}
	// mov ax, seg cseg129 // 0
	// push ax
	// call createCodeSegment
	// mov _roomActionSeg, ax - mov _dialogueDataSeg, ax
	static const uint8_t ops_fix4[] = { op_mov, op_push, op_call, op_mov };
	static const uint8_t pat_action[] = { op_mov | 0x80, t_reg, 0, 'w', t_seg | t_imm, 3, 0x18, 0xFD };
	static const uint8_t pat_dialog[] = { op_mov | 0x80, t_reg, 0, 'w', t_seg | t_imm, 3, 0x1A, 0xFD };
	static const uint8_t pat_pal[]    = { op_mov | 0x80, t_reg, 0, 'w', t_seg | t_imm, 3, 0x1C, 0xFD };
	for (i = 0; i < _instructionsCount; ) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 4; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix4[j]) {
				break;
			}
		}
		if (j == 4) {
			static const uint8_t pat1[] = { op_call, 1, 0, 0x48, 0x3E };
			if (memcmp(insn[2].code, pat1, sizeof(pat1)) == 0) {
				if (memcmp(insn[3].code, pat_dialog, sizeof(pat_dialog)) == 0) {
					fprintf(stdout, "Fixing 'create_dialogue_data' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
					emit_call(insn[2], 999, trap_setDialogueData);
					i += 4;
					continue;
				}
				if (memcmp(insn[3].code, pat_action, sizeof(pat_action)) == 0) {
					fprintf(stdout, "Fixing 'create_action_data' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
					emit_call(insn[2], 999, trap_setActionData);
					i += 4;
					continue;
				}
				if (memcmp(insn[3].code, pat_pal, sizeof(pat_pal)) == 0) {
					fprintf(stdout, "Fixing 'create_palette_data' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
					emit_call(insn[2], 999, trap_setPaletteData);
					i += 4;
					continue;
				}
				fprintf(stderr, "createCodeSegment called at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				exit(1);
			}
		}
		++i;
	}
	// mov ax, _roomActionSeg / _dialogueDataSeg
	// push ax
	// mov di, offset DAT_CollegeCorridorMargaret // 2
	// pop es
	// push es
	// push di
	// mov ax, _roomActionSeg / _dialogueDataSeg
	// push ax
	// mov di, 4972h // 8
	// pop es
	// push es
	// push di
	// push 6273 // 12
	// call @Move$qm3Anyt14Word ; Move(var source, dest; count: Word)
	memset(&_roomDat, 0, sizeof(_roomDat));
	static const uint8_t ops_fix7[] = {  op_mov, op_push, op_mov, op_pop, op_push, op_push, op_mov, op_push, op_mov, op_pop, op_push, op_push, op_push, op_call };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 14; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix7[j]) {
				break;
			}
		}
		if (j == 14) {
			static const uint8_t pat_action[] = { op_mov | 0x80, t_seg | t_imm, 3, 0x18, 0xFD, t_reg, 0, 'w' };
			if (memcmp(insn[0].code, pat_action, sizeof(pat_action)) == 0 && memcmp(insn[6].code, pat_action, sizeof(pat_action)) == 0) {
				fprintf(stdout, "Fixing 'load_action_data' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				_roomDat.p.seg  = 0; // insn[-4].code[2] | (insn[-4].code[3] << 8);
				_roomDat.p.ptr  =  insn[2].code[2] |  (insn[2].code[3] << 8);
				_roomDat.p.size = insn[12].code[2] | (insn[12].code[3] << 8);
				_roomDat.offset =  insn[8].code[2] |  (insn[8].code[3] << 8);
//				fprintf(stdout, "room_action_data %d:%x %x %x\n", _roomDat.p.seg, _roomDat.p.ptr, _roomDat.p.size, _roomDat.offset);
				emit_pushimm(insn[0], _roomDat.p.seg);
				emit_pushimm(insn[1], _roomDat.p.ptr);
				emit_pushimm(insn[2], _roomDat.p.size);
				emit_pushimm(insn[3], _roomDat.offset);
				emit_call(insn[4], 999, trap_loadActionData);
				for (j = 5; j < 14; ++j) {
					emit_nop(insn[j]);
				}
				i += 14;
				continue;
			}
			static const uint8_t pat_dialog[] = { op_mov | 0x80, t_seg | t_imm, 3, 0x1A, 0xFD, t_reg, 0, 'w' };
			if (memcmp(insn[0].code, pat_dialog, sizeof(pat_dialog)) == 0 && memcmp(insn[6].code, pat_dialog, sizeof(pat_dialog)) == 0) {
				fprintf(stdout, "Fixing 'load_dialogue_data' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				_roomDat.p.seg  = 0; // insn[-4].code[2] | (insn[-4].code[3] << 8);
				_roomDat.p.ptr  =  insn[2].code[2] |  (insn[2].code[3] << 8);
				_roomDat.p.size = insn[12].code[2] | (insn[12].code[3] << 8);
				_roomDat.offset =  insn[8].code[2] |  (insn[8].code[3] << 8);
//				fprintf(stdout, "room_dialog_data %d:%x %x %x\n", _roomDat.p.seg, _roomDat.p.ptr, _roomDat.p.size, _roomDat.offset);
				emit_pushimm(insn[0], _roomDat.p.seg);
				emit_pushimm(insn[1], _roomDat.p.ptr);
				emit_pushimm(insn[2], _roomDat.p.size);
				emit_pushimm(insn[3], _roomDat.offset);
				emit_call(insn[4], 999, trap_loadDialogueData);
				for (j = 5; j < 14; ++j) {
					emit_nop(insn[j]);
				}
				i += 14;
				continue;
			}
#if 0
			static const uint8_t pat_palette[] = { op_mov | 0x80, t_seg | t_imm, 3, 0x1C, 0xFD, t_reg, 0, 'w' };
			if (memcmp(insn[0].code, pat_palette, sizeof(pat_palette)) == 0 && memcmp(insn[6].code, pat_palette, sizeof(pat_palette)) == 0) {
				fprintf(stdout, "Fixing 'load_palette_data' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				_roomDat.p.seg  = 0; // insn[-4].code[2] | (insn[-4].code[3] << 8);
				_roomDat.p.ptr  =  insn[2].code[2] |  (insn[2].code[3] << 8);
				_roomDat.p.size = insn[12].code[2] | (insn[12].code[3] << 8);
				_roomDat.offset =  insn[8].code[2] |  (insn[8].code[3] << 8);
//				fprintf(stdout, "room_dialog_data %d:%x %x %x\n", _roomDat.p.seg, _roomDat.p.ptr, _roomDat.p.size, _roomDat.offset);
				emit_pushimm(insn[0], _roomDat.p.seg);
				emit_pushimm(insn[1], _roomDat.p.ptr);
				emit_pushimm(insn[2], _roomDat.p.size);
				emit_pushimm(insn[3], _roomDat.offset);
				emit_call(insn[4], 999, trap_loadPaletteData);
				for (j = 5; j < 14; ++j) {
					emit_nop(insn[j]);
				}
				i += 14;
				continue;
			}
#endif
		}
		++i;
	}
	// mov al, 0F0h // 0
	// mov dx, 3C8h
	// out dx, al
	// mov al, [bp+arg_8]
	// mov dx, 3C9h // 4
	// out dx, al
	// mov al, [bp+arg_6]
	// mov dx, 3C9h
	// out dx, al // 8
	// mov al, [bp+arg_4]
	// mov dx, 3C9h
	// out dx, al
	static const uint8_t ops_fix5[] = { op_mov, op_mov, op_out };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 3 * 4; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix5[j % 3]) {
				break;
			}
		}
		if (j == 12) {
			static const uint8_t imm1[] = { t_imm, 0xC8, 3 };
			static const uint8_t imm2[] = { t_imm, 0xC9, 3 };
			if (memcmp(insn[1].code + 1, imm1, sizeof(imm1)) == 0 && memcmp(insn[4].code + 1, imm2, sizeof(imm2)) == 0) {
				fprintf(stdout, "Fixing 'out_0x3C8' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				emit_pushreg(insn[1], 0, 'l');
				emit_nop(insn[2]);
				emit_pushreg(insn[4], 0, 'l');
				emit_nop(insn[5]);
				emit_pushreg(insn[7], 0, 'l');
				emit_nop(insn[8]);
				emit_pushreg(insn[10], 0, 'l');
				emit_call(insn[11], 999, trap_setPaletteColor);
				i += 12;
				continue;
			}
		}
		++i;
	}
	// push 0
	// mov di, offset unk_56B6FA
	// push cs
	// push di
	// call drawActionSentence
	static const uint8_t ops_fix6[] = { op_push, op_mov, op_push, op_push, op_call };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 5; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix6[j]) {
				break;
			}
		}
		if (j == 5) {
			static const uint8_t imm0[] = { t_imm, 0, 0 };
			static const uint8_t pat1[] = { t_seg, 1 };
			static const uint8_t pat2[] = { 222, 0, 0x78, 0x10 };
			if (memcmp(insn[0].code + 1, imm0, sizeof(imm0)) == 0 && memcmp(insn[2].code + 1, pat1, sizeof(pat1)) == 0 && memcmp(insn[4].code + 1, pat2, sizeof(pat2)) == 0) {
				fprintf(stdout, "Fixing 'clear_action_sentence' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				emit_nop(insn[1]);
				emit_pushseg(insn[2], 3);
				emit_pushimm(insn[3], 0);
				i += 5;
				continue;
			}
		}
		++i;
	}
	// cseg148:003C mov di, offset IMG_Meanwhile
	// cseg148:003F mov ax, seg cseg148
	// ...
	// cseg148:0066 push 0B400h
	// cseg148:0069 call @Move$qm3Anyt14Word ; Move(var source, dest; count: Word)
	static const uint8_t pat1[] = { op_mov | 0x80, t_imm, 0xB7, 0, t_reg, 7, 'w' };
	static const uint8_t pat2[] = { op_mov | 0x80, t_imm, 148, 0, t_reg, 0, 'w' };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		if (memcmp(insn[0].code, pat1, sizeof(pat1)) == 0 && memcmp(insn[1].code, pat2, sizeof(pat2)) == 0) {
			static const uint8_t pat3[] = { op_push | 0x80, t_imm, 0, 0xB4 };
			static const uint8_t pat4[] = { op_call, 230, 0, 0x86, 0x16 };
			int j;
			for (j = 2; j < 32; ++j) {
				if (memcmp(insn[j].code, pat3, sizeof(pat3)) == 0 && memcmp(insn[j + 1].code, pat4, sizeof(pat4)) == 0) {
					break;
				}
			}
			if (j != 32) {
				fprintf(stdout, "Fixing 'load_meanwhile_img' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				++j;
				emit_call(insn[j], 999, trap_loadImageData);
				i += j;
			}
		}
		++i;
	}
	// cseg222:3937 loc_8341E7:
	// cseg222:3937 call skipDialogueText
	// cseg222:393C or al, al
	// cseg222:393E jz short loc_8341E7
	static const uint8_t ops_fix9[] = { op_call, op_or, op_jmp2 };
	i = 0;
	while (i < _instructionsCount) {
		static const uint8_t pat1[] = { op_call, 221, 0, 0x59, 0x28 };
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 3; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix9[j]) {
				break;
			}
		}
		if (j == 3) {
			if (memcmp(insn->code, pat1, sizeof(pat1)) == 0) {
				static const uint8_t pat3[] = { op_jmp2, 0, ' ', 1 }; // jz
				if (memcmp(insn[2].code, pat3, sizeof(pat3)) == 0) {
					const int offset = (int16_t)(insn[2].code[4] | (insn[2].code[5] << 8));
					if (offset == -9) {
						fprintf(stdout, "Fixing 'wait_input' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
						emit_call(insn[0], 999, trap_waitInput);
						i += 3;
						continue;
					}
				}
			}
		}
		++i;
	}
	// mov ax, 8
	// mov cx, 0
	// mov dx, 8Fh
	// int 33h
	static const uint8_t ops_fix10[] = { op_mov, op_mov, op_mov, op_int };
	i = 0;
	while (i < _instructionsCount) {
		Instruction *insn = &_instructionsBuf[i];
		int j = 0;
		for (; j < 4; ++j) {
			if ((insn[j].code[0] & 0x7F) != ops_fix10[j]) {
				break;
			}
		}
		if (j == 4) {
			static const uint8_t pat0[] = { op_mov | 0x80, t_imm, 8, 0, t_reg, 0, 'w' };
			static const uint8_t pat3[] = { op_int, t_imm, 0x33, 0 };
			if (memcmp(insn[0].code, pat0, sizeof(pat0)) == 0 && memcmp(insn[3].code, pat3, sizeof(pat3)) == 0) {
				fprintf(stdout, "Fixing 'set_mouse_vertical_range' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				emit_nop(insn[0]);
				const int y1 = insn[1].code[2] + (insn[1].code[3] << 8);
				emit_pushimm(insn[1], y1);
				const int y2 = insn[2].code[2] + (insn[2].code[3] << 8);
				emit_pushimm(insn[2], y2);
				emit_call(insn[3], 999, trap_setMouseRange);
				i += 4;
				continue;
			}
		}
		++i;
	}
	// cseg18:0BC6 mov al, _gameTicksTimer2
	// cseg18:0BC9 mov di, _gameState.counter
	// cseg18:0BCD cmp al, byte_87F61D[di] / byte_87F627[di]
	// cseg18:0BD1 jnz short loc_D5BD6
	i = 0;
	while (i < _instructionsCount) {
		static const uint8_t ops_fix[] = { op_mov, op_mov, op_cmp, op_jmp2 };
		Instruction *insn = &_instructionsBuf[i];
		if (checkCodeOps(insn, 4, ops_fix)) {
			static const uint8_t pat0[] = { (t_seg | t_imm), 3, 0xC8, 0xEA, t_reg, 0x00, 'l' };
			static const uint8_t pat2[] = { (t_seg | t_reg | t_imm), 3, 7, 'w' };
			if (memcmp(insn[0].code + 1, pat0, sizeof(pat0)) == 0 && memcmp(insn[2].code + 1, pat2, sizeof(pat2)) == 0) {
				fprintf(stdout, "Fixing 'busy_wait5' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				emit_nop(insn[0]);
				emit_nop(insn[2]);
				insn[3].code[0] = op_wait2;
				memcpy(insn[3].code + 1, pat2, sizeof(pat2));
				int size = 1 + sizeof(pat2);
				insn[3].code[size++] = insn[2].code[5];
				insn[3].code[size++] = insn[2].code[6];
				insn[3].size = size;
				i += 4;
				continue;
			}
		}
		++i;
	}
	// cseg09:12D3 les di, dword ptr stru_885460.ptr
	// cseg09:12D7 cmp word ptr es:[di], 0
	// cseg09:12DB jnz short loc_74003
	i = 0;
	while (i < _instructionsCount) {
		static const uint8_t ops_fix[] = { op_les, op_cmp, op_jmp2 };
		Instruction *insn = &_instructionsBuf[i];
		if (checkCodeOps(insn, 3, ops_fix)) {
			static const uint8_t pat0[] = { (t_seg | t_imm), 3, 0x90, 0x5E, t_reg, 7, 'w' };
			const int offset = (int16_t)(insn[3].code[4] | (insn[3].code[5] << 8));
			if (memcmp(insn[0].code + 1, pat0, sizeof(pat0)) == 0 && offset == -10) {
				fprintf(stdout, "Fixing 'sound_wait' at 0x%06X (cseg%02d:%04X)\n", insn[0].addr, insn[0].seg, insn[0].ptr);
				emit_nop(insn[0]);
				emit_nop(insn[1]);
				emit_call(insn[2], 999, trap_waitSound);
				i += 3;
				continue;
			}
		}
		++i;
	}



#undef emit_pushimm
#undef emit_pushreg
#undef emit_pushseg
#undef emit_call
#undef emit_nop
}

static void dumpOpcodeStats() {
	int statsUsed[op_count];
	memset(statsUsed, 0, sizeof(statsUsed));
	int cmpJmpOps[op_count];
	memset(cmpJmpOps, 0, sizeof(cmpJmpOps));
	for (int i = 0; i < _instructionsCount; ++i) {
		int op = _instructionsBuf[i].code[0] & 0x7F;
		++statsUsed[op];
		if ((op == op_jmp || op == op_jmp2) && i > 0) {
			if (_instructionsBuf[i].code[1] != 0 || _instructionsBuf[i].code[2] != 0) { // conditionnal jump
				op = _instructionsBuf[i - 1].code[0] & 0x7F;
				++cmpJmpOps[op];
			}
		}
	}
	fprintf(stdout, "unused opcodes: ");
	for (int i = 1; i < op_count; ++i) {
		if (statsUsed[i] == 0) {
			for (int j = 0; _opcodes[j].name; ++j) {
				if (_opcodes[j].op == i) {
					fprintf(stdout, "'%s' ", _opcodes[j].name);
					break;
				}
			}
		}
	}
	fprintf(stdout, "\n");
	fprintf(stdout, "cmp opcodes before jmp: ");
	for (int i = 0; i < op_count; ++i) {
		if (cmpJmpOps[i] != 0) {
			for (int j = 0; _opcodes[j].name; ++j) {
				if (_opcodes[j].op == i) {
					fprintf(stdout, "'%s' ", _opcodes[j].name);
					break;
				}
			}
		}
	}
	fprintf(stdout, "\n");
}

static void outputByte(FILE *out, uint8_t i) {
	fputc(i, out);
}

static void outputLE16(FILE *out, uint16_t i) {
	fputc(i & 255, out);
	fputc(i >> 8, out);
}

static void outputLE32(FILE *out, uint32_t i) {
	outputLE16(out, i & 0xFFFF);
	outputLE16(out, i >> 16);
}

static void outputRoomAnim(int num) {
	char name[32];
	snprintf(name, sizeof(name), "out/room_%03d.bin", num);
	FILE *room_out = fopen(name, "wb");
	outputLE32(room_out, 0);
	int roomsCount = 0;
	snprintf(name, sizeof(name), "out/anim_%03d.bin", num);
	FILE *anim_out = fopen(name, "wb");
	outputLE32(anim_out, 0);
	outputLE32(anim_out, 0);
	int animsCount = 0;
	int animsSize = 0;
	for (int i = 0; i < _funcsCount; ++i) {
		Instruction *insn = &_instructionsBuf[_funcs[i]];
		const uint32_t addr = (insn[0].seg << 16) | insn[0].ptr;
		if (detectCode_LoadRoomData(insn, addr)) {
			outputLE16(room_out, insn[0].seg);
			outputLE16(room_out, insn[0].ptr);
			for (int j = 0; j < res_count; ++j) {
				outputLE16(room_out, _data[j].seg);
				outputLE16(room_out, _data[j].ptr);
				outputLE16(room_out, _data[j].size);
				outputLE16(room_out, 0);
			}
			++roomsCount;
			killCode(insn);
			_funcs[i] = KILLED_FUNCTION;
			continue;
		}
		if (detectCode_LoadAnimData(insn, addr)) {
			outputLE16(anim_out, insn[0].seg);
			outputLE16(anim_out, insn[0].ptr);
			outputLE32(anim_out, _animSize * 8 + 2);
			outputLE16(anim_out, _animSize);
			for (int j = 0; j < _animSize; ++j) {
				outputLE16(anim_out, _anim[j].p.seg);
				outputLE16(anim_out, _anim[j].p.ptr);
				outputLE16(anim_out, _anim[j].p.size);
				outputLE16(anim_out, _anim[j].offset);
			}
			++animsCount;
			animsSize += _animSize * 8 + 2;
			killCode(insn);
			_funcs[i] = KILLED_FUNCTION;
			continue;
		}
	}
	fseek(room_out, 0, SEEK_SET);
	outputLE32(room_out, roomsCount);
	fclose(room_out);
	fprintf(stdout, "rooms count %d\n", roomsCount);
	fseek(anim_out, 0, SEEK_SET);
	outputLE32(anim_out, animsCount);
	outputLE32(anim_out, animsSize);
	fclose(anim_out);
	fprintf(stdout, "anims count %d size %d\n", animsCount, animsSize);
}

static void outputPartCode(int num) {
	char name[32];
	snprintf(name, sizeof(name), "out/code_%03d.bin", num);
	FILE *out = fopen(name, "wb");
	if (out) {
		outputLE16(out, _instructionsBuf[0].seg);
		outputLE16(out, _instructionsBuf[0].ptr);
		outputLE32(out, 0);
		outputLE32(out, 0); // funcsCount
		int funcsCount = 0;
		for (int i = 0; i < _funcsCount; ++i) {
			if (_funcs[i] == KILLED_FUNCTION) {
				continue;
			}
			Instruction &insn = _instructionsBuf[_funcs[i]];
			outputLE16(out, insn.seg);
			outputLE16(out, insn.ptr);
			outputLE32(out, insn.addr);
			++funcsCount;
		}
		outputLE32(out, _instructionsSize);
		for (int i = 0; i < _instructionsCount; ++i) {
			Instruction &insn = _instructionsBuf[i];
			fputc(insn.size, out);
			fwrite(insn.code, 1, insn.size, out);
		}
		fseek(out, 8, SEEK_SET);
		outputLE32(out, funcsCount);
		fclose(out);
	}
}

static void outputMain(const char *name) {
	FILE *out = fopen(name, "wb");
	if (out) {
		outputLE32(out, 0);
		outputLE16(out, _funcsCount);
		for (int i = 0; i < _funcsCount; ++i) {
			assert(_funcs[i] != KILLED_FUNCTION);
			Instruction &insn = _instructionsBuf[_funcs[i]];
			outputLE16(out, insn.seg);
			outputLE16(out, insn.ptr);
			outputLE32(out, insn.addr);
		}
		outputLE32(out, _instructionsSize);
		for (int i = 0; i < _instructionsCount; ++i) {
			Instruction &insn = _instructionsBuf[i];
			fputc(insn.size, out);
			fwrite(insn.code, 1, insn.size, out);
		}
		outputByte(out, _partCallsCount);
		for (int i = 0; i < _partCallsCount; ++i) {
			const int seg = _partCalls[i] >> 16;
			const int ptr = _partCalls[i] & 0xFFFF;
			outputLE16(out, seg);
			outputLE16(out, ptr);
			outputByte(out, i + 1); // index 1-based
		}
		fclose(out);
	}
}

static void compileCode(const char *path, int num) {
	FILE *fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "unable to open '%s'\n", path);
		exit(1);
	}
	fprintf(stdout, "Compiling '%s'...\n", path);
	int line = 0;
	char buf[256];
	while (fgets(buf, sizeof(buf), fp)) {
		++line;
		if (buf[0] == '#') {
			int seg, ptr;
			if (sscanf(buf + 2, "func sub_%03d_%04X", &seg, &ptr) == 2) {
				assert(_funcsCount < MAX_FUNCS);
				_funcs[_funcsCount] = _instructionsCount;
				++_funcsCount;
			}
			continue;
		}
		Instruction i;
		if (sscanf(buf, "cseg%03d:%04X", &i.seg, &i.ptr) != 2) {
			fprintf(stderr, "no seg:addr on line %d ('%s')\n", line, buf);
			exit(1);
		}
		char *p = strchr(buf, ' ');
		while (*p == ' ') ++p;
		char *q = p + strlen(p) - 1;
		while (q >= p && (*q == '\n' || *q == ' ')) *q-- = 0;
		if (_printGeneratedCode) {
			fprintf(stdout, "%s\n", p);
		}
		if (i.seg == 118 && i.ptr == 0x3856) { // busy loop
			fprintf(stdout, "Fixing 'busy_wait4' at 0x%06X (cseg%02d:%04X)\n", i.addr, i.seg, i.ptr);
			i.code[0] = op_wait;
			i.code[1] = 2;
			i.size = 2;
			emit(i);
		}
		if (!parseOpcode(p, i)) {
			fprintf(stderr, "invalid opcode on line %d ('%s')\n", line, buf);
			exit(1);
		}
	}
	fclose(fp);
	fprintf(stdout, "code size: %d instructions\n", _instructionsCount);
	if (num != NUM_MAIN) {
		outputRoomAnim(num);
	}
}

static void dumpParts() {
	if (detectCode_PartCall(_instructionsBuf, _instructionsCount)) {
		FILE *fp = fopen("parts.txt", "w");
		if (fp) {
			for (int i = 0; i < _partCallsCount; ++i) {
				const int seg = _partCalls[i] >> 16;
				const int ptr = _partCalls[i] & 0xFFFF;
				fprintf(fp, "%02d %03d %04X # cseg%02d:%04X\n", i + 1, seg, ptr, seg, ptr);
			}
			fclose(fp);
		}
	}
}

static void dumpFuncs(const char *name) {
	const char *p = strrchr(name, '/');
	p = p ? p + 1 : name;
	int seg, ptr;
	if (sscanf(p, "%03d_%04X.asm", &seg, &ptr) == 2) {
		char path[512];
		snprintf(path, sizeof(path), "out/funcs_%03d_%04X.txt", seg, ptr);
		FILE *out = fopen(path, "w");
		if (out) {
			for (int i = 0; i < _funcsCount; ++i) {
				if (_funcs[i] != KILLED_FUNCTION) {
					Instruction &insn = _instructionsBuf[_funcs[i]];
					fprintf(out, "cseg%03d:%04X\n", insn.seg, insn.ptr);
				}
			}
			fclose(out);
		}
	}
}

static int compareDupFunction(const void *p1, const void *p2) {
	const int a1 = ((const DupFunction *)p1)->addr;
	const int a2 = ((const DupFunction *)p2)->addr;
	return a1 - a2;
}

static void printDupFuncs(int argc, char *argv[]) {
	for (int i = 0; i < argc; ++i) {
		FILE *fp = fopen(argv[i], "r");
		if (fp) {
			int count = _dupFuncsCount;
			int seg, ptr;
			DupFunction func;
			while (fscanf(fp, "cseg%03d:%04X\n", &seg, &ptr) == 2) {
				func.addr = (seg << 16) | ptr;
				DupFunction *df = (DupFunction *)bsearch(&func, _dupFuncs, count, sizeof(DupFunction), compareDupFunction);
				if (df) {
					++df->stats;
				} else {
					func.stats = 1;
					assert(_dupFuncsCount < MAX_DUP_FUNCS);
					_dupFuncs[_dupFuncsCount] = func;
					++_dupFuncsCount;
				}
			}
			fclose(fp);
		}
		qsort(_dupFuncs, _dupFuncsCount, sizeof(DupFunction), compareDupFunction);
	}
	fprintf(stdout, "collected %d functions\n", _dupFuncsCount);
	for (int i = 0; i < _dupFuncsCount; ++i) {
		if (_dupFuncs[i].stats != 1) {
			fprintf(stdout, "cseg%03d:%04X stats %d\n", _dupFuncs[i].addr >> 16, _dupFuncs[i].addr & 0xFFFF, _dupFuncs[i].stats);
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc >= 3) {
		int num = -1;
		if (strcmp(argv[1], "--main") == 0) {
			num = NUM_MAIN;
		} else if (strncmp(argv[1], "--part=", 7) == 0) {
			num = NUM_PART + strtol(argv[1] + 7, 0, 10);
		} else if (strcmp(argv[1], "--dupfuncs") == 0) {
			printDupFuncs(argc - 2, argv + 2);
			return 0;
		} else {
			fprintf(stderr, "invalid compile mode '%s'\n", argv[1]);
			exit(1);
		}
		compileCode(argv[2], num);
		if (num == NUM_MAIN) {
			dumpParts();
			if (argc >= 4) {
				const int count = _instructionsCount;
				compileCode(argv[3], NUM_MAIN);
				if (detectCode_Action(_instructionsBuf + count, _instructionsCount - count)) {
					// TODO: check count matches IDA
					fprintf(stdout, "found %d actions\n", _actionsCount);
					for (int i = 0; i < _actionsCount; ++i) {
						assert(_funcsCount < MAX_FUNCS);
						_funcs[_funcsCount] = count + _actions[i];
						++_funcsCount;
					}
				}
			}
		}
		fixUpCode();
		fixJmpOffsets();
		if (_printGeneratedCode) {
			dumpOpcodeStats();
		}
		if (num == NUM_MAIN) {
			outputMain("out/main.bin");
		} else {
			outputPartCode(num);
			dumpFuncs(argv[2]);
		}
		fprintf(stdout, "done.\n");
	}
	return 0;
}
