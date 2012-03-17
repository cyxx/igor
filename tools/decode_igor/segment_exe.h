
#ifndef SEGMENT_H__
#define SEGMENT_H__

#include "file.h"

enum RelocationEntryType {
	kRelocationInternalRef,
	kRelocationImportOrdinal
};

struct RelocationEntry {
	RelocationEntryType type;
	uint32_t offset;
	union {
		struct {
			uint8_t segment;
			uint16_t offset;
		} ref;
		struct {
			uint8_t index;
			uint8_t num;
		} ordinal;
	} data;
};

struct OverlaySegment {
	uint32_t offset;
	uint16_t dataSize, allocSize;
	uint16_t flags;
	int _relocationsCount;
	RelocationEntry *_relocations;
};

struct SegmentExecutable {
	File _f;
	int _segmentsCount;
	OverlaySegment *_segments;
	uint16_t _startCS, _startIP;

	SegmentExecutable(const char *fileName, const char *dataPath);
	~SegmentExecutable();

	void parseSegmentsInfo();
	void parseSegmentRelocationInfo(OverlaySegment *segment);
	const OverlaySegment *getSegmentInfo(int num) const;
	int readSegment(int num, uint8_t *data);
	const RelocationEntry *findRelocationInfo(int segment, int addr) const;
};

#endif

