
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION 1

static const char *_name = "txt_en.bin";

// version : LE32
// struct TextBlock { count: LE16, size: LE16, offset: LE32 }
// TextBlock main[4]
// struct TextRoom { cseg: LE16, ptr: LE16, TextBlock[2] }
// uint16_t count : LE16
// TextRoom rooms[count]
// data[] : byte

static const int _mainTextsOffsetEN = 0x82331D + 0xFB8; /* size == 28028 */

#define MAX_TEXTS 100
#define TEXT_BUF_SIZE 16

static const int ABS(int x) { return x < 0 ? -x : x; }

struct Text {
	int seg;
	int ptr;
	int size;
	int offset;
	int count1;
	int count2;
	char buf[TEXT_BUF_SIZE];

	void clear() {
		count1 = count2 = 0;
	}
	bool compare(const Text &other) const {
		return count1 == other.count1 && count2 == other.count2;
	}
	bool compareOneOff(const Text &other) const {
		return ABS(count1 - other.count1) <= 1 || ABS(count2 - other.count2) <= 1;
	}
};

struct TextAddr {
	int seg;
	int ptr;
	int seg2;
	int ptr2;
};

struct TextBlock {
	int count;
	int size;
	int offset;
};

static Text _texts[2][MAX_TEXTS];
static int _textsCount[2];
static int _textsMap[MAX_TEXTS];
static TextBlock _mainTextBlocks[4];
static TextAddr _roomTextAddr[MAX_TEXTS];
static TextBlock _roomTextBlocks[MAX_TEXTS][2];

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

static void outputTextChar(const char chr, FILE *out) {
	if (isprint(chr)) {
		fprintf(out, "%c", chr);
	} else {
		fprintf(out, "\\x%02X", (uint8_t)chr);
	}
}

static char decodeRoomTextChar(uint8_t code) {
	if ((code >= 0xAE && code <= 0xC7) || (code >= 0xCE && code <= 0xE7)) {
		code -= 0x6D;
	} else if (code == 0xE8) {
		code = 0xA0;
	} else if (code == 0xE9) {
		code = 0x82;
	} else if (code >= 0xEA && code <= 0xEE) {
		code = 0xA1 + code - 0xEA;
	}
	return code;
}

static void decodeRoomText(FILE *in, const Text *t, FILE *out) {
	fseek(in, t->offset, SEEK_SET);
	int countObjects = t->count1;
	int countDialogues = t->count2;
	if (countObjects != 0 && countDialogues == 0) { // no object names
		countDialogues = countObjects;
		countObjects = -1;
	}
	fprintf(out, "%d\n", countObjects);
	if (countObjects != -1) {
		int code = fgetc(in);
		int index = -1;
		while (code != 0xF6) {
			if (code == 0xF4) {
				++index;
			}
			const int len = fgetc(in);
			if (len != 0) {
				fprintf(out, "O #%03d %03d ", index, len);
				for (int i = 0; i < len; ++i) {
					const char chr = decodeRoomTextChar(fgetc(in));
					outputTextChar(chr, out);
				}
				fprintf(out, "\n");
			}
			code = fgetc(in);
		}
	}
	fprintf(out, "%d\n", countDialogues);
	if (countDialogues != 0) {
		int code = fgetc(in);
		int index = 200;
		while (code != 0xF6) {
			if (code == 0xF4) {
				++index;
			}
			const int len = fgetc(in);
			if (len != 0) {
				fprintf(out, "D #%03d %03d ", index, len);
				for (int i = 0; i < len; ++i) {
					const char chr = decodeRoomTextChar(fgetc(in));
					outputTextChar(chr, out);
				}
				fprintf(out, "\n");
			}
			code = fgetc(in);
		}
	}
}

static char decodeMainTextChar(FILE *in) {
	const int code = fgetc(in);
	return code - 0x6D;
}

static void decodeMainText(FILE *in, FILE *out) {
	static const struct {
		int srcOffset;
		int count;
		int stringLen;
		const char *prefix;
	} offsets[] = {
		{      0,   3,  7 * 2, "P" },
		{   0x2A,  35, 31 * 2, "O" },
		{  0x8BA, 250, 51 * 2, "D" },
		{ 0x6CA4,   9, 12 * 2, "V" },
		{     -1,   0,      0, 0 }
	};
	for (int j = 0; offsets[j].srcOffset != -1; ++j) {
		fprintf(out, "%d\n", offsets[j].count);
		int offset = _mainTextsOffsetEN + offsets[j].srcOffset;
		for (int i = 0; i < offsets[j].count; ++i, offset += offsets[j].stringLen) {
			fseek(in, offset, SEEK_SET);
			const int len = decodeMainTextChar(in);
			fprintf(out, "%s #%03d %03d ", offsets[j].prefix, i, len);
			for (int i = 0; i < len; ++i) {
				outputTextChar(decodeMainTextChar(in), out);
			}
			fprintf(out, "\n");
		}
	}
}

static int findText(const Text *t, bool (Text::*compareFunc)(const Text &) const) {
	const Text *texts = _texts[1];
	for (int i = 0; i < _textsCount[1]; ++i) {
		if ((t->*compareFunc)(texts[i])) {
			return i;
		}
	}
	return -1;
}

static void dumpTextMap(int src, int dst, char cmp) {
	fprintf(stdout, "spa[%2d] -> eng[%2d] ('%s' -> '%s') %c\n", src, dst, _texts[0][src].buf, _texts[1][dst].buf, cmp);
}

static int parseText(const char *str, Text *t) {
	int ptr2 = 0;
	if (sscanf(str, "cseg%03d:%04X-%04X %08X (%3d, %3d)", &t->seg, &t->ptr, &ptr2, &t->offset, &t->count1, &t->count2) == 6) {
		t->size = ptr2 - t->ptr;
		const char *sep = strchr(str, '\'');
		if (sep) {
			++sep;
			for (int i = 0; i < TEXT_BUF_SIZE - 1; ++i) {
				if (*sep && *sep != '\'') {
					t->buf[i] = *sep++;
				} else {
					t->buf[i] = ' ';
				}
			}
			t->buf[TEXT_BUF_SIZE - 1] = 0;
		}
		return 1;
	}
	return 0;
}

static void parseTextBlock(const char *path, TextBlock *tb) {
	FILE *in = fopen(path, "r");
	if (in) {
		int i = 0;
		int lines = 0;
		int state = 0;
		char buf[256];
		while (fgets(buf, sizeof(buf), in)) {
			switch (state) {
			case 0:
				if (sscanf(buf, "%d", &tb[i].count) == 1) {
					state = 1;
					lines = 0;
					if (tb[i].count <= 0) {
						++i;
						state = 0;
					}
				}
				break;
			case 1: {
					char type;
					int index, len;
					if (sscanf(buf, "%c #%3d %3d", &type, &index, &len) == 3) {
						tb[i].size += len + 2;
						assert(tb[i].size < 0x10000);
						++lines;
						if (lines == tb[i].count) {
							++i;
							state = 0;
						}
					}
				}
				break;
			}
		}
		fclose(in);
	}
}

static int hex(char chr) {
	static const char *kAlphabet = "0123456789ABCDEF";
	const char *p = strchr(kAlphabet, chr);
	if (p) {
		return p - kAlphabet;
	}
	fprintf(stderr, "Invalid hex character '%c'\n", chr);
	exit(-1);
	return -1;
}

static char unescapeChar(const char *seq) {
	if (seq[0] == 'x') {
		const int hi = hex(seq[1]);
		const int lo = hex(seq[2]);
		return hi * 16 + lo;
	}
	fprintf(stderr, "Invalid escape sequence '%c'\n", seq[0]);
	exit(-1);
	return -1;
}

static void outputTextBlock(const char *path, FILE *out) {
	FILE *in = fopen(path, "r");
	if (in) {
		char buf[512];
		while (fgets(buf, sizeof(buf), in)) {
			char type;
			int index;
			if (sscanf(buf, "%c #%3d", &type, &index) == 2) {
				assert(index < 256);
				outputByte(out, index);
				char *p = buf + 11;
				int len = 0;
				while (*p && *p != '\n') {
					if (*p == '\\') {
						p += 3;
					}
					++len;
					++p;
				}
				assert(len < 256);
				outputByte(out, len);
				p = buf + 11;
				while (*p && *p != '\n') {
					char chr = *p;
					if (chr == '\\') {
						chr = unescapeChar(p + 1);
						p += 3;
					}
					outputByte(out, chr);
					++p;
				}
			}
		}
		fclose(in);
	}
}

static const char *kStringTextFiles[] = {
	"strings_data_sp_cdrom.txt",
	"strings_data_en.txt",
	0
};

static const bool _dumpTexts = true;

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		const char *binPath = argv[1];
		for (int i = 0; kStringTextFiles[i]; ++i) {
			FILE *in = fopen(kStringTextFiles[i], "r");
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
			fprintf(stdout, "texts %d (%s)\n", _textsCount[i], kStringTextFiles[i]);
		}
		assert(_textsCount[0] == _textsCount[1]);
		const int roomTextsCount = _textsCount[0];
		if (_dumpTexts) {
			FILE *in = fopen(binPath, "rb");
			if (in) {
				FILE *out = fopen("dump/main.txt", "wb");
				if (out) {
					decodeMainText(in, out);
					fclose(out);
				}
				for (int i = 0; i < _textsCount[1]; ++i) {
					const Text *t = &_texts[1][i];
					char name[64];
					snprintf(name, sizeof(name), "dump/cseg%03d_%04x.txt", t->seg, t->ptr);
					FILE *out = fopen(name, "wb");
					if (out) {
						decodeRoomText(in, t, out);
						fclose(out);
					}
				}
				fclose(in);
			}
		}
		for (int i = 0; i < _textsCount[0]; ++i) {
			int j = findText(&_texts[0][i], &Text::compare);
			_textsMap[i] = j;
			if (j != -1) {
				dumpTextMap(i, j, 'C');
				_texts[1][j].clear();
				continue;
			}
			j = findText(&_texts[0][i], &Text::compareOneOff);
			_textsMap[i] = j;
			if (j != -1) {
				dumpTextMap(i, j, '1');
				_texts[1][j].clear();
				continue;
			}
			fprintf(stderr, "No mapping found for text %d\n", i);
			exit(1);
		}
		parseTextBlock("dump/main.txt", _mainTextBlocks);
		for (int i = 0; i < roomTextsCount; ++i) {
			const int j = _textsMap[i];
			assert(j != -1);
			char name[64];
			snprintf(name, sizeof(name), "dump/cseg%03d_%04x.txt", _texts[1][j].seg, _texts[1][j].ptr);
			parseTextBlock(name, _roomTextBlocks[i]);
			_roomTextAddr[i].seg = _texts[0][j].seg;
			_roomTextAddr[i].ptr = _texts[0][j].ptr;
			_roomTextAddr[i].seg2 = _texts[1][j].seg;
			_roomTextAddr[i].ptr2 = _texts[1][j].ptr;
		}
		FILE *out = fopen(_name, "wb");
		if (out) {
			int offset = 0;
			outputLE32(out, VERSION);
			offset += 4;
			static const int kHeaderSize = 8;
			const int dataOffset = kHeaderSize * 4 + 2 + roomTextsCount * 4 + roomTextsCount * kHeaderSize * 2;
			offset += dataOffset;
			for (int i = 0; i < 4; ++i) {
				outputLE16(out, _mainTextBlocks[i].count);
				outputLE16(out, _mainTextBlocks[i].size);
				outputLE32(out, offset);
				offset += _mainTextBlocks[i].size;
			}
			outputLE16(out, roomTextsCount);
			for (int j = 0; j < roomTextsCount; ++j) {
				outputLE16(out, _roomTextAddr[j].seg);
				outputLE16(out, _roomTextAddr[j].ptr);
				for (int i = 0; i < 2; ++i) {
					int count = _roomTextBlocks[j][i].count;
					if (count < 0) {
						count = 0;
					}
					outputLE16(out, count);
					outputLE16(out, _roomTextBlocks[j][i].size);
					outputLE32(out, offset);
					offset += _roomTextBlocks[j][i].size;
				}
			}
			outputTextBlock("dump/main.txt", out);
			for (int i = 0; i < roomTextsCount; ++i) {
				char name[64];
				snprintf(name, sizeof(name), "dump/cseg%03d_%04x.txt", _roomTextAddr[i].seg2, _roomTextAddr[i].ptr2);
				outputTextBlock(name, out);
			}
			fclose(out);
		}
	}
	return 0;
}
