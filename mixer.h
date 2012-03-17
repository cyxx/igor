
#ifndef MIXER_H__
#define MIXER_H__

#include <stdint.h>

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

#endif // MIXER_H__
