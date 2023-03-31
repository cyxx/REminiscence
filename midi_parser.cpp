
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "midi_parser.h"
#include "util.h"

static uint32_t readVLQ(File *f) {
	uint32_t value = 0;
	for (int i = 0; i < 4; ++i) {
		const uint8_t b = f->readByte();
		value = (value << 7) | (b & 0x7F);
		if ((b & 0x80) == 0) {
			break;
		}
	}
	return value;
}

static void loadTrk(File *f, int len, MidiTrack &track) {
	const uint32_t end = f->tell() + len;
	while (f->tell() < end) {
		MidiEvent ev;
		ev.timestamp = readVLQ(f);
		ev.command = f->readByte();
		switch (ev.command & 0xF0) {
		case MIDI_COMMAND_NOTE_OFF:
		case MIDI_COMMAND_NOTE_ON:
			ev.param1 = f->readByte();
			ev.param2 = f->readByte();
			//fprintf(stdout, "\t Note %s: note %d vel %d timestamp %d\n", (((ev.command & 0xF0) == MIDI_COMMAND_NOTE_OFF) ? "Off" : "On"), ev.param1, ev.param2, ev.timestamp);
			break;
		case MIDI_COMMAND_PROGRAM_CHANGE:
			ev.param1 = f->readByte();
			//fprintf(stdout, "\t Program %d timestamp %d\n", ev.param1, ev.timestamp);
			break;
		case MIDI_COMMAND_CHANNEL_PRESSURE:
			ev.param1 = f->readByte();
			//fprintf(stdout, "\t Channel pressure %d timestamp %d\n", ev.param1, ev.timestamp);
			break;
		case MIDI_COMMAND_PITCH_BEND:
			ev.param1 = f->readByte();
			ev.param2 = f->readByte();
			//fprintf(stdout, "\t Pitch Bend %d,%d\n", ev.param1, ev.param2);
			break;
		case MIDI_COMMAND_SYSEX:
			if (ev.command == 0xFF) {
				f->readByte(); /* type */
				const int len = readVLQ(f);
				f->seek(f->tell() + len);
				//fprintf(stdout, "\t SysEx timestamp %d\n", ev.timestamp);
				assert(ev.timestamp == 0);
				continue;
			}
			/* fall-through */
		default:
			warning("Unhandled MIDI command 0x%x", ev.command);
			break;
		}
		track.events.push(ev);
	}
}

void MidiParser::loadMid(File *f) {
	for (int i = 0; i < MAX_TRACKS; ++i) {
		_tracks[i].events = std::queue<MidiEvent>();
	}
	_tracksCount = 0;
	char tag[4];
	f->read(tag, 4);
	assert(memcmp(tag, "MThd", 4) == 0);
	const uint32_t len = f->readUint32BE();
	assert(len == 6);
	f->readByte();
	/* const int type = */ f->readByte();
	const int tracksCount = f->readUint16BE();
	assert(tracksCount <= MAX_TRACKS);
	/* const int ppqn = */ f->readUint16BE();
	//fprintf(stdout, "MThd type %d tracks %d ppqn %d len %d\n", type, tracksCount, ppqn, len);
	for (int i = 0; i < tracksCount; ++i) {
		f->read(tag, 4);
		assert(memcmp(tag, "MTrk", 4) == 0);
		const uint32_t len = f->readUint32BE();
		//fprintf(stdout, "Track #%d len %d\n", i, len);
		loadTrk(f, len, _tracks[i]);
		//fprintf(stdout, "Track #%d events %ld\n", i, track.events.size());
	}
	_tracksCount = tracksCount;
}
