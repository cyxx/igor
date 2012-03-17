/*
 * Igor: Objective Uikokahonia engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SEGMENT_EXE_H__
#define SEGMENT_EXE_H__

#include "file.h"

struct OverlaySegment {
	uint32_t offset;
	uint16_t dataSize, allocSize;
	uint16_t flags;
};

struct SegmentExecutable {
	File _f;
	int _segmentsCount;
	OverlaySegment *_segments;
	uint16_t _startCS, _startIP;

	SegmentExecutable();
	~SegmentExecutable();

	void parseSegmentsInfo();
	const OverlaySegment *getSegmentInfo(int num) const;
	int readSegment(int num, uint8_t *data);
};

#endif

