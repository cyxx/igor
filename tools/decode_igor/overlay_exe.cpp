
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
		stub.prevstub = READ_LE_UINT16(hdr + 14);
		return stub.size != 0;
	}
	return false;
}

void OverlayExecutable::parse() {
	uint8_t buf[0x1C];
	_exe.read(buf, sizeof(buf));
	const int paragraphs = READ_LE_UINT16(buf + 8);
	_startIP = READ_LE_UINT16(buf + 0x14);
	_startCS = READ_LE_UINT16(buf + 0x16);
	const int relocationSize = READ_LE_UINT16(buf + 6);
	const int relocationTableOffset = READ_LE_UINT16(buf + 0x18);
	_exe.seek(relocationTableOffset);
	for (int i = 0; i < relocationSize; ++i) {
		const int ptr = _exe.readUint16LE();
		const int seg = _exe.readUint16LE();
//		fprintf(stdout, "relocation 0x%04X:0x%04X\n", seg, ptr);
	}
	assert(paragraphs * 16 == _exe.tell());
	PascalStub stub;
	_exe.seek(0x19F0);
	_stubsCount = 0;
	while (!_exe.ioErr()) {
		uint8_t hdr[32];
		_exe.read(hdr, sizeof(hdr));
		if (isPascalStub(hdr, stub)) {
			fprintf(stdout, "stub %d offset 0x%X size %d (rel %d) entries %d (overlay offset 0x%x)\n", _stubsCount, stub.offset, stub.size, stub.relsize, stub.count, _exe.tell());
			assert(_stubsCount < MAX_PASCAL_STUBS);
			_stubs[_stubsCount] = stub;
			++_stubsCount;
			const int jmpSize = ((stub.count * 5) + 15) & ~15;
			_exe.seek(jmpSize, SEEK_CUR);
		}
	}
	_exeOffset = _exe.tell();
}

int OverlayExecutable::readSegment(int num, uint8_t *data) {
	if (num == 0) {
		const int segSize = _exe.size() - _exeOffset;
		_exe.seek(_exeOffset);
		_exe.read(data, segSize);
		return segSize;
	}
	--num;
	assert(num < _stubsCount);
	_dat.seek(_stubs[num].offset);
	_dat.read(data, _stubs[num].size);
	return _stubs[num].size;
}
