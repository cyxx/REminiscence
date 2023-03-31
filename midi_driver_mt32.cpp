
#include <stdio.h>
#include <stdint.h>
#include "midi_driver.h"

#define MT32EMU_API_TYPE 1
#include <mt32emu.h>

static const bool _isCM32L = false;

static const char *CM32L_CONTROL_FILENAME = "CM32L_CONTROL.ROM";
static const char *CM32L_PCM_FILENAME = "CM32L_PCM.ROM";
static const char *MT32_CONTROL_FILENAME = "MT32_CONTROL.ROM";
static const char *MT32_PCM_FILENAME = "MT32_PCM.ROM";

static uint32_t msg(int cmd, int param1 = 0, int param2 = 0) {
	return (param2 << 16) | (param1 << 8) | cmd;
}

struct MidiDriver_mt32emu: MidiDriver {
	virtual int init() {
		mt32emu_report_handler_i report = { 0 };
		_context = mt32emu_create_context(report, this);
		const char *control = _isCM32L ? CM32L_CONTROL_FILENAME : MT32_CONTROL_FILENAME;
		if (mt32emu_add_rom_file(_context, control) != MT32EMU_RC_ADDED_CONTROL_ROM) {
			fprintf(stdout, "WARNING: Failed to add MT32 rom file '%s'", control);
			return -1;
		}
		const char *pcm = _isCM32L ? CM32L_PCM_FILENAME : MT32_PCM_FILENAME;
		if (mt32emu_add_rom_file(_context, pcm) != MT32EMU_RC_ADDED_PCM_ROM) {
			fprintf(stdout, "WARNING: Failed to add MT32 rom file '%s'", pcm);
			return -1;
		}
		mt32emu_rom_info romInfo;
		mt32emu_get_rom_info(_context, &romInfo);
		fprintf(stdout, "mt32emu version %s\n", mt32emu_get_library_version_string());
		//fprintf(stdout, "control rom %s %s\n", romInfo.control_rom_id, romInfo.control_rom_description);
		//fprintf(stdout, "pcm rom %s %s\n", romInfo.pcm_rom_id, romInfo.pcm_rom_description);

		mt32emu_set_midi_delay_mode(_context, MT32EMU_MDM_IMMEDIATE);
		return 0;
	}
	virtual void fini() {
		if (mt32emu_is_open(_context)) {
			mt32emu_close_synth(_context);
		}
		mt32emu_free_context(_context);
	}
	virtual void reset(int rate) {
		mt32emu_set_stereo_output_samplerate(_context, rate);
		if (mt32emu_is_open(_context)) {
			mt32emu_close_synth(_context);
		}
		mt32emu_open_synth(_context);
	}

	virtual void noteOff(int channel, int note, int velocity) {
		mt32emu_play_msg(_context, msg(0x80 | channel, note, velocity));
	}
	virtual void noteOn(int channel, int note, int velocity) {
		mt32emu_play_msg(_context, msg(0x90 | channel, note, velocity));
	}
	virtual void controlChange(int channel, int type, int value) {
		mt32emu_play_msg(_context, msg(0xB0 | channel, type, value));
	}
	virtual void programChange(int channel, int num) {
		mt32emu_play_msg(_context, msg(0xC0 | channel, num));
	}
	virtual void pitchBend(int channel, int lsb, int msb) {
		mt32emu_play_msg(_context, msg(0xE0 | channel, lsb, msb));
	}

	virtual void readSamples(int16_t *samples, int len) {
		mt32emu_render_bit16s(_context, samples, len);
	}

	mt32emu_context _context;
};

static MidiDriver *createMidiDriver() {
	return new MidiDriver_mt32emu;
}

const MidiDriverInfo midi_driver_mt32 = {
	"MT32",
	createMidiDriver
};
