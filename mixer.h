
#ifndef MIXER_H__
#define MIXER_H__

#include <stdint.h>

#ifndef USE_MIXER_IMPL

struct MixerChannel;

struct Mixer {

	const char *_dataPath;
	int _rate;
	void (*_lock)(int);
	MixerChannel *_sound;
	MixerChannel *_track;

	Mixer(const char *dataPath);
	~Mixer();

	void setFormat(int rate, int fmt);

	void playSound(int num, int type, int offset);
	void stopSound();
	bool isSoundPlaying();

	void playTrack(int num);
	void stopTrack();
	bool isTrackPlaying();

	void mixBuf(int16_t *buf, int len);
	static void mixCb(void *param, uint8_t *buf, int len);
};

#else

struct MixerImpl;

struct Mixer {

	MixerImpl *_impl;
	const char *_dataPath;

	Mixer(const char *dataPath)
		: _impl(0), _dataPath(dataPath) {
	}
	~Mixer() {
	}

	void playSound(int num, int type, int offset) {}
	void stopSound() {}
	bool isSoundPlaying() { return false; }

	void playTrack(int num) {}
	void stopTrack() {}
	bool isTrackPlaying() { return false; }
};

#endif

#endif // MIXER_H__
