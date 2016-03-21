/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "file.h"
#include "util.h"
#include "segment_exe.h"

SegmentExecutable::SegmentExecutable() {
	_segmentsCount = 0;
	_segments = 0;
}

SegmentExecutable::~SegmentExecutable() {
	_f.close();
	_segmentsCount = 0;
	free(_segments);
	_segments = 0;
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
	fprintf(stdout, "_segmentsCount %d\n", _segmentsCount);
	const int segmentTableOffset = READ_LE_UINT16(buf + 0x22);
	_f.seek(segmentedExeHeaderOffset + segmentTableOffset);
	_segments = (OverlaySegment *)calloc(_segmentsCount, sizeof(OverlaySegment));
	for (int i = 0; i < _segmentsCount; ++i) {
		_segments[i].offset = _f.readUint16LE() * 256;
		_segments[i].dataSize = _f.readUint16LE();
		_segments[i].flags = _f.readUint16LE(); // &15;0==CODE,1==DATA
		_segments[i].allocSize = _f.readUint16LE();
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
	fprintf(stdout, "readSegment num %d dataSize %d allocSize %d\n", num, _segments[num].dataSize, _segments[num].allocSize);
	return _segments[num].dataSize;
}
