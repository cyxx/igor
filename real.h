/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef REAL_H__
#define REAL_H__

#include <stdint.h>

struct Reg;

void realOp1(char op, Reg *r);
void realOp2(char op, Reg *r);
int realCmp(Reg *r);
void realInt(Reg *r);

void convertRealToFloat(uint8_t *p, int count);

#endif
