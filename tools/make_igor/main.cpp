
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define VERSION 1

#define MAX_PARTS 100

static const char *_name = "igor.bin";

static const bool _compressFileData = false;

static uint32_t _dsegOffset;
static uint32_t _mainOffset;
static uint32_t _partOffset[MAX_PARTS * 3];
static uint32_t _partSize[MAX_PARTS * 3];

static void outputByte(FILE *out, uint8_t i) {
	fputc(i, out);
}

static void outputLE16(FILE *out, uint16_t i) {
	fputc(i & 255, out);
	fputc(i >> 8, out);
}

static void outputLE32(FILE *out, uint32_t i) {
	outputLE16(out, i & 0xFFFF);
	outputLE16(out, i >> 16);
}

static uint32_t outputFile(FILE *out, const char *path, const char *name) {
	char filePath[512];
	snprintf(filePath, sizeof(filePath), "%s/%s", path, name);
	FILE *in = fopen(filePath, "rb");
	if (!in) {
		fprintf(stderr, "unable to open '%s'\n", filePath);
		return 0;
	}
	uint32_t size = 0;
	uint8_t buf[4096], buf2[4096];
	int r;
	if (_compressFileData) {
		z_stream strm;
		memset(&strm, 0, sizeof(strm));
		r = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
		if (r != Z_OK) {
			fprintf(stderr, "deflateInit failed, r %d\n", r);
			exit(1);
		}
		do {
			if ((r = fread(buf, 1, sizeof(buf), in)) < 0) {
				fprintf(stderr, "i/o error, read %d\n", r);
				exit(1);
			}
			strm.next_in = buf;
			strm.avail_in = r;
			const int f = feof(in) ? Z_FINISH : Z_NO_FLUSH;
			do {
				strm.next_out = buf2;
				strm.avail_out = sizeof(buf2);
				r = deflate(&strm, f);
				if (r == Z_STREAM_ERROR) {
					fprintf(stderr, "deflate failed, r %d\n", r);
					exit(1);
				}
				r = sizeof(buf2) - strm.avail_out;
				const int count = fwrite(buf2, 1, r, out);
				if (count != r) {
					fprintf(stderr, "i/o error, written %d bytes out of %d\n", count, r);
					exit(1);
				}
				size += count;
			} while (strm.avail_out == 0);
		} while (!feof(in));
		r = deflateEnd(&strm);
		if (r != Z_OK) {
			fprintf(stderr, "deflateEnd failed, r %d\n", r);
			exit(1);
		}
	} else {
		while ((r = fread(buf, 1, sizeof(buf), in)) > 0) {
			const int count = fwrite(buf, 1, r, out);
			if (count != r) {
				fprintf(stderr, "i/o error, written %d bytes out of %d\n", count, r);
				exit(1);
			}
			size += count;
		}
	}
	fclose(in);
	return size;
}

// LE32 : version
// LE32 : main offset
// for (1..100) {
//   LE32 : anim %03d offset
//   LE32 : code %03d offset
//   LE32 : room %03d offset
// }

int main(int argc, char *argv[]) {
	if (argc >= 3) {
		const char *dsegPath = argv[1];
		const char *binPath = argv[2];
		FILE *out = fopen(_name, "wb");
		if (out) {
			uint32_t size, offset = 0;
			outputLE32(out, VERSION);
			offset += 4;
			outputLE32(out, 0); // main
			offset += 4;
			for (int i = 1; i < MAX_PARTS; ++i) {
				for (int j = 0; j < 3; ++j) {
					outputLE32(out, 0);
					offset += 4;
				}
			}
			_mainOffset = offset;
			size = outputFile(out, binPath, "main.bin");
			if (size == 0) {
				fprintf(stderr, "unable to open 'main.bin'\n");
				exit(1);
			}
			offset += size;
			for (int i = 1; i < MAX_PARTS; ++i) {
				static const char *namesFmt[] = { "room_%03d.bin", "anim_%03d.bin", "code_%03d.bin" };
				for (int j = 0; j < 3; ++j) {
					char name[32];
					snprintf(name, sizeof(name), namesFmt[j], i);
					size = outputFile(out, binPath, name);
					if (size == 0) {
						_partOffset[i * 3 + j] = 0xFFFFFFFF;
						_partSize[i * 3 + j] = 0;
					} else {
						_partOffset[i * 3 + j] = offset;
						_partSize[i * 3 + j] = size;
					}
					offset += size;
				}
			}
			fseek(out, 4, SEEK_SET);
			outputLE32(out, _mainOffset);
			for (int i = 1; i < MAX_PARTS; ++i) {
				for (int j = 0; j < 3; ++j) {
					outputLE32(out, _partOffset[i * 3 + j]);
				}
			}
			fclose(out);
		}
	}
	return 0;
}
