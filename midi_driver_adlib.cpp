
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "midi_driver.h"
extern "C" {
#include "opl3.c"
}

#define NUM_CHANNELS 11 /* rhythm mode */

static opl3_chip _opl3;

static const int kMusicVolume = 63; // 44;

/* Two-operator melodic and percussion mode */
static const uint8_t _adlibOperatorIndexTable[] = { 0, 3, 1, 4, 2, 5, 6, 9, 7, 0xA, 8, 0xB, 0xC, 0xF, 0x10, 0x10, 0xE, 0xE, 0x11, 0x11, 0xD, 0xD };

static const uint8_t _adlibOperatorTable[] = { 0, 1, 2, 3, 4, 5, 8, 9, 0xA, 0xB, 0xC, 0xD, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15 };

static const uint16_t _adlibFrequencyTable[] = {
        0x157, 0x16C, 0x181, 0x198, 0x1B1, 0x1CB, 0x1E6, 0x203, 0x222, 0x243, 0x266, 0x28A
};

static struct {
	uint8_t midi_channel; // track_num
	uint8_t note;
	uint8_t hw_channel;
	const uint8_t *instrument;
} _adlibPlayingNoteTable[NUM_CHANNELS];

static int16_t _adlibChannelPlayingTable[NUM_CHANNELS]; // could be a bitmask
static int16_t _adlibChannelVolumeTable[NUM_CHANNELS];
static uint8_t _regBD = 0x20;

static const uint8_t *_currentInstrumentData;

static uint16_t READ_LE_UINT16(const uint8_t *p) {
	return p[0] | (p[1] << 8);
}

struct MidiDriver_adlib: MidiDriver {
	virtual int init() {
		return 0;
	}
	virtual void fini() {
	}

	virtual void reset(int rate) {
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			_adlibPlayingNoteTable[i].midi_channel = 0xFF;
			_adlibChannelPlayingTable[i] = 0;
			_adlibChannelVolumeTable[i] = 10;
		}
		OPL3_Reset(&_opl3, rate);
		adlibReset();
	}

	virtual void setInstrumentData(int channel, int num, const void *data) {
		// fprintf(stdout, "instrument channel:%d num:%d data:%p\n", channel, num, data);
		assert(data);
		_currentInstrumentData = (const uint8_t *)data;
	}

	virtual void noteOff(int channel, int note, int velocity) {
		// fprintf(stdout, "noteOff %d channel %d\n", note, channel);
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			if (_adlibPlayingNoteTable[i].midi_channel != channel) {
				continue;
			}
			if (_adlibPlayingNoteTable[i].note != note) {
				//fprintf(stdout, "WARNING: noteOff: channel %d is playing note %d (expected %d)\n", channel, _adlibPlayingNoteTable[i].note, note);
				continue;
			}
			_adlibPlayingNoteTable[i].midi_channel = 0xFF;
			const int hw_channel = _adlibPlayingNoteTable[i].hw_channel;
			//fprintf(stdout, "free hw_channel %d\n", hw_channel);
			_adlibChannelPlayingTable[hw_channel] = 0;
			if (_adlibPlayingNoteTable[i].instrument != _currentInstrumentData) {
				// fprintf(stdout, "WARNING: noteOff: instrument changed for channel %d hw_channel %d\n", channel, hw_channel);
			}
			if (_adlibPlayingNoteTable[i].instrument[0] == 0) { //_currentInstrumentData[0] == 0) {
				adlibMelodicKeyOff(hw_channel, note);
			} else {
				adlibPercussionKeyOff(hw_channel);
			}
			return;
		}
		// fprintf(stdout, "WARNING: noteOff: channel %d note %d not found\n", channel, note);
	}
	virtual void noteOn(int channel, int note, int velocity) {
		if (_currentInstrumentData[0] == 0) {
			/* melodic channel */
			int i;
			for (i = 0; i < 6; ++i) {
				if (_adlibChannelPlayingTable[i] == 0) {
					break;
				}
			}
			if (i == 6) {
				// fprintf(stdout, "No free melodic channel found (A) 6 voices busy, track %d note %d\n", channel, note);
				return;
			}
			const int hw_channel = i;
			_adlibChannelPlayingTable[hw_channel] = 0xFF;
			for (i = 0; i < NUM_CHANNELS; ++i) {
				if (_adlibPlayingNoteTable[i].midi_channel == 0xFF) {
					break;
				}
			}
			if (i == NUM_CHANNELS) {
				_adlibChannelPlayingTable[hw_channel] = 0;
				// fprintf(stdout, "No free channel found (B)\n");
				return;
			}
			_adlibPlayingNoteTable[i].midi_channel = channel;
			_adlibPlayingNoteTable[i].note = note;
			_adlibPlayingNoteTable[i].hw_channel = hw_channel;
			_adlibPlayingNoteTable[i].instrument = _currentInstrumentData;
			_adlibChannelVolumeTable[hw_channel] = velocity;
			adlibSetupInstrument(hw_channel, _currentInstrumentData);
			adlibMelodicKeyOn(hw_channel, velocity, note);
		} else {
			/* percussion channel */
			const int hw_channel = _currentInstrumentData[1]; // channel
			assert(hw_channel >= 6 && hw_channel <= 10);
			if (_adlibChannelPlayingTable[hw_channel] != 0) {
				// fprintf(stdout, "No free channel found (C) hw_channel %d for track %d note %d\n", hw_channel, channel, note);
				return;
			}
			_adlibChannelPlayingTable[hw_channel] = 0xFF;
			int i;
			for (i = 6; i < NUM_CHANNELS; ++i) {
				if (_adlibPlayingNoteTable[i].midi_channel == 0xFF) {
					break;
				}
			}
			if (i == NUM_CHANNELS) {
				_adlibChannelPlayingTable[hw_channel] = 0;
				// fprintf(stdout, "No free channel found (D) hw_channel %d\n", hw_channel);
				return;
			}
			//fprintf(stdout, "Adding percussion channel %d note %d index %d track %d\n", hw_channel, note, i, channel);
			_adlibPlayingNoteTable[i].midi_channel = channel;
			_adlibPlayingNoteTable[i].note = note;
			_adlibPlayingNoteTable[i].hw_channel = hw_channel;
			_adlibPlayingNoteTable[i].instrument = _currentInstrumentData;
			_adlibChannelVolumeTable[hw_channel] = velocity;
			adlibSetupInstrument(hw_channel, _currentInstrumentData);
			adlibPercussionKeyOn(hw_channel, velocity, note);
		}
	}
	virtual void controlChange(int channel, int type, int value) {
	}
	virtual void programChange(int channel, int num) {
	}
	virtual void pitchBend(int channel, int lsb, int msb) {
		adlibPitchBend(channel, (msb << 7) | lsb);
	}

	virtual void readSamples(int16_t *samples, int len) {
		OPL3_GenerateStream(&_opl3, samples, len);
	}

	void adlibSetupInstrument(int channel, const uint8_t *arg0) {
		const int volume = (_adlibChannelVolumeTable[channel] * kMusicVolume) >> 6;
		// fprintf(stdout, "instrument channel:%d volume:%d\n", channel, volume);
		const int mode = arg0[0];
		const int hw_channel = arg0[1];
		const int num = (mode != 0) ? hw_channel : channel;
		const uint8_t modulatorOperator = _adlibOperatorTable[_adlibOperatorIndexTable[num * 2]];
		const uint8_t carrierOperator = _adlibOperatorTable[_adlibOperatorIndexTable[num * 2 + 1]];
		//fprintf(stdout, "operator modulator %d carrier %d\n", modulatorOperator, carrierOperator);
		if (mode == 0 || hw_channel == 6) { // else goto loc_F24_284E5
			// modulator
			const uint8_t *bx = arg0 + 6;
			uint16_t reg2x = 0;
			if (READ_LE_UINT16(bx + 0x12) != 0) { // amplitude vibrato
				reg2x |= 0x80;
			}
			if (READ_LE_UINT16(bx + 0x14) != 0) { // frequency vibrato
				reg2x |= 0x40;
			}
			if (READ_LE_UINT16(bx + 0x0A) != 0) { // sustaining sound
				reg2x |= 0x20;
			}
			if (READ_LE_UINT16(bx + 0x16) != 0) { // envelope scaling
				reg2x |= 0x10;
			}
			reg2x |= READ_LE_UINT16(bx + 2) & 15; // frequency multiplier
			writeRegister(0x20 + modulatorOperator, reg2x);
			uint16_t dx;
			if (READ_LE_UINT16(bx + 0x18) != 0) { // frequency modulation
				dx = READ_LE_UINT16(bx + 0x10) & 63;
			} else {
				const int ax = (63 - (READ_LE_UINT16(bx + 0x10) & 63)) * volume; // output level
				dx = 63 - (((ax + 127) + ax) / 254);
			}
			const uint8_t reg4x = dx | (READ_LE_UINT16(bx) << 6);
			writeRegister(0x40 + modulatorOperator, reg4x);
			writeRegister(0x60 + modulatorOperator, (READ_LE_UINT16(bx + 12) & 15) | (READ_LE_UINT16(bx + 6) << 4));
			writeRegister(0x80 + modulatorOperator, (READ_LE_UINT16(bx + 14) & 15) | (READ_LE_UINT16(bx + 8) << 4));
			uint16_t ax = 1;
			if (READ_LE_UINT16(bx + 0x18) != 0) {
				ax = 0;
			}
			dx = (READ_LE_UINT16(bx + 4) << 1) | ax;
			ax = 0xC0 + ((mode != 0) ? hw_channel : channel);
			writeRegister(ax, dx);
			writeRegister(0xE0 + modulatorOperator, arg0[2] & 3); /* original writes to 0xE, typo */
		}
		// carrier
		const uint8_t *bx = arg0 + 0x20; // 26 + 6
		uint8_t reg2x = 0;
		if (READ_LE_UINT16(bx + 0x12) != 0) {
			reg2x |= 0x80;
		}
		if (READ_LE_UINT16(bx + 0x14) != 0) {
			reg2x |= 0x40;
		}
		if (READ_LE_UINT16(bx + 0x0A) != 0) {
			reg2x |= 0x20;
		}
		if (READ_LE_UINT16(bx + 0x16) != 0) {
			reg2x |= 0x10;
		}
		reg2x |= (READ_LE_UINT16(bx + 2) & 15);
		writeRegister(0x20 + carrierOperator, reg2x);
		const int ax = (63 - (READ_LE_UINT16(bx + 0x10) & 63)) * volume;
		int dx = 63 - (((ax + 127) + ax) / 254);
		const uint8_t reg4x = dx | (READ_LE_UINT16(bx) << 6);
		writeRegister(0x40 + carrierOperator, reg4x);
		writeRegister(0x60 + carrierOperator, (READ_LE_UINT16(bx + 12) & 15) | (READ_LE_UINT16(bx + 6) << 4));
		writeRegister(0x80 + carrierOperator, (READ_LE_UINT16(bx + 14) & 15) | (READ_LE_UINT16(bx + 8) << 4));
		writeRegister(0xE0 + carrierOperator, arg0[3] & 3); /* original writes to 0xE, typo */
	}

	void adlibPercussionKeyOn(int channel, int var6, int note) {
		// 6: Bass Drum
		// 7: Snare Drum
		// 8: Tom-Tom
		// 9: Cymbal
		// 10: Hi-Hat
		assert(channel >= 6 && channel <= 10);
		_regBD &= ~(1 << (10 - channel));
		writeRegister(0xBD, _regBD);
		int hw_channel = channel;
		if (channel == 9) {
			hw_channel = 8;
		} else if (channel == 10) {
			hw_channel = 7;
		}
		const uint16_t freq = _adlibFrequencyTable[note % 12];
		writeRegister(0xA0 + hw_channel, freq);
		const uint8_t regBx = ((note / 12) << 2) | ((freq & 0x300) >> 8);
		writeRegister(0xB0 + hw_channel, regBx);
		_regBD |= 1 << (10 - channel);
		writeRegister(0xBD, _regBD);
	}

	void adlibMelodicKeyOn(int channel, int volume, int note) {
		writeRegister(0xB0 + channel, 0);
		const uint16_t freq = _adlibFrequencyTable[note % 12];
		writeRegister(0xA0 + channel, freq & 0xFF); /* Frequency (Low) */
		const uint8_t octave = (note / 12);
		const uint8_t regBx = 0x20 | (octave << 2) | ((freq & 0x300) >> 8); /* Key-On | Octave | Frequency (High) */
		writeRegister(0xB0 + channel, regBx);
	}

	void adlibMelodicKeyOff(int channel, int note) {
		const uint16_t freq = _adlibFrequencyTable[note % 12];
		const uint8_t octave = (note / 12);
		const uint8_t regBx = (octave << 2) | ((freq & 0x300) >> 8); /* Octave | Frequency (High) */
		writeRegister(0xB0 + channel, regBx);
	}

	void adlibPercussionKeyOff(int channel) {
		assert(channel >= 6 && channel <= 10);
		_regBD &= ~(1 << (10 - channel));
		writeRegister(0xBD, _regBD);
	}

	void adlibPitchBend(int channel, int value) {
		//fprintf(stdout, "pitch bend channel:%d value:%d\n", channel, value);
		if (value == 0x2000) {
			return;
		}
		if (value & 0x2000) {
			value = -(value & 0x1FFF);
		}
		const int pitchBend = value;
		for (int i = 0; i < NUM_CHANNELS; ++i) {
			if (_adlibPlayingNoteTable[i].midi_channel != channel) {
				continue;
			}
			const int note = _adlibPlayingNoteTable[i].note;
			const int hw_channel = _adlibPlayingNoteTable[i].hw_channel;
			static const int kVarC = 0x2000; // / byte_1B73_2BE66;
			value = pitchBend / kVarC + note % 12;
			int octave = note / 12;
			if (value < 0) {
				--octave;
				value = -value;
			} else if (value > 12) {
				++octave;
				value -= 12;
			}
			--octave;
			const int var14 = ((pitchBend % kVarC) * 25) / kVarC;
			const uint16_t freq = var14 + _adlibFrequencyTable[value % 12];
			writeRegister(0xA0 + hw_channel, freq & 0xFF);
			const uint8_t regBx = 0x20 | (octave << 2) | ((freq & 0x300) >> 8);
			writeRegister(0xB0 + hw_channel, regBx);
		}
	}

	void adlibReset() {
		for (int i = 0; i < 6; ++i) {
			adlibMelodicKeyOff(i, 0);
		}
		for (int i = 6; i < 11; ++i) {
			adlibPercussionKeyOff(i);
		}
		writeRegister(8, 0x40);
		for (int i = 0; i < 18; ++i) {
			writeRegister(_adlibOperatorTable[i] + 0x40, 0x3F);
		}
		for (int i = 0; i < 9; ++i) {
			writeRegister(0xB0 + i, 0);
		}
		for (int i = 0; i < 9; ++i) {
			writeRegister(0xC0 + i, 0);
		}
		for (int i = 0; i < 18; ++i) {
			writeRegister(_adlibOperatorTable[i] + 0x60, 0);
		}
		for (int i = 0; i < 18; ++i) {
			writeRegister(_adlibOperatorTable[i] + 0x80, 0);
		}
		for (int i = 0; i < 18; ++i) {
			writeRegister(_adlibOperatorTable[i] + 0x20, 0);
		}
		for (int i = 0; i < 18; ++i) {
			writeRegister(_adlibOperatorTable[i] + 0xE0, 0);
		}
		writeRegister(1, 0x20);
		writeRegister(1, 0);
		for (int i = 0; i < 9; ++i) {
			writeRegister(0xB0 + i, 0);
			writeRegister(0xA0 + i, 0);
		}
		writeRegister(0xBD, 0);
		_regBD = 0x20;
	}

	void writeRegister(uint8_t port, uint8_t value) {
		//fprintf(stdout, "0x%02x:0x%02x\n", port, value);
		OPL3_WriteReg(&_opl3, port, value);
	}
};

static MidiDriver *createMidiDriver() {
	return new MidiDriver_adlib;
}

const MidiDriverInfo midi_driver_adlib = {
	"Adlib",
	createMidiDriver
};
