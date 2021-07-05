#include "midi.h"

#include "RtMidi.h"

#include "vera/vera_psg.h"
#include <unordered_map>

#define MAX_MIDI_CHANNELS (16) // Comes from the 4-bit field in the midi message format

const midi_port INVALID_MIDI_PORT = { RtMidi::Api::NUM_APIS, -1 };

struct psg_midi_mapping {
	midi_port midi_control_port;
	uint16_t  midi_channel_mask;
};

static psg_midi_mapping Psg_midi_mappings[PSG_NUM_CHANNELS];
static RtMidiIn         Midi_in_api;

static std::unordered_map<uint32_t, RtMidiIn *> Open_midi_ports;

static bool Show_midi_messages = false;

// Frequency table matching MIDI keys to PSG frequency settings.
// See also: /tools/generate_psg_frequency_table.cpp
static uint16_t Psg_frequency_table[128] = {
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

bool midi_port::operator==(const midi_port &rhs) const
{
	return (this->api == rhs.api) && (this->port_number == rhs.port_number);
}

bool midi_port::operator!=(const midi_port &rhs) const
{
	return (this->api != rhs.api) || (this->port_number != rhs.port_number);
}

midi_port::operator uint32_t() const
{
	return as_uint32();
}

uint32_t midi_port::as_uint32() const
{
	return *reinterpret_cast<const uint32_t *>(this);
}

midi_port midi_port::from_uint32(uint32_t port)
{
	return *reinterpret_cast<midi_port *>(&port);
}

static void note_off(const midi_port &port, int channel, int keynum, int velocity)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: note off %d %d %d\n", port.api, port.port_number, channel, keynum, velocity);
	}

	for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
		const psg_midi_mapping &mapping = Psg_midi_mappings[i];
		if ((mapping.midi_control_port == port) && (mapping.midi_channel_mask & (1 << channel))) {
			psg_set_channel_volume(i, 0);
		}
	}
}

static void note_on(const midi_port &port, int channel, int keynum, int velocity)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: note on %d %d %d\n", port.api, port.port_number, channel, keynum, velocity);
	}

	for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
		const psg_midi_mapping &mapping = Psg_midi_mappings[i];
		if ((mapping.midi_control_port == port) && (mapping.midi_channel_mask & (1 << channel))) {
			psg_set_channel_frequency(i, Psg_frequency_table[keynum]);
			psg_set_channel_volume(i, (uint8_t)(velocity >> 1));
		}
	}
}

static void polyphonic_key_pressure(midi_port port, int channel, int keynum, int pressure)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: polyphonic key pressure %d %d %d\n", port.api, port.port_number, channel, keynum, pressure);
	}
	// TODO: Write me.
}

static void control_change(const midi_port &port, int channel, int controller_number, int controller_value)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: control change %d %d %d\n", port.api, port.port_number, channel, controller_number, controller_value);
	}

	// TODO: Obviously this is a hack to support my particular MIDI device. We'll probably need a way
	//		 to bind behaviors to controller values, to support varying MIDI devices.
	if (controller_number == 1) {
		for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
			const psg_midi_mapping &mapping = Psg_midi_mappings[i];
			if ((mapping.midi_control_port == port) && (mapping.midi_channel_mask & (1 << channel))) {
				psg_set_channel_pulse_width(i, (uint8_t)(controller_value >> 1));
			}
		}
	}
}

static void program_change(const midi_port &port, int channel, int program)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: program change %d %d\n", port.api, port.port_number, channel, program);
	}
	// TODO: Write me.
}

static void channel_pressure(const midi_port &port, int channel, int pressure)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: channel pressure %d %d\n", port.api, port.port_number, channel, pressure);
	}
	// TODO: Write me.
}

static void pitch_bend(const midi_port &port, int channel, int bend)
{
	if (Show_midi_messages) {
		printf("midi [%d,%d]: pitch bend %d %d\n", port.api, port.port_number, channel, bend);
	}
	// TODO: Write me.
}

static void parse_message(const midi_port &port, const std::vector<unsigned char> &message)
{
	const int channel = message[0] & 0x0f;
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
			pitch_bend(port, channel, (message[1] << 7) | (message[2]));
			break;
	}
}

void midi_init()
{
	for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
		Psg_midi_mappings[i].midi_control_port = INVALID_MIDI_PORT;
		Psg_midi_mappings[i].midi_channel_mask = 0;
	}
}

void midi_process()
{
	std::vector<unsigned char> message;
	for (auto [port, controller] : Open_midi_ports) {
		controller->getMessage(&message);
		while (message.size() > 0) {
			parse_message(midi_port::from_uint32(port), message);
			controller->getMessage(&message);
		}
	}
}

void midi_open_port(const midi_port &port)
{
	if (Open_midi_ports.find(port) == Open_midi_ports.end()) {
		RtMidiIn *midi_controller = new RtMidiIn((RtMidi::Api)port.api);
		midi_controller->openPort(port.port_number);
		if (midi_controller->isPortOpen()) {
			Open_midi_ports.insert({ port, midi_controller });
		}
	}
}

void midi_close_port(const midi_port &port)
{
	auto open_port = Open_midi_ports.find(port);
	if (open_port != Open_midi_ports.end()) {
		auto &[port_number, controller] = *open_port;

		controller->closePort();
		delete controller;

		for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
			psg_midi_mapping &mapping = Psg_midi_mappings[i];
			if (mapping.midi_control_port == port) {
				mapping.midi_control_port = INVALID_MIDI_PORT;
				mapping.midi_channel_mask = 0;
			}
		}

		Open_midi_ports.erase(port);
	}
}

void midi_set_psg_channel_controller(int psg_channel, midi_port port)
{
	if (psg_channel < PSG_NUM_CHANNELS) {
		Psg_midi_mappings[psg_channel].midi_control_port = port;
	}
}

void midi_toggle_channel_control(int psg_channel, midi_port, int midi_channel)
{
	if (psg_channel < PSG_NUM_CHANNELS && midi_channel < MAX_MIDI_CHANNELS) {
		Psg_midi_mappings[psg_channel].midi_channel_mask ^= (1 << midi_channel);
	}
}

bool midi_get_port_name(midi_port midi_port, std::string &name)
{
	std::vector<RtMidi::Api> apis;
	RtMidi::getCompiledApi(apis);

	for (RtMidi::Api api : apis) {
		if (api == (RtMidi::Api)midi_port.api) {
			RtMidiIn midi_api(api);
			int      max_ports = (int)midi_api.getPortCount();
			if (midi_port.port_number < max_ports) {
				name = midi_api.getPortName(midi_port.port_number);
				return true;
			}
		}
	}

	return false;
}

void midi_for_each_psg_channel(std::function<void(int, midi_port, uint16_t)> fn)
{
	for (int i = 0; i < PSG_NUM_CHANNELS; ++i) {
		fn(i, Psg_midi_mappings[i].midi_control_port, Psg_midi_mappings[i].midi_channel_mask);
	}
}

void midi_for_each_port(std::function<void(midi_port, const std::string &)> fn)
{
	std::vector<RtMidi::Api> apis;
	RtMidi::getCompiledApi(apis);

	for (RtMidi::Api api : apis) {
		RtMidiIn midi_api(api);
		int16_t  max_ports = (int)midi_api.getPortCount();
		for (int16_t i = 0; i < max_ports; ++i) {
			const std::string name = RtMidi::getApiDisplayName((RtMidi::Api)api) + " " + Midi_in_api.getPortName((uint32_t)i);
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