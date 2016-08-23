
#ifndef DETECTION_H__
#define DETECTION_H__

enum ExecutableType {
	kUnknownExe,
	kSegmentExe,
	kOverlayExe
};

enum GameVersion {
	kDemo100,
	kDemo110,
	kEngFloppy,
	kSpaCd,
};

int detectGameVersion(const char *path);

#endif // DETECTION_H__
