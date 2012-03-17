
#ifndef SYMBOLS_H__
#define SYMBOLS_H__

struct Function {
	char type;
	uint32_t addr;
	char name[32];
	char signature[8];
};

struct Symbols {
	int _functionsCount;
	Function *_functions;

	Symbols();
	void load(const char *path);
};

#endif
