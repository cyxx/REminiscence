
#ifndef MIDI_DRIVER_H__
#define MIDI_DRIVER_H__

struct MidiDriver {
	virtual ~MidiDriver() {};

	virtual int init() = 0;
	virtual void fini() = 0;

	virtual void reset(int rate) = 0;

	virtual void setInstrumentData(int channel, int num, const void *data) { // fmopl only
	}
	virtual void fixExRegisterAddress(bool state) { // fmopl only
	}

	virtual void noteOff(int channel, int note, int velocity) = 0;
	virtual void noteOn(int channel, int note, int velocity) = 0;
	virtual void controlChange(int channel, int type, int value) = 0;
	virtual void programChange(int channel, int num) = 0;
	virtual void pitchBend(int channel, int lsb, int msb) = 0;

	virtual void readSamples(short *samples, int len) = 0;
};

struct MidiDriverInfo {
	const char *name;
	MidiDriver *(*create)();
};

#ifndef MIDI_DRIVER_SYMBOL
#ifdef _WIN32
#define MIDI_DRIVER_SYMBOL __declspec(dllexport)
#else
#define MIDI_DRIVER_SYMBOL
#endif
#endif

MIDI_DRIVER_SYMBOL extern const MidiDriverInfo midi_driver_adlib;
MIDI_DRIVER_SYMBOL extern const MidiDriverInfo midi_driver_mt32;

#endif /* MIDI_DRIVER_H__ */
