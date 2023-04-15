
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "midi_parser.h"
#include "util.h"

void MidiTrack::rewind() {
	endOfTrack = false;
	offset = 0;
}

static uint32_t readVLQ(const uint8_t *data, uint32_t& offset) {
	uint32_t value = 0;
	for (int i = 0; i < 4; ++i) {
		const uint8_t b = data[offset++];
		value = (value << 7) | (b & 0x7F);
		if ((b & 0x80) == 0) {
			break;
		}
	}
	return value;
}

const MidiEvent *MidiTrack::nextEvent() {
	while (offset < size) {
		MidiEvent &ev = event;
		ev.timestamp = readVLQ(data, offset);
		ev.command = data[offset++];
		switch (ev.command & 0xF0) {
		case MIDI_COMMAND_NOTE_OFF:
		case MIDI_COMMAND_NOTE_ON:
			ev.param1 = data[offset++];
			ev.param2 = data[offset++];
			debug(DBG_MIDI, "Note %s: note %d vel %d timestamp %d", (((ev.command & 0xF0) == MIDI_COMMAND_NOTE_OFF) ? "Off" : "On"), ev.param1, ev.param2, ev.timestamp);
			break;
		case MIDI_COMMAND_PROGRAM_CHANGE:
			ev.param1 = data[offset++];
			debug(DBG_MIDI, "Program %d timestamp %d", ev.param1, ev.timestamp);
			break;
		case MIDI_COMMAND_CHANNEL_PRESSURE:
			ev.param1 = data[offset++];
			debug(DBG_MIDI, "Channel pressure %d timestamp %d", ev.param1, ev.timestamp);
			break;
		case MIDI_COMMAND_PITCH_BEND:
			ev.param1 = data[offset++];
			ev.param2 = data[offset++];
			debug(DBG_MIDI, "Pitch Bend %d,%d", ev.param1, ev.param2);
			break;
		case MIDI_COMMAND_SYSEX:
			if (ev.command == 0xFF) {
				const int num = data[offset++];
				const int len = readVLQ(data, offset);
				offset += len;
				debug(DBG_MIDI, "SysEx timestamp %d", ev.timestamp);
				assert(ev.timestamp == 0);
				if (num == MIDI_SYSEX_END_OF_TRACK) {
					endOfTrack = true;
					return 0;
				}
				continue;
			}
			/* fall-through */
		default:
			warning("Unhandled MIDI command 0x%x", ev.command);
			break;
		}
		return &event;
	}
	return 0;
}

MidiParser::MidiParser() {
	memset(_tracks, 0, sizeof(_tracks));
	_tracksCount = 0;
}

void MidiParser::loadMid(File *f) {
	for (int i = 0; i < MAX_TRACKS; ++i) {
		free(_tracks[i].data);
		_tracks[i].data = 0;
	}
	memset(_tracks, 0, sizeof(_tracks));
	_tracksCount = 0;
	char tag[4];
	f->read(tag, 4);
	assert(memcmp(tag, "MThd", 4) == 0);
	const uint32_t len = f->readUint32BE();
	assert(len == 6);
	f->readByte();
	const int type = f->readByte();
	const int tracksCount = f->readUint16BE();
	assert(tracksCount <= MAX_TRACKS);
	const int ppqn = f->readUint16BE();
	debug(DBG_MIDI, "MThd type %d tracks %d ppqn %d len %d", type, tracksCount, ppqn, len);
	for (int i = 0; i < tracksCount; ++i) {
		f->read(tag, 4);
		assert(memcmp(tag, "MTrk", 4) == 0);
		const uint32_t len = f->readUint32BE();
		debug(DBG_MIDI, "Track #%d len %d", i, len);
		MidiTrack *track = &_tracks[i];
		track->data = (uint8_t *)malloc(len);
		if (!track->data) {
			warning("Failed to allocate %d bytes for MIDI track %d", len, i);
		} else {
			track->endOfTrack = false;
			track->offset = 0;
			track->size = len;
			f->read(track->data, len);
		}
	}
	_tracksCount = tracksCount;
}
