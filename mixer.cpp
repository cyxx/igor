
#include <tremor/ivorbisfile.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "mixer.h"
#include "util.h"

struct MixerChannel {
	virtual ~MixerChannel() {}
	virtual bool load(int rate, uint32_t offset = 0) = 0;
	virtual bool readSample(int16_t &sample) = 0;
};

struct Frac {
	static const int kBits = 16;
	uint32_t inc;
	uint32_t offset;
	int pos;
};

struct MixerChannel_Voc : MixerChannel {

	File _f;
	uint32_t _fileOffset;
	int _rate;
	int _size;
	Frac _sfrac;
	uint8_t _sbuf;

	~MixerChannel_Voc() {
		_f.close();
	}

	virtual bool load(int rate, uint32_t offset) {
		_rate = rate;
		_f.seek(offset);
		uint8_t buf[26];
		_f.read(buf, sizeof(buf));
		if (memcmp(buf, "Creative Voice File", 19) != 0) {
			return false;
		}
		const int size = READ_LE_UINT16(buf + 20);
		assert(size == sizeof(buf));
		_fileOffset = offset + sizeof(buf);
		return readCode() != 0;
	}
	virtual bool readSample(int16_t &sample) {
		const int pos = _sfrac.offset >> Frac::kBits;
		if (pos > _sfrac.pos) {
			_sfrac.pos = pos;
			if (_size == 0) {
				if (readCode() == 0) {
					return false;
				}
				_sfrac.pos = 0;
			}
			--_size;
			_sbuf = _f.readByte();
		}
		_sfrac.offset += _sfrac.inc;
		// unsigned 8 to signed 16
		sample = (_sbuf << 8) ^ 0x8000;
		return true;
	}
	int readCode() {
		const int code = _f.readByte();
		if (code == 0) {
			return 0;
		}
		_size = _f.readUint16LE();
		_size |= _f.readByte() << 16;
		_fileOffset += _size + 4;
		switch (code) {
		case 1: { // pcm data
				const int rate = 1000000 / (256 - _f.readByte());
				_sfrac.inc = (rate << Frac::kBits) / _rate;
				const int codec = _f.readByte();
				if (codec != 0) {
					warning("unhandled .voc codec %d", codec);
					return 0;
				}
				_size -= 2;
				_sfrac.offset = 0;
				_sfrac.pos = -1;
				_sbuf = 0;
			}
			break;
		case 5: // comment, skip to next code
			_f.seek(_fileOffset);
			return readCode();
		default:
			warning("unhandled .voc code %d", code);
			return 0;
		}
		return code;
	}
};

struct VorbisFile: File {
	uint32_t offset;

	static size_t readHelper(void *ptr, size_t size, size_t nmemb, void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		if (size != 0 && nmemb != 0) {
			const int n = vf->read(ptr, size * nmemb);
			if (n > 0) {
				vf->offset += n;
				return n / size;
			}
		}
		return 0;
	}
	static int seekHelper(void *datasource, ogg_int64_t offset, int whence) {
		VorbisFile *vf = (VorbisFile *)datasource;
		switch (whence) {
		case SEEK_SET:
			vf->offset = offset;
			break;
		case SEEK_CUR:
			vf->offset += offset;
			break;
		case SEEK_END:
			vf->offset = vf->size() + offset;
			break;
		}
		vf->seek(vf->offset);
		return 0;
	}
	static long tellHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		return vf->offset;
	}
};

struct MixerChannel_Vorbis : MixerChannel {

	VorbisFile _f;
	OggVorbis_File _ovf;
	int _channels;
	int16_t _sbuf[2];

	~MixerChannel_Vorbis() {
		_f.close();
		if (_ovf.datasource) {
			ov_clear(&_ovf);
		}
	}
	virtual bool load(int rate, uint32_t offset) {
		_f.offset = 0;
		memset(&_ovf, 0, sizeof(_ovf));
		ov_callbacks ovcb;
		memset(&ovcb, 0, sizeof(ovcb));
		ovcb.read_func = VorbisFile::readHelper;
		ovcb.seek_func = VorbisFile::seekHelper;
		ovcb.tell_func = VorbisFile::tellHelper;
		if (ov_open_callbacks(&_f, &_ovf, 0, 0, ovcb) < 0) {
			warning("invalid .ogg input file");
			return false;
		}
		const vorbis_info *vi = ov_info(&_ovf, -1);
		if (vi->rate != rate) {
			warning("unsupported .ogg sample rate %d hz", (int)vi->rate);
			return false;
		}
		_channels = vi->channels;
		return true;
	}
	virtual bool readSample(int16_t &sample) {
		while (1) {
			const int len = ov_read(&_ovf, (char *)_sbuf, _channels * sizeof(int16_t), 0);
			if (len < 0) {
				// error in decoder
				return false;
			} else if (len != 0) {
				break;
			}
			// end of stream, loop
			ov_raw_seek(&_ovf, 0);
		}
		// output is mono
		switch (_channels) {
		case 2:
			sample = (_sbuf[0] + _sbuf[1]) / 2;
			break;
		default:
			sample = _sbuf[0];
			break;
		}
		return true;
	}
};

struct MixerLock {
	void (*_lock)(int);
	MixerLock(void (*lock)(int))
		: _lock(lock) {
		_lock(1);
	}
	~MixerLock() {
		_lock(0);
	}
	static void noLock(int) {
	}
};

Mixer::Mixer(const char *dataPath)
	: _dataPath(dataPath), _rate(0), _lock(&MixerLock::noLock) {
	_sound = 0;
	_track = 0;
}

Mixer::~Mixer() {
}

void Mixer::setFormat(int rate, int fmt) {
	_rate = rate;
}

void Mixer::playSound(int num, int type, int offset) {
	stopSound();
	MixerChannel *channel = new MixerChannel_Voc;
	if (!((MixerChannel_Voc *)channel)->_f.open("IGOR.DAT", _dataPath, "rb") || !channel->load(_rate, offset)) {
		delete channel;
		channel = 0;
	}
	_sound = channel;
}

void Mixer::stopSound() {
	MixerLock ml(_lock);
	delete _sound;
	_sound = 0;
}

bool Mixer::isSoundPlaying() {
	MixerLock ml(_lock);
	return _sound != 0;
}

void Mixer::playTrack(int num) {
	stopTrack();
	MixerLock ml(_lock);
	char name[16];
	snprintf(name, sizeof(name), "track%02d.ogg", num);
	MixerChannel *channel = new MixerChannel_Vorbis;
	if (!((MixerChannel_Vorbis *)channel)->_f.open(name, _dataPath, "rb") || !channel->load(_rate)) {
		delete channel;
		channel = 0;
	}
	_track = channel;
}

void Mixer::stopTrack() {
	MixerLock ml(_lock);
	delete _track;
	_track = 0;
}

bool Mixer::isTrackPlaying() {
	MixerLock ml(_lock);
	return _track != 0;
}

static void mix(int16_t *dst, int pcm, int volume) {
	pcm = *dst + ((pcm * volume) >> 8);
	if (pcm < -32768) {
		pcm = -32768;
	} else if (pcm > 32767) {
		pcm = 32767;
	}
	*dst = pcm;
}

void Mixer::mixBuf(int16_t *buf, int len) {
	int16_t sample;
	if (_sound) {
		for (int i = 0; i < len; ++i) {
			if (!_sound->readSample(sample)) {
				delete _sound;
				_sound = 0;
				break;
			}
			mix(&buf[i], sample, 256);
		}
	}
	if (_track) {
		for (int i = 0; i < len; ++i) {
			if (!_track->readSample(sample)) {
				delete _track;
				_track = 0;
				break;
			}
			mix(&buf[i], sample, 128); // soften music compared to sfx
		}
	}
}

void Mixer::mixCb(void *param, uint8_t *buf, int len) {
	memset(buf, 0, len);
	((Mixer *)param)->mixBuf((int16_t *)buf, len / 2);
}
