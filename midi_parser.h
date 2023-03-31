
#ifndef MIDI_PARSER_H__
#define MIDI_PARSER_H__

#include <queue>
#include <stdint.h>

#define MIDI_COMMAND_NOTE_OFF         0x80
#define MIDI_COMMAND_NOTE_ON          0x90
#define MIDI_COMMAND_PROGRAM_CHANGE   0xC0
#define MIDI_COMMAND_CHANNEL_PRESSURE 0xD0 /* unused */
#define MIDI_COMMAND_PITCH_BEND       0xE0
#define MIDI_COMMAND_SYSEX            0xF0 /* unused */

struct MidiEvent {
	uint32_t timestamp;
	uint8_t command;
	uint8_t param1;
	uint8_t param2;
};

struct MidiTrack {
	std::queue<MidiEvent> events;
};

struct File;

#define MAX_TRACKS 16

struct MidiParser {
	void loadMid(File *f);

	int _tracksCount;
	MidiTrack _tracks[MAX_TRACKS];
};

#endif /* MIDI_PARSER_H__ */
