
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2015 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "scaler.h"
#include "util.h"

static void point1x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	dstPitch >>= 1;
	while (h--) {
		memcpy(dst, src, w * 2);
		dst += dstPitch;
		src += srcPitch;
	}
}

static void point2x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	dstPitch >>= 1;
	const int dstPitch2 = dstPitch * 2;
	while (h--) {
		uint16_t *p = dst;
		for (int i = 0; i < w; ++i, p += 2) {
			const uint16_t c = *(src + i);
			for (int j = 0; j < 2; ++j) {
				*(p + j) = *(p + dstPitch + j) = c;
			}
		}
		dst += dstPitch2;
		src += srcPitch;
	}
}

static void point3x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	dstPitch >>= 1;
	const int dstPitch2 = dstPitch * 2;
	const int dstPitch3 = dstPitch * 3;
	while (h--) {
		uint16_t *p = dst;
		for (int i = 0; i < w; ++i, p += 3) {
			const uint16_t c = *(src + i);
			for (int j = 0; j < 3; ++j) {
				*(p + j) = *(p + dstPitch + j) = *(p + dstPitch2 + j) = c;
			}
		}
		dst += dstPitch3;
		src += srcPitch;
	}
}

static void point4x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	dstPitch >>= 1;
	const int dstPitch2 = dstPitch * 2;
	const int dstPitch3 = dstPitch * 3;
	const int dstPitch4 = dstPitch * 4;
	while (h--) {
		uint16_t *p = dst;
		for (int i = 0; i < w; ++i, p += 4) {
			const uint16_t c = *(src + i);
			for (int j = 0; j < 4; ++j) {
				*(p + j) = *(p + dstPitch + j) = *(p + dstPitch2 + j) = *(p + dstPitch3 + j) = c;
			}
		}
		dst += dstPitch4;
		src += srcPitch;
	}
}

static void scale2x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	dstPitch >>= 1;
	const int dstPitch2 = dstPitch * 2;
	while (h--) {
		uint16_t *p = dst;
		uint16_t D = *(src - 1);
		uint16_t E = *(src);
		for (int i = 0; i < w; ++i, p += 2) {
			uint16_t B = *(src + i - srcPitch);
			uint16_t F = *(src + i + 1);
			uint16_t H = *(src + i + srcPitch);
			if (B != H && D != F) {
				*(p) = D == B ? D : E;
				*(p + 1) = B == F ? F : E;
				*(p + dstPitch) = D == H ? D : E;
				*(p + dstPitch + 1) = H == F ? F : E;
			} else {
				*(p) = E;
				*(p + 1) = E;
				*(p + dstPitch) = E;
				*(p + dstPitch + 1) = E;
			}
			D = E;
			E = F;
		}
		dst += dstPitch2;
		src += srcPitch;
	}
}

static void scale3x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	dstPitch >>= 1;
	const int dstPitch2 = dstPitch * 2;
	const int dstPitch3 = dstPitch * 3;
	while (h--) {
		uint16_t *p = dst;
		uint16_t A = *(src - srcPitch - 1);
		uint16_t B = *(src - srcPitch);
		uint16_t D = *(src - 1);
		uint16_t E = *(src);
		uint16_t G = *(src + srcPitch - 1);
		uint16_t H = *(src + srcPitch);
		for (int i = 0; i < w; ++i, p += 3) {
			uint16_t C = *(src + i - srcPitch + 1);
			uint16_t F = *(src + i + 1);
			uint16_t I = *(src + i + srcPitch + 1);
			if (B != H && D != F) {
				*(p) = D == B ? D : E;
				*(p + 1) = (D == B && E != C) || (B == F && E != A) ? B : E;
				*(p + 2) = B == F ? F : E;
				*(p + dstPitch) = (D == B && E != G) || (D == B && E != A) ? D : E;
				*(p + dstPitch + 1) = E;
				*(p + dstPitch + 2) = (B == F && E != I) || (H == F && E != C) ? F : E;
				*(p + dstPitch2) = D == H ? D : E;
				*(p + dstPitch2 + 1) = (D == H && E != I) || (H == F && E != G) ? H : E;
				*(p + dstPitch2 + 2) = H == F ? F : E;
			} else {
				*(p) = E;
				*(p + 1) = E;
				*(p + 2) = E;
				*(p + dstPitch) = E;
				*(p + dstPitch + 1) = E;
				*(p + dstPitch + 2) = E;
				*(p + dstPitch2) = E;
				*(p + dstPitch2 + 1) = E;
				*(p + dstPitch2 + 2) = E;
			}
			A = B;
			B = C;
			D = E;
			E = F;
			G = H;
			H = I;
		}
		dst += dstPitch3;
		src += srcPitch;
	}
}

void scale4x(uint16_t *dst, int dstPitch, const uint16_t *src, int srcPitch, int w, int h) {
	static struct {
		uint16_t *ptr;
		int w, h, pitch;
		int size;
	} buf;
	const int size = (w * 2 + 2) * (h * 2 + 2) * sizeof(uint16_t);
	if (buf.size < size) {
		free(buf.ptr);
		buf.size = size;
		buf.w = w * 2;
		buf.h = h * 2;
		buf.pitch = buf.w + 2;
		buf.ptr = (uint16_t *)malloc(buf.size);
		if (!buf.ptr) {
			error("Unable to allocate scale4x intermediate buffer");
		}
	}
	scale2x(buf.ptr + buf.pitch + 1, buf.pitch * sizeof(uint16_t), src, srcPitch, w, h);
	scale2x(dst, dstPitch, buf.ptr + buf.pitch + 1, buf.pitch, w * 2, h * 2);
}

const Scaler _scalers[] = {
	{ "point1x", &point1x, 1 },
	{ "point2x", &point2x, 2 },
	{ "scale2x", &scale2x, 2 },
	{ "point3x", &point3x, 3 },
	{ "scale3x", &scale3x, 3 },
	{ "point4x", &point4x, 4 },
	{ "scale4x", &scale4x, 4 },
	{ 0, 0, 0 }
};

