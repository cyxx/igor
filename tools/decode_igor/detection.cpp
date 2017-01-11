
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include "detection.h"
#include "file.h"

static const struct {
	uint32_t crc;
	int version;
	int exeType;
	const char *label;
} gameVersions[] = {
	{ 0x3f35ad61, kDemo100, kOverlayExe, "English Shareware v1.00" },
	{ 0xf96e56f1, kDemo110, kOverlayExe, "English Shareware v1.10" },
	{ 0xaa10bf7c, kEngFloppy, kOverlayExe, "English Floppy" },
	{ 0x4c4d83e5, kSpaCd, kSegmentExe, "Spanish CDROM" },
	{ 0, 0, 0 }
};

int detectGameVersion(const char *path) {
	File f;
	if (f.open("IGOR.EXE", path)) {
		uint8_t buf[4096];
		uint32_t crc = crc32(0, Z_NULL, 0);
		int count = 0;
		while ((count = f.read(buf, sizeof(buf))) > 0) {
			crc = crc32(crc, buf, sizeof(buf));
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
