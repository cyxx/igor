
#ifndef ROOM_H__
#define ROOM_H__

bool checkRoomString(const uint8_t *src, int sz);
void decodeRoomString(const uint8_t *src, char *dst, int sz);
void decodeRoomData(int num, const uint8_t *src, int size);
void dumpRoomString(int num, const uint8_t *txt, const uint8_t *end);
void dumpRoomImage(int num, const uint8_t *src, int size);

#endif
