
#ifndef PRF_PLAYER_H__
#define PRF_PLAYER_H__

#include <stdint.h>
#include "midi_parser.h"

static const int TIMER_ADLIB_HZ = 2082;
static const int TIMER_MT32_HZ  = 2242;

struct MidiDriver;

#define INSTRUMENT_NAME_LEN 30
#define MIDI_FILENAME_LEN   20

struct PrfData {
	char instruments[16][INSTRUMENT_NAME_LEN];
	int16_t adlibNotes[16];
	int16_t adlibVelocities[16];
	uint32_t timerTicks;
	uint16_t timerMod;
	char midi[MIDI_FILENAME_LEN];
	uint16_t adlibDoNotesLookup;
	int16_t adlibPrograms[16];
	int16_t mt32Programs[16];
	int16_t mt32Velocities[16];
	int16_t mt32Notes[16];
	uint32_t totalDurationTicks;
	uint8_t mt32DoChannelsLookup;
};

struct PrfTrack {
	uint8_t instrument_num;
	uint32_t counter;
	uint32_t counter2;
	uint8_t hw_channel_num;
	uint8_t mt32_program_num;
	uint8_t loop_flag;
};

#define ADLIB_INSTRUMENT_DATA_LEN 58

struct AdlibInstrument {
	uint8_t mode;
	uint8_t channel_num;
	uint8_t modulator_wave_select;
	uint8_t carrier_wave_select;
	uint8_t unk4;
	uint8_t unk5;
	struct {
		uint16_t key_scaling; // 0
		uint16_t frequency_multiplier; // 2
		uint16_t feedback_strength; // 4
		uint16_t attack_rate; // 6
		uint16_t sustain_level; // 8
		uint16_t sustain_sound; // 10
		uint16_t decay_rate; // 12
		uint16_t release_rate; // 14
		uint16_t output_level; // 16
		uint16_t amplitude_vibrato; // 18
		uint16_t frequency_vibrato; // 20
		uint16_t envelope_scaling; // 22
		uint16_t frequency_modulation; // 24
	} op[2]; /* modulator, carrier */
};

enum {
	MODE_ADLIB,
	MODE_MT32,
};

struct File;
struct FileSystem;
struct Mixer;

struct PrfPlayer {
	static const char *_names[];
	static const int _namesCount;

	PrfPlayer(Mixer *mix, FileSystem *fs, int mode);
	~PrfPlayer();

	void play(int num);

	void loadPrf(File *f);
	void loadIns(File *f, int num);

	void play();
	void stop();

	void mt32NoteOn(int track, int note, int velocity);
	void mt32NoteOff(int track, int note, int velocity);
	void mt32ProgramChange(int track, int num);
	void mt32PitchBend(int track, int lsb, int msb);
	void mt32ControlChangeResetRPN(int track);

	void adlibNoteOn(int track, int note, int velocity);
	void adlibNoteOff(int track, int note, int velocity);

	void handleTick();

	bool end() const;
	int readSamples(int16_t *samples, int count);

	static bool mixCallback(void *param, int16_t *buf, int len);
	bool mix(int16_t *buf, int len);

	bool _playing;
	Mixer *_mixer;
	FileSystem *_fs;
	int _mode;
	int _timerHz;
	PrfTrack _tracks[16];
	MidiDriver *_driver;
	MidiParser _parser;
	PrfData _prfData;
	int _samplesLeft, _samplesPerTick;
	uint32_t _timerTick, _musicTick;
	uint8_t _adlibInstrumentData[16][ADLIB_INSTRUMENT_DATA_LEN];
};

#endif /* PRF_PLAYER_H__ */
