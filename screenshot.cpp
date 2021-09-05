
#include "screenshot.h"
#include "file.h"

static const uint16_t TAG_BM = 0x4D42;

void saveBMP(const char *filename, const uint8_t *bits, const uint8_t *pal, int w, int h) {
	File f;
	if (f.open(filename, "wb", ".")) {
		const int paletteSize = pal ? 4 * 256 : 0;
		const int bitsCount = pal ? 8 : 32;
		const int alignWidth = ((w * bitsCount / 8) + 3) & ~3;
		const int imageSize = alignWidth * h;

		// Write file header
		f.writeUint16LE(TAG_BM);
		f.writeUint32LE(14 + 40 + paletteSize + imageSize);
		f.writeUint16LE(0); // reserved1
		f.writeUint16LE(0); // reserved2
		f.writeUint32LE(14 + 40 + paletteSize);

		// Write info header
		f.writeUint32LE(40);
		f.writeUint32LE(w);
		f.writeUint32LE(h);
		f.writeUint16LE(1); // planes
		f.writeUint16LE(bitsCount); // bit_count
		f.writeUint32LE(0); // compression
		f.writeUint32LE(imageSize); // size_image
		f.writeUint32LE(0); // x_pels_per_meter
		f.writeUint32LE(0); // y_pels_per_meter
		f.writeUint32LE(0); // num_colors_used
		f.writeUint32LE(0); // num_colors_important

		if (pal) {
			// Write palette data
			for (int i = 0; i < 256; ++i) {
				f.writeByte(pal[2]);
				f.writeByte(pal[1]);
				f.writeByte(pal[0]);
				f.writeByte(0);
				pal += 3;
			}
		}

		// Write bitmap data
		w *= bitsCount / 8;
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
