
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "file.h"
#include "insn_x86.h"
#include "overlay_exe.h"
#include "room.h"
#include "segment_exe.h"
extern "C" {
#include "sha1.h"
}
#include "symbols.h"
#include "util.h"
#include "voc.h"

static const bool _dumpCode = true;
static const bool _dumpAssets = false;
static const bool _dumpStrings = false;

static uint8_t _bufSeg[1 << 16];

static void dumpBin(const char *name, const uint8_t *data, int dataSize) {
	File f;
	if (!f.open(name, "dump", "wb")) {
		return;
	}
	f.write(data, dataSize);
}

#define MAX_TRAPS 64

static uint32_t _traps[MAX_TRAPS];
static int _trapsCount;

#define MAX_FUNCS 64

static uint32_t _funcs[MAX_FUNCS];
static int _funcsCount;

#define MAX_CALLS 1024

static uint32_t _calls[MAX_CALLS];
static int _callsCount;

static struct {
	int counter;
	int seg, ptr;
	int num;
} _func_ptr;

static void queueCall(uint32_t addr, uint16_t cs, uint16_t ip) {
	if ((addr >> 16) == 0) {
		ip += (int16_t)addr;
		addr = (cs << 16) | ip;
	}
	for (int i = 0; i < _trapsCount; ++i) {
		if (_traps[i] == addr) {
			return;
		}
	}
	for (int i = 0; i < _funcsCount; ++i) {
		if (_funcs[i] == addr) {
			return;
		}
	}
	if ((addr >> 16) == 230) {
		fprintf(stderr, "pascal runtime function cseg%02d:%04X found\n", addr >> 16, addr & 0xFFFF);
		exit(1);
	}
	for (int i = 0; i < _callsCount; ++i) {
		if (_calls[i] == addr) {
			return;
		}
	}
	assert(_callsCount < MAX_CALLS);
	fprintf(stdout, "queue call cseg%02d:%04X count %d\n", addr >> 16, addr & 0xFFFF, _callsCount);
	_calls[_callsCount++] = addr;
}

static void checkCall(Insn_x86 &i, int cs, int pos) {
	if (i._call_addr != 0) {
		queueCall(i._call_addr, cs, pos);
	} else if (cs != 222) {
		// basic heuristic for actions func ptr
		switch (i._opcode) {
		case 0xBF: // mov di,
			_func_ptr.ptr = READ_LE_UINT16(i._code + 1);
			_func_ptr.counter = 1;
			break;
		case 0xB8: // mov ax,
			if (_func_ptr.counter == 1) {
				_func_ptr.seg = READ_LE_UINT16(i._code + 1);
				if (_func_ptr.seg >= 0 && _func_ptr.seg < 255) {
					++_func_ptr.counter;
				}
			}
			break;
		case 0xA3: // mov _actionsTable.ptr+, ax
			if (_func_ptr.counter == 2) {
				_func_ptr.num = READ_LE_UINT16(i._code + 1);
				if (_func_ptr.num >= 0x593C && _func_ptr.num  < 0x5B24) {
					++_func_ptr.counter;
				}
			}
			break;
		case 0x89: // mov _actionsTable.ptr+, dx
			if (_func_ptr.counter == 3 && i._code[1] == 0x16) {
				if (READ_LE_UINT16(i._code + 2) == _func_ptr.num + 2) {
					fprintf(stdout, "action func_ptr cseg%02d:%04X at cseg%02d:%04X\n", _func_ptr.seg, _func_ptr.ptr, cs, pos);
					queueCall((_func_ptr.seg << 16 | _func_ptr.ptr), cs, pos);
					memset(&_func_ptr, 0, sizeof(_func_ptr));
				}
			}
			break;
		}
	}
}

static void disasmCode(File &f, uint16_t cs, const uint8_t *code, int startOffset, int endOffset) {
	Insn_x86 i;
	int pos = startOffset; 
	while (1) {
		i.decode(code + pos);
		char buf[256];
		snprintf(buf, sizeof(buf), "cseg%03d:%04X  ", cs, pos);
		const int len = i.dump(buf + 14);
		strcat(buf, "\n");
		f.write(buf, strlen(buf));
		pos += i._pos;
		checkCall(i, cs, pos);
		if (endOffset == -1) {
			if (i._opcode == 0xC2 || i._opcode == 0xCB || i._opcode == 0xCA) break; // ret, retf
		} else {
			if (pos > endOffset) break;
		}
	}
}

static void disasmFunc(File &f, uint16_t cs, const uint8_t *code, int startOffset) {
	char buf[256];
	snprintf(buf, sizeof(buf), "# func sub_%03d_%04X\n", cs, startOffset);
	f.write(buf, strlen(buf));
	Insn_x86 i;
	int pos = startOffset;
	do {
		i.decode(code + pos);
		char buf[256];
		snprintf(buf, sizeof(buf), "cseg%03d:%04X  ", cs, pos);
		const int len = i.dump(buf + 14);
		strcat(buf, "\n");
		f.write(buf, strlen(buf));
		pos += i._pos;
		checkCall(i, cs, pos);
	} while (i._opcode != 0xC2 && i._opcode != 0xCB && i._opcode != 0xCA); // ret, retf
	snprintf(buf, sizeof(buf), "# endfunc\n");
	f.write(buf, strlen(buf));
}

static void relocateCode(uint8_t *code, const OverlaySegment *segInfo, int seg) {
	if (seg == 230) {
		fprintf(stdout, "Ignoring segment 230 relocations\n");
		return;
	}
	for (int i = 0; i < segInfo->_relocationsCount; ++i) {
		const RelocationEntry *re = &segInfo->_relocations[i];
		switch (re->type) {
		case kRelocationInternalRef:
			switch (code[re->offset - 1]) {
			case 0x9A: // call
				assert(READ_LE_UINT16(code + re->offset) == 0xFFFF);
				assert(READ_LE_UINT16(code + re->offset + 2) == 0);
				WRITE_LE_UINT32(code + re->offset, re->data.ref.offset | (re->data.ref.segment << 16) );
				break;
			case 0xB8 ... 0xBF: // mov ax, Iv
				assert(READ_LE_UINT16(code + re->offset) == 0xFFFF);
				WRITE_LE_UINT16(code + re->offset, re->data.ref.segment);
				break;
			default:
				fprintf(stderr, "unimplemented kRelocationInternalRef opcode 0x%X offset 0x%X\n", code[re->offset - 1], re->offset);
				fprintf(stderr, "data 0x%04X ref 0x%X seg 0x%X\n", READ_LE_UINT16(code + re->offset), re->data.ref.offset, re->data.ref.segment);
				exit(1);
			}
			break;
		case kRelocationImportOrdinal:
			switch (code[re->offset - 1]) {
			case 0x9A: // call
				assert(READ_LE_UINT16(code + re->offset) == 0xFFFF);
				assert(READ_LE_UINT16(code + re->offset + 2) == 0);
				WRITE_LE_UINT32(code + re->offset, re->data.ordinal.num | ((re->data.ordinal.index + 900) << 16) );
				break;
			default:
				fprintf(stderr, "unimplemented kRelocationImportOrdinal opcode 0x%X offset 0x%X\n", code[re->offset - 1], re->offset);
//				exit(1);
			}
			break;
		}
	}
}

static void dumpCode(SegmentExecutable& seg_exe, int codeSeg, int startOffset, int endOffset) {
	fprintf(stdout, "dump code cseg%02d:%04X\n", codeSeg, startOffset);
	int size = seg_exe.readSegment(codeSeg, _bufSeg);
	relocateCode(_bufSeg, seg_exe.getSegmentInfo(codeSeg), codeSeg);
	char name[32];
	snprintf(name, sizeof(name), "%03d_%04X.asm", codeSeg, startOffset);
	File f;
	if (!f.open(name, "code", "wb")) {
		return;
	}
	_callsCount = 0;
	disasmCode(f, codeSeg, _bufSeg, startOffset, endOffset);
}

static void dumpCodeRec(SegmentExecutable& seg_exe, int codeSeg, int startOffset, int endOffset) {
	fprintf(stdout, "dump code recursive cseg%02d:%04X\n", codeSeg, startOffset);
	int size = seg_exe.readSegment(codeSeg, _bufSeg);
	relocateCode(_bufSeg, seg_exe.getSegmentInfo(codeSeg), codeSeg);
	char name[32];
	snprintf(name, sizeof(name), "%03d_%04X.asm", codeSeg, startOffset);
	File f;
	if (!f.open(name, "code", "wb")) {
		return;
	}
	_callsCount = 0;
	disasmCode(f, codeSeg, _bufSeg, startOffset, endOffset);
	int callsCount, callsOffset = 0;
	do {
		callsCount = _callsCount;
		for (int i = callsOffset; i < callsCount; ++i) {
			const int cs = _calls[i] >> 16;
			if  (cs != codeSeg) {
				size = seg_exe.readSegment(cs, _bufSeg);
				 relocateCode(_bufSeg, seg_exe.getSegmentInfo(cs), cs);
				codeSeg = cs;
			}
			disasmFunc(f, cs, _bufSeg, _calls[i] & 0xFFFF);
		}
		callsOffset = callsCount;
	} while (callsCount != _callsCount);
}

static void dumpMainParts(SegmentExecutable& seg_exe, int codeSeg, int startOffset, int endOffset) {
	fprintf(stdout, "dump main parts cseg%02d:%04X\n", codeSeg, startOffset);
	int size = seg_exe.readSegment(codeSeg, _bufSeg);
	relocateCode(_bufSeg, seg_exe.getSegmentInfo(codeSeg), codeSeg);
	char name[32];
	snprintf(name, sizeof(name), "%03d_%04X.asm", codeSeg, startOffset);
	File f;
	if (!f.open(name, "code", "wb")) {
		return;
	}
	// main loop dispatch
	_callsCount = 0;
	disasmCode(f, codeSeg, _bufSeg, startOffset, endOffset);
	uint32_t parts[256];
	assert(_callsCount < 256);
	memcpy(parts, _calls, _callsCount * sizeof(uint32_t));
	int partsCount = _callsCount;
	// common/main functions
	fprintf(stdout, "funcs count %d\n", _funcsCount);
	memcpy(_calls, _funcs, _funcsCount * sizeof(uint32_t));
	_callsCount = _funcsCount;
	int callsCount, callsOffset = 0;
	do {
		callsCount = _callsCount;
		for (int i = callsOffset; i < callsCount; ++i) {
			const int cs = _calls[i] >> 16;
			if  (cs != codeSeg) {
				size = seg_exe.readSegment(cs, _bufSeg);
				 relocateCode(_bufSeg, seg_exe.getSegmentInfo(cs), cs);
				codeSeg = cs;
			}
			disasmFunc(f, cs, _bufSeg, _calls[i] & 0xFFFF);
		}
		callsOffset = callsCount;
	} while (callsCount != _callsCount);
	// parts
	fprintf(stdout, "parts count %d\n", partsCount);
	for (int i = 0; i < partsCount; ++i) {
		const int cs = parts[i] >> 16;
		dumpCodeRec(seg_exe, parts[i] >> 16, parts[i] & 0xFFFF, -1);
	}
}

static void dumpSound(File& f) {
	static const char *voc = "Creative Voice File";
	uint32_t offsets[4096];
	int count = 0;
	int bufOffset = 0;
	while (!f.ioErr()) {
		_bufSeg[0] = f.readByte();
		if (_bufSeg[0] == voc[0] && f.read(_bufSeg + 1, 18) == 18) {
			if (memcmp(_bufSeg, voc, 19) == 0) {
				assert(count < 4096);
				offsets[count++] = bufOffset;
			}
			bufOffset += 18;
		}
		++bufOffset;
	}
	assert(count < 4096);
	offsets[count] = f.size();
	for (int i = 0; i < count; ++i) {
		f.seek(offsets[i]);
		int sz = offsets[i + 1] - offsets[i];
		char name[16];
		snprintf(name, sizeof(name), "%04d.voc", i);
		fprintf(stdout, "Voice %d/%d size %d\n", i, count, sz);
		File out;
		if (out.open(name, "assets", "wb")) {
			while (sz > 0) {
				const int r = f.read(_bufSeg, sz > sizeof(_bufSeg) ? sizeof(_bufSeg) : sz);
				sz -= r;
				out.write(_bufSeg, r);
			}
			out.close();
			decodeVoc(name, "assets");
		}
	}
}

static void dumpRoom(int num, uint8_t *code, int size) {
	const uint8_t *end = code + size - 2;
	while (end >= code) {
		if (end[0] == 0xC9 && (end[1] == 0xCB || end[1] == 0xCA)) { // leave, retf
			int offset = end + 2 - code;
			if (end[1] == 0xCA) {
				offset += 2;
			}
			char name[16];
			snprintf(name, sizeof(name), "room.%03d", num);
			fprintf(stdout, "Room %d size %d\n", num, size - offset);
			File out;
			if (out.open(name, "dump", "wb")) {
				out.write(code + offset, size - offset);
			}
			decodeRoomData(num, code + offset, size - offset);
			break;
		}
		--end;
	}
}

static void dumpStrings(int seg, uint8_t *buf, int size, int offset) {
	static const uint8_t codes[] = { 0xF4, 0, 0xF5, 0 };
	const uint8_t *end = buf + size;
	const uint8_t *cur = buf;
	while (1) {
		const uint8_t *p = (const uint8_t *)memchr(cur, codes[0], end - cur);
		if (!p) {
			++cur;
			break;
		}
		const uint8_t *start = p;
		const uint8_t *str = 0, *str2 = 0;
		bool decoded = true;
		int count1 = 0;
		int count2 = 0;
		int i = 0;
		// room objects (optionnal)
		while (p < end && *p != 0xF6) {
			const uint8_t code = *p++;
			if (code != codes[i * 2]) {
				decoded = false;
				break;
			}
			i = (i + 1) & 1;
			const int len = *p++;
			if (len != 0) {
				++count1;
				if (!str) {
					str = p;
				}
			}
			if (!checkRoomString(p, len)) {
				decoded = false;
				break;
			}
			p += len;
		}
		// room dialogues
		++p;
		if (*p == 0xF4) {
			i = 0;
			while (p < end && *p != 0xF6) {
				const uint8_t code = *p++;
				if (code != codes[i * 2]) {
					decoded = false;
					break;
				}
				i = (i + 1) & 1;
				const int len = *p++;
				if (len != 0) {
					++count2;
					if (!str2) {
						str2 = p;
					}
				}
				if (!checkRoomString(p, len)) {
					decoded = false;
					break;
				}
				p += len;
			}
			++p;
		}
		if (!decoded || p > end || (count1 < 2 && count2 < 2)) {
			++cur;
			continue;
		}
		FILE *out = stdout;
		const int ptr1 = start - buf;
		const int ptr2 = p - buf;
		fprintf(out, "cseg%03d:%04X-%04X %08X (%3d,%3d) ", seg, ptr1, ptr2, offset + ptr1, count1, count2);
if (0) {
		static const int hashLen = 16;
		assert(start >= buf + hashLen);
		SHA1_CTX hash;
		SHA1Init(&hash);
		SHA1Update(&hash, start - hashLen, hashLen);
		uint8_t digest[SHA_DIGEST_LENGTH];
		SHA1Final(digest, &hash);
		for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
			fprintf(out, "%X%X", digest[i] >> 4, digest[i] & 15);
		}
}
		if (str2 || str) {
			const int len = str2 ? str2[-1] : str[-1];
			fprintf(out, "%3d:'", len);
			char strBuf[256];
			decodeRoomString(str2 ? str2 : str, strBuf, len);
			for (int i = 0; i < len; ++i) {
				fprintf(out, "%c", strBuf[i]);
			}
		}
		fprintf(out, "'\n");
		cur = p;
	}
}

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		File f;
		if (f.open("IGOR.FSD", argv[1])) {
			// floppy version, non-segmented exe
			OverlayExecutable ovl_exe;
			ovl_exe._exe.open("IGOR.EXE", argv[1]);
			ovl_exe._dat.open("IGOR.DAT", argv[1]);
			ovl_exe.parse();
			for (int i = 0; i < ovl_exe._stubsCount; ++i) {
				const int seg = i;
				const int size = ovl_exe.readSegment(seg, _bufSeg);
				if (_dumpAssets) {
					dumpRoom(seg, _bufSeg, size);
				}
				if (_dumpStrings) {
					dumpStrings(seg, _bufSeg, size, ovl_exe._stubs[seg].offset);
				}
			}
			return 0;
		}
		SegmentExecutable seg_exe("IGOR.EXE", argv[1]);
		seg_exe.parseSegmentsInfo();
		if (_dumpStrings) {
			for (int i = 0; i < seg_exe._segmentsCount; ++i) {
				const int seg = i + 1;
				const int size = seg_exe.readSegment(seg, _bufSeg);
				dumpStrings(seg, _bufSeg, size, seg_exe.getSegmentInfo(seg)->offset);
			}
			return 0;
		}
		if (argc >= 3) {
			Symbols syms;
			syms.load(argv[2]);
			for (int i = 0; i < syms._functionsCount; ++i) {
				switch (syms._functions[i].type) {
				case 't':
					assert(_trapsCount < MAX_TRAPS);
					_traps[_trapsCount++] = syms._functions[i].addr;
					break;
				case 'm':
					assert(_funcsCount < MAX_FUNCS);
					_funcs[_funcsCount++] = syms._functions[i].addr;
					break;
				}
			}
			// traps.h
			FILE *out = fopen("traps.h", "w");
			if (out) {
				fprintf(out, "static const Trap _traps[] = {\n");
				for (int i = 0; i < syms._functionsCount; ++i) {
					Function *func = &syms._functions[i];
					if (func->type == 't') {
						fprintf(out, "\t{ %03d, 0x%04X, &Game::trap_%s, \"%s\" },\n", func->addr >> 16, func->addr & 0xFFFF, func->name, func->signature);
					}
				}
				fprintf(out, "\t{ 0, 0, 0, 0 }\n");
				fprintf(out, "};\n");
				fclose(out);
			}
		}
		// data
		const int dataSeg = 231;
		int size = seg_exe.readSegment(dataSeg, _bufSeg);
		dumpBin("dseg.231", _bufSeg, size);
#if 0
		// code
		dumpCode(seg_exe, seg_exe._startCS, 0x8B7, 0xFA3);
		// SET_MAIN_ACTIONS
		dumpCode(seg_exe, 222, 0x3C2B, 0x3FC5);
		// actions
		dumpCode(seg_exe, 222, 0x2A6A, 0x3C2A);
		// startup logos - part_90
		dumpCodeRec(seg_exe,   9, 0x3CC2, 0x3EE7);
		// introduction sequence - part_85
		dumpCodeRec(seg_exe, 209, 0x0DA4, 0x13BD);
#endif
		if (_dumpCode) {
			// main + parts
			dumpMainParts(seg_exe, seg_exe._startCS, 0x8B7, 0xFA3);
			// actions
			dumpCodeRec(seg_exe, 222, 0x2A6A, 0x3C2A);
		}
		if (_dumpAssets) {
			// sound
			File f;
			if (f.open("IGOR.DAT", argv[1])) {
				dumpSound(f);
			}
			// rooms
			for (int i = 0; i < seg_exe._segmentsCount; ++i) {
				const int seg = i + 1;
				size = seg_exe.readSegment(seg, _bufSeg);
				dumpRoom(seg, _bufSeg, size);
			}
		}
	}
	return 0;
}
