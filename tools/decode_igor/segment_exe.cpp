
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "file.h"
#include "util.h"
#include "segment_exe.h"

SegmentExecutable::SegmentExecutable(const char *fileName, const char *dataPath) {
	_f.open(fileName, dataPath);
	_segmentsCount = 0;
	_segments = 0;
}

SegmentExecutable::~SegmentExecutable() {
	for (int i = 0; i < _segmentsCount; ++i) {
		free(_segments[i]._relocations);
	}
	_segmentsCount = 0;
	free(_segments);
	_segments = 0;
}

static int compareRelocationEntry(const void *a, const void *b) {
	return ((RelocationEntry *)a)->offset - ((RelocationEntry *)b)->offset;
}

void SegmentExecutable::parseSegmentsInfo() {
	_f.seek(0x18);
	int offset1 = _f.readUint16LE();
	assert(offset1 == 0x40);
	_f.seek(0x3C);
	int segmentedExeHeaderOffset = _f.readUint16LE();
	_f.seek(segmentedExeHeaderOffset);
	
	// segmented exe header
	uint8_t buf[64];
	_f.read(buf, 64);
	assert(memcmp(buf, "NE", 2) == 0);
	_startIP = READ_LE_UINT16(buf + 0x14);
	_startCS = READ_LE_UINT16(buf + 0x16);
	_segmentsCount = READ_LE_UINT16(buf + 0x1C);
	const int segmentTableOffset = READ_LE_UINT16(buf + 0x22);
	_f.seek(segmentedExeHeaderOffset + segmentTableOffset);
	_segments = (OverlaySegment *)calloc(_segmentsCount, sizeof(OverlaySegment));
	for (int i = 0; i < _segmentsCount; ++i) {
		_segments[i].offset = _f.readUint16LE() * 256;
		_segments[i].dataSize = _f.readUint16LE();
		_segments[i].flags = _f.readUint16LE(); // &15;0==CODE,1==DATA
		_segments[i].allocSize = _f.readUint16LE();
	}
	for (int i = 0; i < _segmentsCount; ++i) {
		if (_segments[i].flags & 0x100) { // relocation records
			_f.seek(_segments[i].offset);
			const int num = _f.readUint16LE();
			assert(num == i + 1);
			_f.seek(_segments[i].offset + _segments[i].dataSize);
			_segments[i]._relocationsCount = _f.readUint16LE();
			_segments[i]._relocations = (RelocationEntry *)calloc(_segments[i]._relocationsCount, sizeof(RelocationEntry));
			parseSegmentRelocationInfo(&_segments[i]);
			qsort(_segments[i]._relocations, _segments[i]._relocationsCount, sizeof(RelocationEntry), compareRelocationEntry);
		}
	}
}

void SegmentExecutable::parseSegmentRelocationInfo(OverlaySegment *segment) {
	for (int r = 0; r < segment->_relocationsCount; ++r) {
		int sourceType = _f.readByte();
		int flags = _f.readByte();
		int offset = _f.readUint16LE();
		segment->_relocations[r].offset = offset;
		switch (flags & 15) {
		case 0:
			segment->_relocations[r].type = kRelocationInternalRef;
			segment->_relocations[r].data.ref.segment = _f.readByte();
			_f.readByte();
			segment->_relocations[r].data.ref.offset = _f.readUint16LE();
			break;
		case 1:
			segment->_relocations[r].type = kRelocationImportOrdinal;
			segment->_relocations[r].data.ordinal.index = _f.readUint16LE();
			segment->_relocations[r].data.ordinal.num = _f.readUint16LE();
			break;
		default:
			assert(0);
			break;
		}
	}
}

const OverlaySegment *SegmentExecutable::getSegmentInfo(int num) const {
	--num;
	assert(num >= 0 && num < _segmentsCount);
	return &_segments[num];
}

int SegmentExecutable::readSegment(int num, uint8_t *data) {
	--num;
	assert(num >= 0 && num < _segmentsCount);
	_f.seek(_segments[num].offset);
	_f.read(data, _segments[num].dataSize);
//	fprintf(stdout, "readSegment num %d dataSize %d allocSize %d\n", num, _segments[num].dataSize, _segments[num].allocSize);
	return _segments[num].dataSize;
}

const RelocationEntry *SegmentExecutable::findRelocationInfo(int num, int addr) const {
	--num;
	assert(num >= 0 && num < _segmentsCount);
	RelocationEntry entry;
	entry.offset = addr;
	return (const RelocationEntry *)bsearch(&entry, _segments[num]._relocations, _segments[num]._relocationsCount, sizeof(RelocationEntry), compareRelocationEntry);
}

