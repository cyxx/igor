
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "symbols.h"

Symbols::Symbols()
	: _functionsCount(0), _functions(0) {
}

void Symbols::load(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		return;
	}
	char buf[512];
	while (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;
		while (*p == ' ') ++p;
		char type = *p++;
		while (*p == ' ') ++p;
		const int seg = strtol(p, &p, 0);
		while (*p == ' ') ++p;
		const int ptr = strtol(p, &p, 0);
		while (*p == ' ') ++p;
		const char *name = p;
		const char *sig = 0;
		p = strchr(p, ' ');
		if (p) {
			*p++ = 0;
			while (*p == ' ') ++p;
			sig = p;
		}
		++_functionsCount;
		_functions = (Function *)realloc(_functions, _functionsCount * sizeof(Function));
		if (_functions) {
			Function *f = &_functions[_functionsCount - 1];
			f->type = type;
			f->addr = (seg << 16) | ptr;
			strcpy(f->name, name);
			if (sig && *sig == '(') {
				char *p = f->signature;
				for (int i = 1; sig[i] != ')'; ++i) {
					*p++ = sig[i];
				}
				*p = 0;
				fprintf(stdout, "adding symbol '%s' (%s) cseg%03d:%04X\n", f->name, f->signature, seg, ptr);
			} else {
				fprintf(stderr, "no signature for symbol '%s'\n", f->name);
				f->signature[0] = 0;
				exit(1);
			}
		}
	}
	fclose(fp);
	// TODO: qsort
}
