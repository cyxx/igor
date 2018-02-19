
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
	kSpaFloppy,
	kSpaCd,
};

int detectGameVersion(const char *path);
ExecutableType getExecutableType(int version);

#endif // DETECTION_H__
