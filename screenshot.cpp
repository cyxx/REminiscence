
#include "screenshot.h"
#include "file.h"

static void TO_LE16(uint8_t *dst, uint16_t value) {
	for (int i = 0; i < 2; ++i) {
		dst[i] = value & 255;
		value >>= 8;
	}
}

#define kTgaImageTypeUncompressedTrueColor 2
#define kTgaImageTypeRunLengthEncodedTrueColor 10
#define kTgaDirectionTop (1 << 5)

static const int TGA_HEADER_SIZE = 18;

void saveTGA(const char *filename, const uint8_t *rgba, int w, int h) {

	static const uint8_t kImageType = kTgaImageTypeRunLengthEncodedTrueColor;
	uint8_t buffer[TGA_HEADER_SIZE];
	buffer[0]            = 0; // ID Length
	buffer[1]            = 0; // ColorMap Type
	buffer[2]            = kImageType;
	TO_LE16(buffer +  3,   0); // ColorMap Start
	TO_LE16(buffer +  5,   0); // ColorMap Length
	buffer[7]            = 0;  // ColorMap Bits
	TO_LE16(buffer +  8,   0); // X-origin
	TO_LE16(buffer + 10,   0); // Y-origin
	TO_LE16(buffer + 12,   w); // Image Width
	TO_LE16(buffer + 14,   h); // Image Height
	buffer[16]           = 24; // Pixel Depth
	buffer[17]           = kTgaDirectionTop;  // Descriptor

	File f;
	if (f.open(filename, "wb", ".")) {
		f.write(buffer, sizeof(buffer));
		if (kImageType == kTgaImageTypeUncompressedTrueColor) {
			for (int i = 0; i < w * h; ++i) {
				f.writeByte(rgba[0]);
				f.writeByte(rgba[1]);
				f.writeByte(rgba[2]);
				rgba += 4;
			}
		} else {
			assert(kImageType == kTgaImageTypeRunLengthEncodedTrueColor);
			int prevColor = rgba[2] + (rgba[1] << 8) + (rgba[0] << 16); rgba += 4;
			int count = 0;
			for (int i = 1; i < w * h; ++i) {
				int color = rgba[2] + (rgba[1] << 8) + (rgba[0] << 16); rgba += 4;
				if (prevColor == color && count < 127) {
					++count;
					continue;
				}
				f.writeByte(count | 0x80);
				f.writeByte((prevColor >> 16) & 255);
				f.writeByte((prevColor >>  8) & 255);
				f.writeByte( prevColor        & 255);
				count = 0;
				prevColor = color;
			}
			if (count != 0) {
				f.writeByte(count | 0x80);
				f.writeByte((prevColor >> 16) & 255);
				f.writeByte((prevColor >>  8) & 255);
				f.writeByte( prevColor        & 255);
			}
		}
	}
}
