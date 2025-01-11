
#ifndef DECODE_MAC_H__
#define DECODE_MAC_H__

#include <stdint.h>
#include "file.h"

uint8_t *decodeLzss(File &f, uint32_t &decodedSize);

struct DecodeBuffer {
	uint8_t *ptr;
	int dst_w, dst_h;
	int dst_x, dst_y;

	int orig_w, orig_h;
	int clip_x, clip_y;
	int clip_w, clip_h;
	uint8_t *clip_buf;
};

void decodeC103(const uint8_t *a3, int w, int h, DecodeBuffer *buf);
void decodeC211(const uint8_t *a3, int w, int h, DecodeBuffer *buf);

#endif
