/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <stdio.h>
#include <stdint.h>
#include "tools/compile_igor/opcodes.h"
#include "util.h"

static int operand(const uint8_t *p, int pos, char *str) {
	switch (p[pos]) {
	case t_seg:
		sprintf(str, "s%d", p[pos + 1]);
		pos += 2;
		break;
	case t_reg:
		sprintf(str, "r%d.%c", p[pos + 1], p[pos + 2]);
		pos += 3;
		break;
	case t_imm:
		sprintf(str, "0x%X", READ_LE_UINT16(p + pos + 1));
		pos += 3;
		break;
	case t_seg | t_reg:
		sprintf(str, "s%d:r%d.%c", p[pos + 1], p[pos + 2], p[pos + 3]);
		pos += 4;
		break;
	case t_seg | t_imm:
		sprintf(str, "s%d:0x%X", p[pos + 1], READ_LE_UINT16(p + pos + 2));
		pos += 4;
		break;
	case t_seg | t_reg | t_imm:
		sprintf(str, "s%d:r%d.%c+0x%X", p[pos + 1], p[pos + 2], p[pos + 3], READ_LE_UINT16(p + pos + 4));
		pos += 6;
		break;
	case t_seg | t_reg | t_imm | t_reg2:
		sprintf(str, "s%d:r%d.%c+0x%X+r%d.%c", p[pos + 1], p[pos + 2], p[pos + 3], READ_LE_UINT16(p + pos + 4), p[pos + 6], p[pos + 7]);
		pos += 8;
		break;
	default:
		str[0] = 0;
		break;
	}
	return pos;
}

void disasmCode(const uint8_t *code, int pos, FILE *out) {
	char opL[32], opR[32];
	const int size = code[pos];
	fprintf(out, "%04X: ", pos);
	uint8_t opcode = code[pos + 1];
	const int b = (opcode & 0x80) == 0;
	opcode &= 0x7F;
	switch (opcode) {
	case op_nop: {
			fprintf(out, "op_nop");
		}
		break;
	case op_jmp: {
			const int16_t offset = READ_LE_UINT16(code + pos + 2);
			fprintf(out, "op_jmp %d (%04X)", offset, pos + offset);
                }
                break;
	case op_call: {
			const uint16_t seg = READ_LE_UINT16(code + pos + 2);
			const uint16_t ptr = READ_LE_UINT16(code + pos + 4);
			fprintf(out, "op_call %03d %04X", seg, ptr);
		}
		break;
	case op_ret: {
			uint16_t sz = 0;
			if (size != 1) {
				sz = READ_LE_UINT16(code + pos + 2);
			}
			fprintf(out, "op_ret %d", sz);
		}
		break;
	case op_push: {
			operand(code, pos + 2, opR);
			fprintf(out, "op_push %s", opR);
		}
		break;
	case op_pop: {
			operand(code, pos + 2, opL);
			fprintf(out, "op_pop %s", opL);
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
			static const char *ops[] = { "op_add", "op_adc", "op_sub", "op_sbb", "op_mov", "op_or", "op_xor", "op_shl", "op_shr" };
			const int pos2 = operand(code, pos + 2, opR);
			operand(code, pos2, opL);
			fprintf(out, "%s.%c %s, %s", ops[opcode - op_add], b ? 'b' : 'w', opL, opR);
                }
		break;
	case op_mul:
	case op_div: {
			static const char *ops[] = { "op_mul", "op_div" };
			const int pos2 = operand(code, pos + 2, opR);
			operand(code, pos2, opL);
			fprintf(out, "%s.%c %s, %s", ops[opcode - op_mul], b ? 'b' : 'w', opL, opR);
		}
		break;
	case op_mul2:
	case op_div2: {
			static const char *ops[] = { "op_mul2", "op_div2" };
			operand(code, pos + 2, opR);
			fprintf(out, "%s.%c %s", ops[opcode - op_mul2], b ? 'b' : 'w', opR);
		}
		break;
	case op_cmp: {
			const int pos2 = operand(code, pos + 2, opR);
			operand(code, pos2, opL);
			fprintf(out, "op_cmp %s, %s", opL, opR);
		}
		break;
	case op_les:
	case op_lds:
	case op_lea: {
			static const char *ops[] = { "op_les", "op_lds", "op_lea" };
			const int pos2 = operand(code, pos + 2, opR);
			operand(code, pos2, opL);
			fprintf(out, "%s %s,%s", ops[opcode - op_les], opL, opR);
		}
		break;
	case op_cwd: {
			fprintf(out, "op_cwd");
		}
		break;
	case op_swap: {
			const int pos2 = operand(code, pos + 2, opR);
			operand(code, pos2, opL);
			fprintf(out, "op_swap %s, %s", opL, opR);
		}
		break;
	case op_str: {
			fprintf(out, "op_str.%c %c repeat %d", code[pos + 3], code[pos + 2], code[pos + 4]);
		}
		break;
	case op_jmp2: {
			const int16_t offset = READ_LE_UINT16(code + pos + 5);
			fprintf(out, "op_jmp2 '%c' %d (%04X) neg %d zero %d", code[pos + 3], offset, pos + offset, code[pos + 2], code[pos + 4]);
		}
		break;
	case op_call2: {
			operand(code, pos + 2, opR);
			fprintf(out, "op_call2 %s", opR);
		}
		break;
	default: {
			fprintf(out, "op.%c_%02X", b ? 'b' : 'w', opcode);
		}
		break;
	}
	fprintf(out, "   # (%d)", size);
	for (int i = 0; i < size; ++i) fprintf(out, " %02X", code[pos + 1 + i]);
	fprintf(out, "\n");
}
