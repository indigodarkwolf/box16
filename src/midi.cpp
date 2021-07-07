#include "midi.h"

#include <cstring>
#include "math.h"
#include "RtMidi.h"

#include "vera/vera_psg.h"
#include <unordered_map>


#define MAX_MIDI_KEYS (128)

struct midi_channel {
	midi_port_descriptor port;
	uint8_t              channel;
};

#define INVALID_VOICE (0xff)

struct midi_key {
	uint8_t voice;
	uint8_t velocity;
};

struct midi_channel_state {
	midi_channel_settings settings;
	midi_key              keys_on[MAX_MIDI_KEYS];
};

struct open_midi_port {
	midi_port_descriptor descriptor;
	RtMidiIn *           controller;
	midi_channel_state   channels[MAX_MIDI_CHANNELS];
};

struct psg_midi_mapping {
	midi_channel channel;
};

const midi_port_descriptor INVALID_MIDI_PORT{ RtMidi::Api::NUM_APIS, 0xffff };
const midi_channel         INVALID_MIDI_CHANNEL{ INVALID_MIDI_PORT, 0xff };

static psg_midi_mapping Psg_midi_mappings[PSG_NUM_CHANNELS];

static RtMidiIn Midi_in_api;

static std::unordered_map<uint32_t, open_midi_port> Open_midi_ports;

static bool Show_midi_messages = false;

// Frequency table matching MIDI keys to PSG frequency settings.
// See also: /tools/generate_psg_frequency_table.cpp
static uint16_t Psg_frequency_table[MAX_MIDI_KEYS] = {
	21,
	23,
	24,
	26,
	27,
	29,
	31,
	32,
	34,
	36,
	39,
	41,
	43,
	46,
	49,
	52,
	55,
	58,
	62,
	65,
	69,
	73,
	78,
	82,
	87,
	93,
	98,
	104,
	110,
	117,
	124,
	131,
	139,
	147,
	156,
	165,
	175,
	186,
	197,
	208,
	221,
	234,
	248,
	263,
	278,
	295,
	312,
	331,
	351,
	372,
	394,
	417,
	442,
	468,
	496,
	526,
	557,
	590,
	625,
	662,
	702,
	744,
	788,
	835,
	884,
	937,
	993,
	1052,
	1114,
	1181,
	1251,
	1325,
	1404,
	1488,
	1576,
	1670,
	1769,
	1874,
	1986,
	2104,
	2229,
	2362,
	2502,
	2651,
	2809,
	2976,
	3153,
	3340,
	3539,
	3749,
	3972,
	4209,
	4459,
	4724,
	5005,
	5303,
	5618,
	5952,
	6306,
	6681,
	7078,
	7499,
	7945,
	8418,
	8918,
	9448,
	10010,
	10606,
	11236,
	11904,
	12612,
	13362,
	14157,
	14999,
	15891,
	16836,
	17837,
	18897,
	20021,
	21212,
	22473,
	23809,
	25225,
	26725,
	28314,
	29998,
	31782,
	33672,
};

// -------------------
//
// midi_port_descriptor
//
// -------------------

midi_port_descriptor::midi_port_descriptor()
    : api(INVALID_MIDI_PORT.api),
      port_number(INVALID_MIDI_PORT.port_number)
{
}

midi_port_descriptor::midi_port_descriptor(uint16_t src_api, uint16_t src_port_number)
    : api(src_api),
      port_number(src_port_number)
{
}

midi_port_descriptor::midi_port_descriptor(const midi_port_descriptor &src)
    : api(src.api),
      port_number(src.port_number)
{
}

midi_port_descriptor::midi_port_descriptor(uint32_t value)
    : api(reinterpret_cast<midi_port_descriptor *>(&value)->api),
      port_number(reinterpret_cast<midi_port_descriptor *>(&value)->port_number)
{
}

midi_port_descriptor::operator uint32_t() const
{
	return *reinterpret_cast<const uint32_t *>(this);
}

static bool operator==(const midi_port_descriptor &lhs, const midi_port_descriptor &rhs)
{
	return (lhs.api == rhs.api) && (lhs.port_number == rhs.port_number);
}

static bool operator!=(const midi_port_descriptor &lhs, const midi_port_descriptor &rhs)
{
	return (lhs.api != rhs.api) || (lhs.port_number != rhs.port_number);
}

// -------------------
//
// midi_channel
//
// -------------------

static bool operator==(const midi_channel &lhs, const midi_channel &rhs)
{
	return (lhs.channel == rhs.channel) && (lhs.port == rhs.port);
}

static bool operator!=(const midi_channel &lhs, const midi_channel &rhs)
{
	return (lhs.channel != rhs.channel) || (lhs.port != rhs.port);
}

// -------------------
//
// psg helpers
//
// -------------------

static uint8_t alloc_psg_voice()
{
	for (uint8_t i = 0; i < PSG_NUM_CHANNELS; ++i) {
		if (Psg_midi_mappings[i].channel == INVALID_MIDI_CHANNEL) {
			return i;
		}
	}
	return INVALID_VOICE;
}

// -------------------
//
// midi message helpers
//
// -------------------

static uint16_t get_bent_frequency(int keynum, int bend)
{
	if (bend == 8192) {
		return Psg_frequency_table[keynum];
	}

	uint32_t f0, f1;
	if (bend < 8192) {
		if (keynum <= 0) {
			return Psg_frequency_table[keynum];
		}
		f0 = Psg_frequency_table[keynum - 1];
		f1 = Psg_frequency_table[keynum];
	} else {
		if (keynum >= 127) {
			return Psg_frequency_table[keynum];
		}
		bend -= 8192;
		f0 = Psg_frequency_table[keynum];
		f1 = Psg_frequency_table[keynum + 1];
	}

	const uint32_t diff = f1 - f0;
	return (uint16_t)(f0 + ((diff * bend) >> 13));
}

static uint8_t get_velocitated_volume(int volume, int velocity)
{
	const int vv = (const int)sqrtf((float)(volume * velocity)); // max 7 bits
	return (uint8_t)(vv >> 1);
}

static void note_off(open_midi_port &port, uint8_t channel, int keynum, int velocity)
{
	if (Show_midi_messages) {
		printf("note off %d %d %d\n", channel, keynum, velocity);
	}

	midi_key &key = port.channels[channel].keys_on[keynum];
	if (key.voice != INVALID_VOICE) {
		switch (port.channels[channel].settings.playback_device) {
			case midi_playback_device::vera_psg:
				psg_set_channel_volume(key.voice, 0);
				break;
			case midi_playback_device::ym2151:
				// TODO: Implement me.
				break;
			default:
				break;
		}

		Psg_midi_mappings[key.voice].channel = INVALID_MIDI_CHANNEL;
	}

	key.voice    = INVALID_VOICE;
	key.velocity = 0;
}

static void note_on(open_midi_port &port, uint8_t channel, int keynum, int velocity)
{
	if (Show_midi_messages) {
		printf("note on %d %d %d\n", channel, keynum, velocity);
	}

	if (velocity == 0) {
		note_off(port, channel, keynum, velocity);
		return;
	}

	midi_channel_settings &settings = port.channels[channel].settings;
	midi_key &             key      = port.channels[channel].keys_on[keynum];
	switch (settings.playback_device) {
		case midi_playback_device::vera_psg:
			if (key.voice == INVALID_VOICE) {
				key.voice = alloc_psg_voice();
			}
			if (key.voice == INVALID_VOICE) {
				return;
			}
			Psg_midi_mappings[key.voice].channel = { port.descriptor, channel };
			psg_set_channel_frequency(key.voice, get_bent_frequency(keynum, settings.pitch_bend));
			psg_set_channel_waveform(key.voice, settings.device.psg.waveform);
			psg_set_channel_pulse_width(key.voice, (uint8_t)(settings.modulation >> 1));
			psg_set_channel_left(key.voice, settings.pan < 96);
			psg_set_channel_right(key.voice, settings.pan > 32);
			psg_set_channel_volume(key.voice, settings.use_velocity ? get_velocitated_volume(settings.volume, velocity) : (uint8_t)(settings.volume >> 1));
			break;
		case midi_playback_device::ym2151:
			// TODO: Implement me.
			break;
		default:
			break;
	}
}

static void polyphonic_key_pressure(open_midi_port &port, uint8_t channel, int keynum, int pressure)
{
	if (Show_midi_messages) {
		printf("polyphonic key pressure %d %d %d\n", channel, keynum, pressure);
	}
	// TODO: Write me.
}

static void control_change_modulation_wheel(open_midi_port &port, uint8_t channel, int controller_number, int controller_value)
{
	midi_channel_settings &settings = port.channels[channel].settings;
	switch (settings.playback_device) {
		case midi_playback_device::vera_psg:
			for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
				midi_channel port_channel{ port.descriptor, channel };
				if (Psg_midi_mappings[i].channel == port_channel) {
					psg_set_channel_pulse_width(i, (uint8_t)(controller_value >> 1));
				}
			}
			break;
		case midi_playback_device::ym2151:
			// TODO: Implement me.
			break;
		default:
			break;
	}
	settings.modulation = controller_value;
}

static void control_change_volume(open_midi_port &port, uint8_t channel, int controller_number, int controller_value)
{
	midi_channel_settings &settings = port.channels[channel].settings;
	switch (settings.playback_device) {
		case midi_playback_device::vera_psg:
			for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
				midi_channel port_channel{ port.descriptor, channel };
				if (Psg_midi_mappings[i].channel == port_channel) {
					psg_set_channel_volume(i, (uint8_t)(controller_value >> 1));
				}
			}
			break;
		case midi_playback_device::ym2151:
			// TODO: Implement me.
			break;
		default:
			break;
	}
	settings.volume = controller_value;
}

static void control_change_balance(open_midi_port &port, uint8_t channel, int controller_number, int controller_value)
{
	midi_channel_settings &settings = port.channels[channel].settings;
	switch (settings.playback_device) {
		case midi_playback_device::vera_psg:
			for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
				midi_channel port_channel{ port.descriptor, channel };
				if (Psg_midi_mappings[i].channel == port_channel) {
					psg_set_channel_left(i, settings.pan < 96);
					psg_set_channel_right(i, settings.pan > 32);
				}
			}
			break;
		case midi_playback_device::ym2151:
			// TODO: Implement me.
			break;
		default:
			break;
	}
	settings.volume = controller_value;
}

static void control_change(open_midi_port &port, uint8_t channel, int controller_number, int controller_value)
{
	if (Show_midi_messages) {
		printf("control change %d %d %d\n", channel, controller_number, controller_value);
	}

	switch (controller_number) {
		case 0x00: // bank select
			break;
		case 0x01: // modulation wheel
			control_change_modulation_wheel(port, channel, controller_number, controller_value);
			break;
		case 0x02: // Breath controller
			break;
		case 0x03: // Undefined
			break;
		case 0x04: // Foot controller
			break;
		case 0x05: // Portamento Time
			break;
		case 0x06: // Data entry MSB
			break;
		case 0x07: // Channel volume
			control_change_volume(port, channel, controller_number, controller_value);
			break;
		case 0x08: // Balance
			control_change_balance(port, channel, controller_number, controller_value);
			break;
		case 0x09: // Undefined
			break;
		case 0x0A: // Pan
			break;
		case 0x0B: // Expression Controller
			break;
		case 0x0C: // Effect control 1
			break;
		case 0x0D: // Effect control 2
			break;
		case 0x0E: // Undefined
			break;
		case 0x0F: // Undefined
			break;

		case 0x10: // General purpose 1
		case 0x11: // General purpose 2
		case 0x12: // General purpose 3
		case 0x13: // General purpose 4
		case 0x14: // Undefined
		case 0x15: // Undefined
		case 0x16: // Undefined
		case 0x17: // Undefined
		case 0x18: // Undefined
		case 0x19: // Undefined
		case 0x1A: // Undefined
		case 0x1B: // Undefined
		case 0x1C: // Undefined
		case 0x1D: // Undefined
		case 0x1E: // Undefined
		case 0x1F: // Undefined
			break;

		case 0x20: // LSB for controller 0
		case 0x21: // LSB for controller 1
		case 0x22: // LSB for controller 2
		case 0x23: // LSB for controller 3
		case 0x24: // LSB for controller 4
		case 0x25: // LSB for controller 5
		case 0x26: // LSB for controller 6
		case 0x27: // LSB for controller 7
		case 0x28: // LSB for controller 8
		case 0x29: // LSB for controller 9
		case 0x2A: // LSB for controller 10
		case 0x2B: // LSB for controller 11
		case 0x2C: // LSB for controller 12
		case 0x2D: // LSB for controller 13
		case 0x2E: // LSB for controller 14
		case 0x2F: // LSB for controller 15
			break;

		case 0x30: // LSB for controller 16
		case 0x31: // LSB for controller 17
		case 0x32: // LSB for controller 18
		case 0x33: // LSB for controller 19
		case 0x34: // LSB for controller 20
		case 0x35: // LSB for controller 21
		case 0x36: // LSB for controller 22
		case 0x37: // LSB for controller 23
		case 0x38: // LSB for controller 24
		case 0x39: // LSB for controller 25
		case 0x3A: // LSB for controller 26
		case 0x3B: // LSB for controller 27
		case 0x3C: // LSB for controller 28
		case 0x3D: // LSB for controller 29
		case 0x3E: // LSB for controller 30
		case 0x3F: // LSB for controller 31
			break;

		case 0x40: // Damper pedal (Sustain) value 0-63 off, 64-127 on
		case 0x41: // Portamento on/off
		case 0x42: // Sostenuto on/off
		case 0x43: // Soft pedal on/off
		case 0x44: // Legato Footswitch
		case 0x45: // Hold 2
		case 0x46: // Sound Controller 1 (default: Variation)
		case 0x47: // Sound Controller 2 (default: Timbre/Harmonic Content)
		case 0x48: // Sound Controller 3 (default: Release Time)
		case 0x49: // Sound Controller 4 (default: Attack Time)
		case 0x4A: // Sound Controller 5 (default: Brightness)
		case 0x4B: // Sound Controller 6
		case 0x4C: // Sound Controller 7
		case 0x4D: // Sound Controller 8
		case 0x4E: // Sound Controller 9
		case 0x4F: // Sound Controller 10
			break;

		case 0x50: // General purpose 5
		case 0x51: // General purpose 6
		case 0x52: // General purpose 7
		case 0x53: // General purpose 8
		case 0x54: // Portamento control
		case 0x55: // Undefined
		case 0x56: // Undefined
		case 0x57: // Undefined
		case 0x58: // High resolution velocity prefix
		case 0x59: // Undefined
		case 0x5A: // Undefined
		case 0x5B: // Effects 1 Depth (old: External Effects Depth)
		case 0x5C: // Effects 2 Depth (old: Tremolo Depth)
		case 0x5D: // Effects 3 Depth (old: Chrous Depth)
		case 0x5E: // Effects 4 Depth (old: Detune Depth)
		case 0x5F: // Effects 5 Depth (old: Phase Depth)
			break;

		case 0x60: // Data increment
		case 0x61: // Data decrement
		case 0x62: // Non-registered parameter number LSB
		case 0x63: // Non-registered parameter number LSB (MSB?)
		case 0x64: // Registered parameter number LSB
		case 0x65: // Registered parameter number MSB
		case 0x66: // Undefined
		case 0x67: // Undefined
		case 0x68: // Undefined
		case 0x69: // Undefined
		case 0x6A: // Undefined
		case 0x6B: // Undefined
		case 0x6C: // Undefined
		case 0x6D: // Undefined
		case 0x6E: // Undefined
		case 0x6F: // Undefined
			break;

		case 0x70: // Undefined
		case 0x71: // Undefined
		case 0x72: // Undefined
		case 0x73: // Undefined
		case 0x74: // Undefined
		case 0x75: // Undefined
		case 0x76: // Undefined
		case 0x77: // Undefined
			break;
		case 0x78: // All sound off
			break;
		case 0x79: // Reset all controllers
			break;
		case 0x7A: // Local control (controller_value 0 = off, 127 = on)
			break;
		case 0x7B: // All notes off
			break;
		case 0x7C: // Omni mode off
			break;
		case 0x7D: // Omni mode on
			break;
		case 0x7E: // Mono mode on (Poly mode off)
			break;
		case 0x7F: // Poly mode on (Mono mode off)
			break;
	}
}

static void program_change(open_midi_port &port, uint8_t channel, int program)
{
	if (Show_midi_messages) {
		printf("program change %d %d\n", channel, program);
	}
	// TODO: Write me.
}

static void channel_pressure(open_midi_port &port, uint8_t channel, int pressure)
{
	if (Show_midi_messages) {
		printf("channel pressure %d %d\n", channel, pressure);
	}
	// TODO: Write me.
}

static void pitch_bend(open_midi_port &port, uint8_t channel, int bend)
{
	if (Show_midi_messages) {
		printf("pitch bend %d %d\n", channel, bend);
	}

	midi_channel_settings &settings = port.channels[channel].settings;
	switch (settings.playback_device) {
		case midi_playback_device::vera_psg:
			for (int i = 0; i < MAX_MIDI_KEYS; ++i) {
				const midi_key &key = port.channels[channel].keys_on[i];
				if (key.voice != INVALID_VOICE) {
					psg_set_channel_frequency(key.voice, get_bent_frequency(i, bend));
				}
			}
			break;
		case midi_playback_device::ym2151:
			// TODO: Implement me.
			break;
		default:
			break;
	}
	settings.pitch_bend = bend;
}

static void parse_message(open_midi_port &port, const std::vector<unsigned char> &message)
{
	if ((message[0] & 0xF0) == 0xF0) {
		// All messages will be 0xFx
		switch (message[0] & 0x0f) {
			case 0x1: // MIDI timing code
				break;
			case 0x2: // Song position pointer
				break;
			case 0x3: // Song select
				break;
			case 0x6: // Tune request
				break;
			case 0x8: // Timing clock
				break;
			case 0xA: // Start sequence
				break;
			case 0xB: // Continue sequence
				break;
			case 0xC: // Stop sequence
				break;
			case 0xE: // Active sensing
				break;
			case 0xF: // System reset
				break;
		}
	} else {
		const uint8_t channel = message[0] & 0x0f;
		switch (message[0] & 0xf0) {
			case 0x80: // Note off
				note_off(port, channel, message[1], message[2]);
				break;
			case 0x90: // Note on
				note_on(port, channel, message[1], message[2]);
				break;
			case 0xA0: // Polyphonic key pressure
				polyphonic_key_pressure(port, channel, message[1], message[2]);
				break;
			case 0xB0: // Control change
				control_change(port, channel, message[1], message[2]);
				break;
			case 0xC0: // Program change
				program_change(port, channel, message[1]);
				break;
			case 0xD0: // Channel pressure
				channel_pressure(port, channel, message[1]);
				break;
			case 0xE0: // Pitch bend
				pitch_bend(port, channel, (message[2] << 7) | (message[1]));
				break;
		}
	}
}

// -------------------
//
// external API
//
// -------------------

void midi_init()
{
	for (uint8_t i = 0; i < PSG_NUM_CHANNELS; ++i) {
		Psg_midi_mappings[i].channel = INVALID_MIDI_CHANNEL;
	}
}

void midi_process()
{
	std::vector<unsigned char> message;
	for (auto &[port, open_port] : Open_midi_ports) {
		open_port.controller->getMessage(&message);
		while (message.size() > 0) {
			if (Show_midi_messages) {
				midi_port_descriptor desc(port);
				printf("midi [%d,%d]: ", (int)desc.api, (int)desc.port_number);
			}
			parse_message(open_port, message);
			open_port.controller->getMessage(&message);
		}
	}
}

void midi_open_port(const midi_port_descriptor &port)
{
	if (Open_midi_ports.find(port) == Open_midi_ports.end()) {
		RtMidiIn *midi_controller = new RtMidiIn((RtMidi::Api)port.api);
		midi_controller->openPort(port.port_number);
		if (midi_controller->isPortOpen()) {
			open_midi_port new_open_port{ 0 };
			new_open_port.descriptor = port;
			new_open_port.controller = midi_controller;
			Open_midi_ports.insert({ (uint32_t)port, new_open_port });
		}
	}
}

void midi_close_port(const midi_port_descriptor &port)
{
	auto value = Open_midi_ports.find(port);
	if (value != Open_midi_ports.end()) {
		auto &[port_number, open_port] = *value;

		open_port.controller->closePort();
		delete open_port.controller;

		Open_midi_ports.erase(port);
	}
}

void midi_for_each_open_port(std::function<void(midi_port_descriptor, const std::string &)> fn)
{
	for (auto &[port_number, open_port] : Open_midi_ports) {
		const std::string name = RtMidi::getApiDisplayName(open_port.controller->getCurrentApi()) + " " + open_port.controller->getPortName(open_port.descriptor.port_number);
		fn(port_number, name);
	}
}

void midi_for_each_port(std::function<void(midi_port_descriptor, const std::string &)> fn)
{
	std::vector<RtMidi::Api> apis;
	RtMidi::getCompiledApi(apis);

	for (RtMidi::Api api : apis) {
		RtMidiIn midi_api(api);
		uint16_t max_ports = (int)midi_api.getPortCount();
		for (uint16_t i = 0; i < max_ports; ++i) {
			const std::string name = RtMidi::getApiDisplayName((RtMidi::Api)api) + " " + midi_api.getPortName((uint32_t)i);
			fn({ (uint16_t)api, i }, name);
		}
	}
}

void midi_set_logging(bool enable)
{
	Show_midi_messages = enable;
}

bool midi_logging_is_enabled()
{
	return Show_midi_messages;
}

const char *midi_playback_device_name(midi_playback_device d)
{
	switch (d) {
		case midi_playback_device::none: return "None";
		case midi_playback_device::vera_psg: return "VERA PSG";
		case midi_playback_device::ym2151: return "YM2151";
	}
	return "None";
}

const midi_channel_settings *midi_port_get_channel(midi_port_descriptor port, uint8_t channel)
{
	if (channel < MAX_MIDI_CHANNELS) {
		auto value = Open_midi_ports.find(port);
		if (value != Open_midi_ports.end()) {
			auto &[port_number, open_port] = *value;

			return &(open_port.channels[channel].settings);
		}
	}

	return nullptr;
}

void midi_port_set_channel_playback_device(midi_port_descriptor port, uint8_t channel, midi_playback_device d)
{
	if (channel < MAX_MIDI_CHANNELS) {
		auto value = Open_midi_ports.find(port);
		if (value != Open_midi_ports.end()) {
			auto &[port_number, open_port] = *value;

			midi_channel_state &state = open_port.channels[channel];
			midi_key keys_on[MAX_MIDI_KEYS];
			memcpy(keys_on, state.keys_on, sizeof(midi_key) * MAX_MIDI_KEYS);

			for (uint8_t i = 0; i < MAX_MIDI_KEYS; ++i) {
				if (keys_on[i].voice != INVALID_VOICE) {
					note_off(open_port, channel, i, 0);
				}
			}

			state.settings.playback_device = d;

			if (d != midi_playback_device::none) {
				for (uint8_t i = 0; i < MAX_MIDI_KEYS; ++i) {
					if (keys_on[i].voice != INVALID_VOICE) {
						note_on(open_port, channel, i, keys_on[i].velocity);
					}
				}
			}
		}
	}
}

void midi_port_set_channel_use_velocity(midi_port_descriptor port, uint8_t channel, bool use_velocity)
{
	if (channel < MAX_MIDI_CHANNELS) {
		auto value = Open_midi_ports.find(port);
		if (value != Open_midi_ports.end()) {
			auto &[port_number, open_port] = *value;

			midi_channel_state &state = open_port.channels[channel];
			state.settings.use_velocity = use_velocity;
		}
	}
}

void midi_port_set_channel_psg_waveform(midi_port_descriptor port, uint8_t channel, uint8_t waveform)
{
	if (channel < MAX_MIDI_CHANNELS) {
		auto value = Open_midi_ports.find(port);
		if (value != Open_midi_ports.end()) {
			auto &[port_number, open_port] = *value;

			midi_channel_state &state = open_port.channels[channel];

			for (uint8_t i = 0; i < MAX_MIDI_KEYS; ++i) {
				if (state.keys_on[i].voice != INVALID_VOICE) {
					psg_set_channel_waveform(state.keys_on[i].voice, waveform);
				}
			}

			state.settings.device.psg.waveform = waveform;			
		}
	}
}
