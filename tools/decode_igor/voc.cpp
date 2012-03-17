
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "util.h"
#include "voc.h"

static uint8_t _pcmBuf[8 << 16];
static int _pcmRate, _pcmSize;

static void writeWavPcm(File &f) {
	static const int chunkSize_fmt = 16;
	static const int bitsPerSample = 8;
	f.write("RIFF", 4);
	f.writeUint32LE( 4 + (8 + chunkSize_fmt) + (8 + _pcmSize) );
		f.write("WAVE", 4);
			f.write("fmt ", 4);
			f.writeUint32LE(chunkSize_fmt); // 16
				f.writeUint16LE(1); // pcm
				f.writeUint16LE(1); // num_channels
				f.writeUint32LE(_pcmRate); // sample_rate
				f.writeUint32LE(_pcmRate * 1 * bitsPerSample / 8); // byte_rate
				f.writeUint16LE(1 * bitsPerSample / 8); // block_align
				f.writeUint16LE(bitsPerSample); // bits_per_sample
			f.write("data", 4);
			f.writeUint32LE(_pcmSize);
				f.write(_pcmBuf, _pcmSize);
	f.close();
}

static int fixRate(int rate) {
	static const int rates[] = {
		22222, 22050,
		11111, 11025,
		10989, 11025,
		 8000,  8000,
		0, 0
	};
	for (int i = 0; rates[i]; i += 2) {
		if (rates[i] == rate) {
			return rates[i + 1];
		}
	}
	fprintf(stderr, "Unhandled voc rate %d hz\n", rate);
	exit(1);
	return 0;
}

static void decodeCode_1(File &f, int size) {
	const int div = 256 - f.readByte();
	_pcmRate = 1000000 / div;
	const int codec = f.readByte();
	assert(codec == 0);
	size -= 2;
	assert(_pcmSize + size <= sizeof(_pcmBuf));
	f.read(_pcmBuf + _pcmSize, size);
	_pcmSize += size;
}

static void decodeCode_5(File &f, int size) {
	char str[32];
	assert(size <= sizeof(str));
	f.read(str, size);
	fprintf(stdout, "str (%d) '", size);
	for (int i = 0; i < size; ++i) {
		fprintf(stdout, "%02x", str[i]);
	}
	fprintf(stdout, "'\n");
}

void decodeVoc(const char *name, const char *path) {
	File f;
	if (!f.open(name, path, "rb")) {
		return;
	}
	uint8_t buf[26];
	f.read(buf, sizeof(buf));
	if (memcmp(buf, "Creative Voice File", 19) == 0) {
		const int size = READ_LE_UINT16(buf + 20);
		assert(size == sizeof(buf));
	}
	bool end = false;
	_pcmSize = 0;
	while (!f.ioErr() && !end) {
		const uint8_t code = f.readByte();
		int size = f.readUint16LE();
		size |= ((int)f.readByte()) << 24;
		switch (code) {
		case 0:
			end = true;
			break;
		case 1:
			decodeCode_1(f, size);
			break;
		case 5:
			decodeCode_5(f, size);
			break;
		default:
			fprintf(stderr, "Unhandled voc code %d size %d\n", code, size);
			exit(1);
			break;
		}
	}
	_pcmRate = fixRate(_pcmRate);
	if (_pcmSize != 0) {
		char wav[32];
		strcpy(wav, name);
		char *p = strrchr(wav, '.');
		if (p) {
			strcpy(p, ".wav");
		}
		File out;
		if (out.open(wav, path, "wb")) {
			writeWavPcm(out);
		}
	}
}
