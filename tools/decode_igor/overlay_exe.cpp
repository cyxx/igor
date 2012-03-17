
#include <assert.h>
#include <stdint.h>
#include "overlay_exe.h"
#include "util.h"

static bool isPascalStub(const uint8_t *hdr, PascalStub &stub) {
	if (READ_LE_UINT16(hdr) == 0x3FCD && READ_LE_UINT16(hdr + 2) == 0) {
		stub.offset = READ_LE_UINT32(hdr + 4);
		stub.size = READ_LE_UINT16(hdr + 8);
		stub.relsize = READ_LE_UINT16(hdr + 10);
		stub.count = READ_LE_UINT16(hdr + 12);
//		+ 14 // prevstub
		if (stub.offset < 16 * 1024 * 1024 && stub.size != 0) {
			return true;
		}
	}
	return false;
}

void OverlayExecutable::parse() {
	uint8_t buf[0x1C];
	_exe.read(buf, sizeof(buf));
	_startIP = READ_LE_UINT16(buf + 0x14);
	_startCS = READ_LE_UINT16(buf + 0x16);
	const int relocationSize = READ_LE_UINT16(buf + 6);
	const int relocationTableOffset = READ_LE_UINT16(buf + 0x18);
	_exe.seek(relocationTableOffset);
	for (int i = 0; i < relocationSize; ++i) {
		const int ptr = _exe.readUint16LE();
		const int seg = _exe.readUint16LE();
//		printf("segment 0x%X %d\n", seg, ptr);
	}
	PascalStub stub;
	_exe.seek(0x19F0);
	int counter = 0;
	int i = 0;
	while (counter < 16) {
		uint8_t hdr[16];
		_exe.read(hdr, sizeof(hdr));
		if (isPascalStub(hdr, stub)) {
//			fprintf(stdout, "stub %d offset 0x%X size %d\n", i, stub.offset, stub.size);
			assert(i < MAX_PASCAL_STUBS);
			_stubs[i] = stub;
			++i;
			counter = 0;
			continue;
		}
		++counter;
	}
	_stubsCount = i;
}

int OverlayExecutable::readSegment(int num, uint8_t *data) {
	assert(num < _stubsCount);
	_dat.seek(_stubs[num].offset);
	_dat.read(data, _stubs[num].size);
	return _stubs[num].size;
}
