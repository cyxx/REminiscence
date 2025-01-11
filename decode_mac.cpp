
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "decode_mac.h"
#include "util.h"

uint8_t *decodeLzss(File &f, uint32_t &decodedSize) {
	decodedSize = f.readUint32BE();
	uint8_t *dst = (uint8_t *)malloc(decodedSize);
	if (!dst) {
		warning("Failed to allocate %d bytes for LZSS", decodedSize);
		return 0;
	}
	uint32_t count = 0;
	while (count < decodedSize) {
		const int code = f.readByte();
		for (int i = 0; i < 8 && count < decodedSize; ++i) {
			if ((code & (1 << i)) == 0) {
				dst[count++] = f.readByte();
			} else {
				int offset = f.readUint16BE();
				const int len = (offset >> 12) + 3;
				offset &= 0xFFF;
				for (int j = 0; j < len; ++j) {
					dst[count + j] = dst[count - offset - 1 + j];
				}
				count += len;
			}
		}
	}
	assert(count == decodedSize);
	return dst;
}

void decodeC103(const uint8_t *src, int w, int h, DecodeBuffer *buf) {
	static const int kBits = 12;
	static const int kMask = (1 << kBits) - 1;
	int cursor = 0;
	int bits = 1;
	int count = 0;
	int offset = 0;
	uint8_t window[(1 << kBits)];

	uint8_t *dst = buf->clip_buf ? buf->clip_buf : buf->ptr;
	for (int y = 0; y < h; ++y, dst += w) {
		for (int x = 0; x < w; ++x) {
			if (count == 0) {
				int carry = bits & 1;
				bits >>= 1;
				if (bits == 0) {
					bits = *src++;
					if (carry) {
						bits |= 0x100;
					}
					carry = bits & 1;
					bits >>= 1;
				}
				if (!carry) {
					const uint8_t color = *src++;
					window[cursor++] = color;
					cursor &= kMask;
					dst[x] = color;
					continue;
				}
				offset = READ_BE_UINT16(src); src += 2;
				count = 3 + (offset >> 12);
				offset &= kMask;
				offset = (cursor - offset - 1) & kMask;
			}
			const uint8_t color = window[offset++];
			offset &= kMask;
			window[cursor++] = color;
			cursor &= kMask;
			dst[x] = color;
			--count;
		}
	}
}

void decodeC211(const uint8_t *src, int w, int h, DecodeBuffer *buf) {
	struct {
		const uint8_t *ptr;
		int repeatCount;
	} stack[512];
	int x = 0;
	int sp = 0;

	uint8_t *dst = buf->clip_buf ? buf->clip_buf : buf->ptr;
	while (1) {
		const uint8_t code = *src++;
		if ((code & 0x80) != 0) {
			dst += w;
			x = 0;
		}
		int count = code & 0x1F;
		if (count == 0) {
			count = READ_BE_UINT16(src); src += 2;
		}
		if ((code & 0x40) == 0) {
			if ((code & 0x20) == 0) {
				if (count == 1) {
					assert(sp > 0);
					--stack[sp - 1].repeatCount;
					if (stack[sp - 1].repeatCount >= 0) {
						src = stack[sp - 1].ptr;
					} else {
						--sp;
					}
				} else {
					assert(sp < ARRAYSIZE(stack));
					stack[sp].ptr = src;
					stack[sp].repeatCount = count - 1;
					++sp;
				}
			} else {
				x += count;
			}
		} else {
			if ((code & 0x20) == 0) {
				if (count == 1) {
					return;
				}
				const uint8_t color = *src++;
				memset(dst + x, color, count);
			} else {
				memcpy(dst + x, src, count);
				src += count;
			}
			x += count;
		}
	}
}
