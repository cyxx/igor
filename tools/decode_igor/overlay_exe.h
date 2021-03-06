
#ifndef OVERLAY_EXE_H__
#define OVERLAY_EXE_H__

#include "file.h"

#define MAX_PASCAL_STUBS 512

struct PascalStub {
	uint32_t offset;
	uint16_t size, relsize;
	uint16_t count;
	uint16_t prevstub;
};

struct OverlayExecutable {
	int _startIP, _startCS;
	File _exe;
	File _dat;
	PascalStub _stubs[MAX_PASCAL_STUBS];
	int _stubsCount;
	int _exeOffset;

	void parse();
	int readSegment(int num, uint8_t *data);
};

#endif
