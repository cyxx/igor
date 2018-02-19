
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include "detection.h"
#include "file.h"

static const struct {
	uint32_t crc;
	int version;
	ExecutableType exeType;
	const char *label;
} gameVersions[] = {
	{ 0xa5e2d537, kDemo100, kOverlayExe, "English Shareware v1.00" },
	{ 0x43e886f0, kDemo110, kOverlayExe, "English Shareware v1.10" },
	{ 0xabc9e6f8, kEngFloppy, kOverlayExe, "English Floppy" },
	{ 0x910f1421, kEngFloppy, kOverlayExe, "English Floppy" },
	{ 0x8edcfa28, kSpaFloppy, kOverlayExe, "Spanish Floppy" },
	{ 0xee79aa7d, kSpaFloppy, kOverlayExe, "Spanish Floppy" },
	{ 0x1076d68c, kSpaCd, kSegmentExe, "Spanish CDROM" },
	{ 0, 0, kUnknownExe, 0 }
};

int detectGameVersion(const char *path) {
	File f;
	if (f.open("IGOR.EXE", path)) {
		uint8_t buf[4096];
		uint32_t crc = crc32(0, Z_NULL, 0);
		int count = 0;
		while ((count = f.read(buf, sizeof(buf))) > 0) {
			crc = crc32(crc, buf, count);
		}
		for (int i = 0; gameVersions[i].crc != 0; ++i) {
			if (gameVersions[i].crc == crc) {
				fprintf(stdout, "Found '%s' version\n", gameVersions[i].label);
				return gameVersions[i].version;
			}
		}
		fprintf(stdout, "Unknown version CRC32 0x%08x\n", crc);
	}
	return -1;
}

ExecutableType getExecutableType(int version) {
	for (int i = 0; gameVersions[i].crc != 0; ++i) {
		if (gameVersions[i].version == version) {
			return gameVersions[i].exeType;
		}
	}
	return kUnknownExe;
}
