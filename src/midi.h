#pragma once
#if !defined(MIDI_H)
#	define MIDI_H

#	include <functional>
#	include <string>

struct midi_port {
	uint16_t api;
	int16_t  port_number;

	bool operator==(const midi_port &rhs) const;
	bool operator!=(const midi_port &rhs) const;

	operator uint32_t() const;

	uint32_t as_uint32() const;
	static midi_port from_uint32(uint32_t port);
};
static_assert(sizeof(midi_port) == sizeof(uint32_t));

extern const midi_port INVALID_MIDI_PORT;

void midi_init();
void midi_process();

void midi_open_port(const midi_port &port);
void midi_close_port(const midi_port &port);

void midi_set_psg_channel_controller(int psg_channel, midi_port port);
void midi_toggle_channel_control(int psg_channel, midi_port port, int midi_channel);

bool midi_get_port_name(midi_port midi_port, std::string &name);

void midi_for_each_psg_channel(std::function<void(int, midi_port, uint16_t)> fn);
void midi_for_each_port(std::function<void(midi_port, const std::string &)> fn);

void midi_set_logging(bool enable);
bool midi_logging_is_enabled();

#endif
