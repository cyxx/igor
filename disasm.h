/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef DISASM_H__
#define DISASM_H__

#include <stdint.h>
#include <stdio.h>

void disasmCode(const uint8_t *code, int pos, FILE *out);

#endif
