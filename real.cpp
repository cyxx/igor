/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "real.h"
#include "script.h"
#include "util.h"

static double decodeReal(uint16_t a, uint16_t b, uint16_t c) {
	if ((a & 0xFF) == 0) {
		return 0.;
	}
	const int sign = ((c & 0x8000) != 0) ? -1 : 1;
	const int mbits[] = { (c >> 8) & 0x7F, c & 0xFF, b >> 8, b & 0xFF, a >> 8 };
	double m = 1.;
	m += mbits[0] / 128.;
	m += mbits[1] / 32768.;
	m += mbits[2] / 8388608.;
	m += mbits[3] / 2147483648.;
	m += mbits[4] / 549755813888.;
	const int e = (a & 0xFF) - 129;
	return sign * ldexp(m, e);
}

// the code assumes sizeof(float) == sizeof(uint32_t)

static float readReal(uint16_t a, uint16_t b, uint16_t c) {
	if (a != 0x7F) {
		const float f = decodeReal(a, b, c);
		return f;
	}
	const uint8_t p[] = { uint8_t(b & 255), uint8_t(b >> 8), uint8_t(c & 255), uint8_t(c >> 8) };
	float f;
	memcpy(&f, p, sizeof(uint32_t));
	return f;
}

static void writeReal(float f, uint8_t *p) {
	WRITE_LE_UINT16(p, 0x7F); // lower bits are never set, use it as marker for 'float converted'
	memcpy(p + 2, &f, sizeof(uint32_t));
}

static void writeReal(float f, uint16_t &a, uint16_t &b, uint16_t &c) {
	uint8_t tmp[4];
	memcpy(tmp, &f, sizeof(uint32_t));
	a = 0x7F;
	b = READ_LE_UINT16(tmp);
	c = READ_LE_UINT16(tmp + 2);
}

void realOp1(char op, Reg *r) {
	float f = readReal(r[reg_ax].val.w, r[reg_bx].val.w, r[reg_dx].val.w);
	switch (op) {
	case 'r':
		f = round(f);
		break;
	case 't':
		f = trunc(f);
		break;
	}
	const int t = f;
	r[reg_dx].val.w = t >> 16;
	r[reg_ax].val.w = t & 0xFFFF;
}

void realOp2(char op, Reg *r) {
	float f1 = readReal(r[reg_ax].val.w, r[reg_bx].val.w, r[reg_dx].val.w);
	float f2 = readReal(r[reg_cx].val.w, r[reg_si].val.w, r[reg_di].val.w);
	switch (op) {
	case '+':
		f1 += f2;
		break;
	case '/':
		if (f2 == 0) {
			snprintf(g_err, sizeof__g_err, "realOp2 div opR is 0.\n");
			exit(1);
		}
		f1 /= f2;
		break;
	}
	writeReal(f1, r[reg_ax].val.w, r[reg_bx].val.w, r[reg_dx].val.w);
}

int realCmp(Reg *r) {
	float f1 = readReal(r[reg_ax].val.w, r[reg_bx].val.w, r[reg_dx].val.w);
	float f2 = readReal(r[reg_cx].val.w, r[reg_si].val.w, r[reg_di].val.w);
	if (f1 == f2) {
		return k_zf;
	} else if (f1 < f2) {
		return k_jb;
	} else {
		return 0;
	}
}

void realInt(Reg *r) {
	const int i = (r[reg_dx].val.w << 16) | r[reg_ax].val.w;
	writeReal((float)i, r[reg_ax].val.w, r[reg_bx].val.w, r[reg_dx].val.w);
}

void convertRealToFloat(uint8_t *p, int count) {
	for (int i = 0; i < count; ++i) {
		const uint16_t r[] = { READ_LE_UINT16(p), READ_LE_UINT16(p + 2), READ_LE_UINT16(p + 4) };
		const float f = decodeReal(r[0], r[1], r[2]);
		writeReal(f, p);
		p += 6;
	}
}
