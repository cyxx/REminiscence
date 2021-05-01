
#include "screenshot.h"
#include "file.h"

#define kTgaImageTypeUncompressedTrueColor 2
#define kTgaImageTypeRunLengthEncodedTrueColor 10
#define kTgaDirectionTop (1 << 5)

void saveTGA(const char *filename, const uint8_t *rgba, int w, int h) {
	static const uint8_t kImageType = kTgaImageTypeRunLengthEncodedTrueColor;
	File f;
	if (f.open(filename, "wb", ".")) {

		f.writeByte(0); // ID Length
		f.writeByte(0); // ColorMap Type
		f.writeByte(kImageType);
		f.writeUint16LE(0); // ColorMap Start
		f.writeUint16LE(0); // ColorMap Length
		f.writeByte(0);     // ColorMap Bits
		f.writeUint16LE(0); // X-origin
		f.writeUint16LE(0); // Y-origin
		f.writeUint16LE(w); // Image Width
		f.writeUint16LE(h); // Image Height
		f.writeByte(24);    // Pixel Depth
		f.writeByte(kTgaDirectionTop); // Descriptor

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

static const uint16_t TAG_BM = 0x4D42;

void saveBMP(const char *filename, const uint8_t *bits, const uint8_t *pal, int w, int h) {
	File f;
	if (f.open(filename, "wb", ".")) {
		const int alignWidth = (w + 3) & ~3;
		const int imageSize = alignWidth * h;

		// Write file header
		f.writeUint16LE(TAG_BM);
		f.writeUint32LE(14 + 40 + 4 * 256 + imageSize);
		f.writeUint16LE(0); // reserved1
		f.writeUint16LE(0); // reserved2
		f.writeUint32LE(14 + 40 + 4 * 256);

		// Write info header
		f.writeUint32LE(40);
		f.writeUint32LE(w);
		f.writeUint32LE(h);
		f.writeUint16LE(1); // planes
		f.writeUint16LE(8); // bit_count
		f.writeUint32LE(0); // compression
		f.writeUint32LE(imageSize); // size_image
		f.writeUint32LE(0); // x_pels_per_meter
		f.writeUint32LE(0); // y_pels_per_meter
		f.writeUint32LE(0); // num_colors_used
		f.writeUint32LE(0); // num_colors_important

		// Write palette data
		for (int i = 0; i < 256; ++i) {
			f.writeByte(pal[2]);
			f.writeByte(pal[1]);
			f.writeByte(pal[0]);
			f.writeByte(0);
			pal += 3;
		}

		// Write bitmap data
		const int pitch = w;
		bits += h * pitch;
		for (int i = 0; i < h; ++i) {
			bits -= pitch;
			f.write(bits, w);
			int pad = alignWidth - w;
			while (pad--) {
				f.writeByte(0);
			}
		}
	}
}
