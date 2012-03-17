
#ifndef OPCODES_H__
#define OPCODES_H__

#include <stdint.h>

enum {
	op_nop   = 0,
	op_jmp,
	op_call,
	op_ret,
	op_push,
	op_pop,
	op_add,
	op_adc,
	op_sub,
	op_sbb,
	op_mov,
	op_or,
	op_xor,
	op_shl,
	op_shr,
	op_mul,
	op_div,
	op_cmp,
	op_test,
	op_inc, // unused
	op_dec, // unused
	op_les,
	op_lds,
	op_lea,
	op_not,
	op_neg,
	op_cbw,
	op_cwd,
	op_swap,
	op_str,
	op_wait,
	op_out,
	op_int,
	op_jmp2,
	op_call2,
	op_mul2,
	op_div2,
	op_wait2,

	op_count
};

#define t_seg  (1 << 0)
#define t_reg  (1 << 1)
#define t_imm  (1 << 2)
#define t_reg2 (1 << 3)

/*
	insn
		uint8: size
		[0] : opcode (1 << 7) -> .w / .b
		[1] : operand mask
		[...] { // seg / reg / imm
			seg {
				uint8 num
			}
			reg {
				uint8 num
				uint8 type : l,h,w
			}
			imm {
				uint16 value (LE)
			}
		}
*/

struct Instruction {
	int seg;
	int ptr;
	uint8_t code[16];
	int size;
	int addr;
};

#endif
