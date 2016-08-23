
#ifndef UTIL_H__
#define UTIL_H__

static inline uint16_t READ_LE_UINT16(const uint8_t *b) {
	return b[0] | (b[1] << 8);
}

static inline uint32_t READ_LE_UINT32(const uint8_t *b) {
	return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

static inline void WRITE_LE_UINT16(uint8_t *b, uint16_t value) {
	b[0] = value & 255;
	b[1] = value >> 8;
}

static inline void WRITE_LE_UINT32(uint8_t *b, uint32_t value) {
	WRITE_LE_UINT16(&b[0], value & 0xFFFF);
	WRITE_LE_UINT16(&b[2], value >> 16);
}

#endif
