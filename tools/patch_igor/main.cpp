
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION 1

static const char *_name = "patch001.bin";

#define MAX_TEXTS 256

struct Text {
	int seg;
	int ptr;
	int size;
	int offset;
	int count1;
	int count2;

	void clear() {
		count1 = count2 = 0;
	}
	bool compare(const Text &other) const {
		return count1 == other.count1 && count2 == other.count2;
	}
};

static Text _texts[2][MAX_TEXTS];
static int _textsCount[2];
static int _textsMap[MAX_TEXTS];

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

static void outputFile(FILE *out, FILE *in, int size) {
	uint8_t buf[4096];
	int r;
	while ((r = fread(buf, 1, size > sizeof(buf) ? sizeof(buf) : size, in)) > 0) {
		const int count = fwrite(buf, 1, r, out);
		if (count != r) {
			fprintf(stderr, "i/o error, written %d bytes out of %d\n", count, r);
			exit(1);
		}
		size -= r;
		if (size == 0) {
			break;
		}
	}
}

static int findText(const Text *t, const Text *texts, int textsCount) {
	for (int i = 0; i < textsCount; ++i) {
// fprintf(stdout, "%d,%d - %d,%d\n", t->count1, t->count2, texts[i].count1, texts[i].count2);
		if (t->compare(texts[i])) {
			return i;
		}
	}
	return -1;
}

static int parseText(const char *str, Text *t) {
	int ptr2 = 0;
	if (sscanf(str, "cseg%03d:%04X-%04X %08X (%3d, %3d)", &t->seg, &t->ptr, &ptr2, &t->offset, &t->count1, &t->count2) == 6) {
		t->size = ptr2 - t->ptr;
		return 1;
	}
	return 0;
}

// version : LE32
// num : LE32
// foreach {
//   cseg:ptr // original offset in spa_cd version
//   offset : LE32
//   size : LE32
// }

static const char *_strings[] = {
	"strings_data_sp_cdrom.txt",
	"strings_data_en.txt",
	0
};

static const struct {
	int seg;
	int ptr;
	int offset;
	int size;
} _patches[] = {
	{ 219, 0x10E0, 0x82331D + 0xFB8, 28028 }, // main texts
	{ 0, 0, 0, 0 }
};

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		const char *binPath = argv[1];
		for (int i = 0; _strings[i]; ++i) {
			FILE *in = fopen(_strings[i], "r");
			if (in) {
				int count = 0;
				char buf[256];
				while (fgets(buf, sizeof(buf), in)) {
					if (parseText(buf, &_texts[i][count])) {
						++count;
					}
				}
				fclose(in);
				_textsCount[i] = count;
			}
			fprintf(stdout, "texts %d (%s)\n", _textsCount[i], _strings[i]);
		}
		assert(_textsCount[0] == _textsCount[1]);
		for (int i = 0; i < _textsCount[0]; ++i) {
			const int j = findText(&_texts[0][i], _texts[1], _textsCount[1]);
			fprintf(stdout, "spa[%d] -> eng[%d]\n", i, j);
			_textsMap[i] = j;
			if (j != -1) {
				_texts[1][j].clear();
			}
		}
		for (int i = 0; i < _textsCount[0]; ++i) {
			if (_textsMap[i] == -1) {
				for (int j = 0; j < _textsCount[1]; ++j) {
					if (_texts[1][j].count1 != 0 || _texts[1][j].count2 != 0) {
						_textsMap[i] = j;
						_texts[1][j].clear();
						break;
					}
				}
			}
		}
		for (int i = 0; i < _textsCount[0]; ++i) {
			fprintf(stdout, "spa[%d] -> eng[%d]\n", i, _textsMap[i]);
		}

		FILE *in = fopen(binPath, "rb");
		if (in) {
			FILE *out = fopen(_name, "wb");
			if (out) {
				int offset = 0;
				const int count = _textsCount[0];
				int count2 = 0;
				for (int i = 0; _patches[i].size != 0; ++i) {
					++count2;
				}
				outputLE32(out, VERSION);
				outputLE32(out, count + count2);
				offset += 8;
				for (int i = 0; i < count; ++i) {
					outputLE16(out, _texts[0][i].seg);
					outputLE16(out, _texts[0][i].ptr);
					outputLE32(out, offset);
					outputLE32(out, _texts[0][i].size);
					offset += _texts[0][i].size;
				}
				for (int i = 0; _patches[i].size != 0; ++i) {
					outputLE16(out, _patches[i].seg);
					outputLE16(out, _patches[i].ptr);
					outputLE32(out, offset);
					outputLE32(out, _patches[i].size);
					offset += _patches[i].size;
				}
				for (int i = 0; i < count; ++i) {
					const int j = _textsMap[i];
					assert(j != -1);
					fseek(in, _texts[1][j].offset, SEEK_SET);
					outputFile(out, in, _texts[1][j].size);
				}
				for (int i = 0; _patches[i].size != 0; ++i) {
					fseek(in, _patches[i].offset, SEEK_SET);
					outputFile(out, in, _patches[i].size);
				}
				fclose(out);
			}
			fclose(in);
		}
	}
	return 0;
}
