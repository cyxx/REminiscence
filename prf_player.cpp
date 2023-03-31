
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#ifdef _WIN32
#define MIDI_DRIVER_SYMBOL __declspec(dllimport)
#else
#define MIDI_DRIVER_SYMBOL
#endif
#include "midi_driver.h"
#include "midi_parser.h"
#include "mixer.h"
#include "prf_player.h"
#include "util.h"

static const struct {
	int mode;
	int hz;
	const MidiDriverInfo *info;
} _midiDrivers[] = {
#ifdef USE_MIDI_DRIVER
	{ MODE_ADLIB, TIMER_ADLIB_HZ, &midi_driver_adlib },
	{ MODE_MT32, TIMER_MT32_HZ, &midi_driver_mt32 },
#endif
	{ -1, 0, 0 }
};

static const int kMusicVolume = 63;

PrfPlayer::PrfPlayer(Mixer *mix, FileSystem *fs, int mode)
	: _playing(false), _mixer(mix), _fs(fs), _mode(mode), _driver(0) {
	for (int i = 0; _midiDrivers[i].info; ++i) {
		if (_midiDrivers[i].mode == mode) {
			_timerHz = _midiDrivers[i].hz;
			_driver = _midiDrivers[i].info->create();
			assert(_driver);
			if (_driver->init() < 0) {
				warning("Failed to initialize MIDI driver %s", _midiDrivers[i].info->name);
				_driver = 0;
			}
			return;
		}
	}
	fprintf(stdout, "WARNING: no midi driver for mode %d", mode);
}

PrfPlayer::~PrfPlayer() {
	if (_driver) {
		_driver->fini();
		_driver = 0;
	}
}

void PrfPlayer::play(int num) {
	_playing = false;
	memset(&_prfData, 0, sizeof(_prfData));
	memset(&_tracks, 0, sizeof(_tracks));
	if (num < _namesCount) {
		char name[64];
		snprintf(name, sizeof(name), "%s.prf", (num == 1 && _mode == MODE_MT32) ? "opt" : _names[num]);
		File f;
		if (f.open(name, "rb", _fs)) {
			loadPrf(&f);
			if (_mode == MODE_ADLIB) {
				for (int i = 0; i < 16; ++i) {
					memset(_adlibInstrumentData[i], 0, ADLIB_INSTRUMENT_DATA_LEN);
					if (_prfData.instruments[i][0]) {
						snprintf(name, sizeof(name), "%s.INS", _prfData.instruments[i]);
						if (f.open(name, "rb", _fs)) {
							loadIns(&f, i);
						} else {
							warning("Unable to open '%s'", name);
						}
					}
				}
			}
			if (f.open(_prfData.midi, "rb", _fs)) {
				_parser.loadMid(&f);
				play();
			}
		}
	}
}

void PrfPlayer::loadPrf(File *f) {
	for (int i = 0; i < 16; ++i) {
		f->read(_prfData.instruments[i], INSTRUMENT_NAME_LEN);
		const int len = strlen(_prfData.instruments[i]);
		if (len > 8) {
			_prfData.instruments[i][8] = 0;
			debug(DBG_PRF, "Truncating instrument name to '%s'", _prfData.instruments[i]);
		}
	}
	for (int i = 0; i < 16; ++i) {
		_prfData.adlibNotes[i] = f->readUint16LE();
	}
	for (int i = 0; i < 16; ++i) {
		_prfData.adlibVelocities[i] = f->readUint16LE();
	}
	_prfData.timerTicks = f->readUint32LE();
	_prfData.timerMod = f->readUint16LE();
	f->read(_prfData.midi, MIDI_FILENAME_LEN);
	_prfData.adlibDoNotesLookup = f->readUint16LE();
	for (int i = 0; i < 16; ++i) {
		_prfData.adlibPrograms[i] = f->readUint16LE();
	}
	for (int i = 0; i < 16; ++i) {
		_prfData.mt32Programs[i] = f->readUint16LE();
	}
	for (int i = 0; i < 16; ++i) {
		_prfData.mt32Velocities[i] = f->readUint16LE();
	}
	for (int i = 0; i < 16; ++i) {
		_prfData.mt32Notes[i] = f->readUint16LE();
	}
	for (int i = 0; i < 16; ++i) {
		_tracks[i].hw_channel_num = f->readByte();
	}
	for (int i = 0; i < 16; ++i) {
		_tracks[i].mt32_program_num = f->readByte();
	}
	for (int i = 0; i < 16; ++i) {
		_tracks[i].loop_flag = f->readByte();
	}
	_prfData.totalDurationTicks = f->readUint32LE();
	_prfData.mt32DoChannelsLookup = f->readByte();
	assert(f->tell() == 0x2F1);
	debug(DBG_PRF, "duration %d timer %d", _prfData.totalDurationTicks, _prfData.timerTicks);
}

void PrfPlayer::loadIns(File *f, int i) {
	uint8_t *p = _adlibInstrumentData[i];
	p[0] = f->readByte();
	p[1] = f->readByte();
	f->read(&p[6], 26 * 2);
	f->seek(54 + 20);
	p[2] = f->readByte();
	f->readByte();
	p[3] = f->readByte();
	f->readByte();
	if (p[2] != 0 || p[3] != 0) {
		debug(DBG_PRF, "Wave Select Register 0x%02x 0x%02x was ignored", p[2], p[3]);
	}
	p[4] = 0;
	p[5] = 0;
	const int unk = f->readUint16LE();
	assert(unk == 1);
	assert(f->tell() == 80);
}

void PrfPlayer::play() {
	if (!_driver) {
		return;
	}
	_driver->reset(_mixer->getSampleRate());
	for (int i = 0; i < _parser._tracksCount; ++i) {
		_tracks[i].instrument_num = i;
		if (_mode == MODE_MT32) {
			mt32ProgramChange(i, _tracks[i].mt32_program_num);
			mt32ControlChangeResetRPN(i);
		}
	}
	for (int i = 0; i < 16; ++i) {
		_tracks[i].instrument_num = i;
	}
	for (int i = 0; i < _parser._tracksCount; ++i) {
		PrfTrack *current_track = &_tracks[i];
		MidiTrack *track = &_parser._tracks[i];
		MidiEvent ev = track->events.front();
		current_track->counter = ev.timestamp;
	}
	_timerTick = _musicTick = 0;
	_samplesLeft = 0;
	_samplesPerTick = _mixer->getSampleRate() / _timerHz;
	_playing = true;
	_mixer->setPremixHook(mixCallback, this);
}

void PrfPlayer::stop() {
	if (_playing) {
		_mixer->setPremixHook(0, 0);
		_playing = false;
	}
}

void PrfPlayer::mt32NoteOn(int track, int note, int velocity) {
	if (_prfData.mt32DoChannelsLookup != 0 && _tracks[track].hw_channel_num == 9) {
		note = _prfData.mt32Notes[_tracks[track].instrument_num];
	}
	_driver->noteOn(_tracks[track].hw_channel_num, note, (velocity * kMusicVolume) >> 6);
}

void PrfPlayer::mt32NoteOff(int track, int note, int velocity) {
	if (_prfData.mt32DoChannelsLookup != 0 && _tracks[track].hw_channel_num == 9) {
		note = _prfData.mt32Notes[_tracks[track].instrument_num];
	}
	_driver->noteOff(_tracks[track].hw_channel_num, note, (velocity * kMusicVolume) >> 6);
}

void PrfPlayer::mt32ProgramChange(int track, int num) {
	_driver->programChange(_tracks[track].hw_channel_num, num);
}

void PrfPlayer::mt32PitchBend(int track, int lsb, int msb) {
	_driver->pitchBend(_tracks[track].hw_channel_num, lsb, msb);
}

static const uint8_t MIDI_CONTROLLER_RPN_LSB = 0x64;
static const uint8_t MIDI_CONTROLLER_RPN_MSB = 0x65;
static const uint8_t MIDI_CONTROLLER_DATA_ENTRY_LSB = 0x26;
static const uint8_t MIDI_CONTROLLER_DATA_ENTRY_MSB = 0x06;

void PrfPlayer::mt32ControlChangeResetRPN(int track) {
	_driver->controlChange(_tracks[track].hw_channel_num, MIDI_CONTROLLER_RPN_MSB, 0);
	_driver->controlChange(_tracks[track].hw_channel_num, MIDI_CONTROLLER_RPN_LSB, 0);
	_driver->controlChange(_tracks[track].hw_channel_num, MIDI_CONTROLLER_DATA_ENTRY_MSB, 1);
	_driver->controlChange(_tracks[track].hw_channel_num, MIDI_CONTROLLER_DATA_ENTRY_LSB, 0);
}

void PrfPlayer::adlibNoteOn(int track, int note, int velocity) {
	const int instrument_num = _tracks[track].instrument_num;
	_driver->setInstrumentData(track, instrument_num, _adlibInstrumentData[instrument_num]);
	_driver->noteOn(track, note, velocity);
}

void PrfPlayer::adlibNoteOff(int track, int note, int velocity) {
	const int instrument_num = _tracks[track].instrument_num;
	_driver->setInstrumentData(track, instrument_num, _adlibInstrumentData[instrument_num]);
	_driver->noteOff(track, note, velocity);
}

void PrfPlayer::handleTick() {
	for (int i = 0; i < _parser._tracksCount; ++i) {
		MidiTrack *track = &_parser._tracks[i];
		if (track->events.size() == 0) {
			continue;
		}
		const int track_index = i;
		PrfTrack *current_track = &_tracks[i];
		if (current_track->counter != 0) {
			--current_track->counter;
			continue;
		}
next_event:
		MidiEvent ev = track->events.front();
		track->events.pop();
		if (current_track->loop_flag) {
			track->events.push(ev);
		}
		switch (ev.command & 0xF0) {
		case MIDI_COMMAND_NOTE_OFF: {
				int note = ev.param1;
				int velocity = ev.param2;
				if (_mode == MODE_MT32) {
					note += _prfData.mt32Notes[current_track->instrument_num];
					velocity += _prfData.mt32Velocities[current_track->instrument_num];
					if (velocity < 0) {
						velocity = 0;
					} else if (velocity > 127) {
						velocity = 127;
					}
					mt32NoteOff(track_index, note, velocity);
				} else {
					if (_prfData.adlibDoNotesLookup) {
						note += _prfData.adlibNotes[current_track->instrument_num];
					} else {
						note += _prfData.adlibNotes[track_index];
					}
					if (_prfData.adlibDoNotesLookup) {
						velocity += _prfData.adlibVelocities[current_track->instrument_num];
					} else {
						velocity += _prfData.adlibVelocities[track_index];
					}
					if (velocity < 0) {
						velocity = 0;
					} else if (velocity > 127) {
						velocity = 127;
					}
					adlibNoteOff(track_index, note, velocity);
				}
			}
			break;
		case MIDI_COMMAND_NOTE_ON: {
				int note = ev.param1;
				int velocity = ev.param2;
				if (_mode == MODE_MT32) {
					note += _prfData.mt32Notes[current_track->instrument_num];
					if (velocity == 0) {
						mt32NoteOff(track_index, note, velocity);
						break;
					}
					velocity += _prfData.mt32Velocities[current_track->instrument_num];
					if (velocity < 0) {
						velocity = 0;
					} else if (velocity > 127) {
						velocity = 127;
					}
					mt32NoteOn(track_index, note, velocity);
				} else {
					assert(_mode == MODE_ADLIB);
					if (_prfData.adlibDoNotesLookup) {
						note += _prfData.adlibNotes[current_track->instrument_num];
					} else {
						note += _prfData.adlibNotes[track_index];
					}
					if (velocity == 0) {
						adlibNoteOff(track_index, note, velocity);
						break;
					}
					if (_prfData.adlibDoNotesLookup) {
						velocity += _prfData.adlibVelocities[current_track->instrument_num];
					} else {
						velocity += _prfData.adlibVelocities[track_index];
					}
					if (velocity < 0) {
						velocity = 0;
					} else if (velocity > 127) {
						velocity = 127;
					}
					adlibNoteOn(track_index, note, velocity);
				}
			}
			break;
		case MIDI_COMMAND_PROGRAM_CHANGE: {
				if (_mode == MODE_MT32) {
					for (int num = 0; num < 16; ++num) {
						if (_prfData.mt32Programs[num] == ev.param1) {
							mt32ProgramChange(track_index, _tracks[num].mt32_program_num);
							break;
						}
					}
				} else {
					assert(_mode == MODE_ADLIB);
					if (_prfData.adlibDoNotesLookup) {
						for (int num = 0; num < 16; ++num) {
							if (_prfData.adlibPrograms[num] == ev.param1) {
								current_track->instrument_num = num;
								break;
							}
						}
					}
				}
			}
			break;
		case MIDI_COMMAND_CHANNEL_PRESSURE:
			break;
		case MIDI_COMMAND_PITCH_BEND: {
				if (_mode == MODE_MT32) {
					mt32PitchBend(track_index, ev.param1, ev.param2);
				} else {
					assert(_mode == MODE_ADLIB);
					_driver->pitchBend(track_index, ev.param1, ev.param2);
				}
			}
			break;
		default:
			warning("Unhandled MIDI event command 0x%x", ev.command);
			break;
		}
		if (track->events.size() != 0) {
			ev = track->events.front();
			current_track->counter = ev.timestamp;
			if (current_track->counter == 0) {
				goto next_event;
			}
			--current_track->counter;
		}
	}
}

bool PrfPlayer::end() const {
	return (_prfData.totalDurationTicks != 0) && _musicTick > _prfData.totalDurationTicks;
}

int PrfPlayer::readSamples(int16_t *samples, int count) {
	const int total = count;
	while (count != 0) {
		if (_samplesLeft == 0) {
			//++_timerTick;
			//if (_timerTick == _prfData.timerTicks) {
				//fprintf(stdout, "musicTick #%d of %d\n", _musicTick, _prfData.totalDurationTicks);
				handleTick();
				//_timerTick = 0;
				if (_prfData.totalDurationTicks != 0) {
					++_musicTick;
					if (_musicTick == _prfData.totalDurationTicks + 1) {
						debug(DBG_PRF, "End of music");
						//break;
					}
				}
			//}
			_samplesLeft = _samplesPerTick * _prfData.timerTicks;
		}
		const int len = (count < _samplesLeft) ? count : _samplesLeft;
		_driver->readSamples(samples, len);
		_samplesLeft -= len;
		count -= len;
		samples += len * 2;
	}
	return total - count;
}

bool PrfPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((PrfPlayer *)param)->mix(buf, len);
}

bool PrfPlayer::mix(int16_t *buf, int len) {
	int16_t *p = (int16_t *)alloca(len * sizeof(int16_t) * 2);
	if (p) {
		const int count = readSamples(p, len);
		/* stereo to mono */
		for (int i = 0; i < count; ++i) {
			const int16_t l = *p++;
			const int16_t r = *p++;
			buf[i] = CLIP((l + r) / 2, -32768, 32767);
		}
		return count != 0;
	}
	return false;
}
