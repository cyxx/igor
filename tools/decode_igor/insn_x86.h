
#ifndef INSN_X86_H__
#define INSN_X86_H__

#include <stdint.h>

struct Insn_x86 {
	const uint8_t *_code;
	int _pos;
	int _opcode;

	int _mod;
	int _reg;
	int _rm;
	int _imm;
	int _seg;

	int _call_addr;

	void decode(const uint8_t *code);
	void mod_rm();
	const char *reg8(int reg);
	const char *operand(const char *arg = 0);
	int imm(const char *arg);
	int op_grp1(char *buf, const char *arg1, const char *arg2);
	int op_grp2(char *buf, const char *arg1, const char *arg2);
	int op_grp3b(char *buf, const char *arg);
	int op_grp4(char *buf, const char *arg);
	int op_grp5(char *buf, const char *arg);
	int dump(char *buf);
};

#endif
