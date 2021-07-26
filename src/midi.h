#pragma once
#if !defined(MIDI_H)
#	define MIDI_H

#	include <functional>
#	include <string>

#	define MAX_MIDI_CHANNELS (16) // Comes from the 4-bit field in the midi message format

class midi_port_descriptor {
public:
	midi_port_descriptor();
	midi_port_descriptor(uint16_t, uint16_t);
	midi_port_descriptor(const midi_port_descriptor &src);
	midi_port_descriptor(uint32_t);

	operator uint32_t() const;

	uint16_t api;
	int16_t  port_number;
};
static_assert(sizeof(midi_port_descriptor) == sizeof(uint32_t));

extern const midi_port_descriptor INVALID_MIDI_PORT;

enum class midi_playback_device {
	none,
	vera_psg,
	ym2151
};

struct midi_psg_settings {
	uint8_t waveform = 0;
};

struct midi_ym_patch_entry {
	uint8_t addr;
	uint8_t value;
};

struct midi_ym2151_settings {
	midi_ym_patch_entry patch_bytes[256];
	int     patch_size = 0;
};

struct midi_channel_settings {
	struct {
		midi_psg_settings    psg;
		midi_ym2151_settings ym2151;
	} device;

	midi_playback_device playback_device = midi_playback_device::none;

	uint16_t pitch_bend = 8192;
	uint8_t  volume = 127;
	uint8_t  balance = 64;
	uint8_t  pan = 64;
	uint8_t  modulation = 127;

	bool use_velocity = false;
};

void midi_init();
void midi_process();

void midi_open_port(const midi_port_descriptor &port);
void midi_close_port(const midi_port_descriptor &port);

void midi_for_each_open_port(std::function<void(midi_port_descriptor, const std::string &)> fn);
void midi_for_each_port(std::function<void(midi_port_descriptor, const std::string &)> fn);

void midi_set_logging(bool enable);
bool midi_logging_is_enabled();

const char *midi_playback_device_name(midi_playback_device d);

const midi_channel_settings *midi_port_get_channel(midi_port_descriptor port, uint8_t channel);

void midi_port_set_channel_playback_device(midi_port_descriptor port, uint8_t channel, midi_playback_device d);
void midi_port_set_channel_use_velocity(midi_port_descriptor port, uint8_t channel, bool use_velocity);

void midi_port_set_channel_psg_waveform(midi_port_descriptor port, uint8_t channel, uint8_t waveform);
void midi_port_set_channel_ym2151_patch_byte(midi_port_descriptor port, uint8_t channel, uint8_t addr, uint8_t value);
void midi_port_get_channel_ym2151_patch(midi_port_descriptor port, uint8_t channel, uint8_t *bytes);

#endif
