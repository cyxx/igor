
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

static inline double READ_REAL(uint16_t a, uint16_t b, uint16_t c) {
	if ((a & 0xFF) == 0) {
		return 0.;
	}
	const int sign = ((c & 0x8000) != 0) ? -1 : 1;
	const int mantissaBits = ((c & 0x7FFF) << 24) | (b << 8) | (a >> 8);
	double mantissa = 1.;
	for (int i = 0; i < 5; ++i) {
		const int n = (mantissaBits >> (8 * (4 - i))) & 0xFF;
		mantissa += n / (float)(1 << (7 + i * 8));
	}
	const int exponent = (a & 0xFF) - 129;
	return sign * (1 << exponent) * mantissa;
}

#endif
