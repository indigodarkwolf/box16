#include "midi_overlay.h"

#include "imgui/imgui.h"
#include "midi.h"

void draw_midi_overlay()
{
	bool midi_logging = midi_logging_is_enabled();
	if (ImGui::Checkbox("Enable MIDI message logging", &midi_logging)) {
		midi_set_logging(midi_logging);
	}

	ImGui::TextDisabled("PSG Channels");
	ImGui::Separator();
	midi_for_each_psg_channel([](int channel, midi_port control_port, uint16_t channel_bindings) {
		ImGui::PushID(channel);

		ImGui::Text("%02d", channel);
		ImGui::SameLine();
		std::string port_name;
		bool        have_name = (control_port != INVALID_MIDI_PORT) && midi_get_port_name(control_port, port_name);

		ImGui::PushItemWidth(128.0f);
		if (ImGui::BeginCombo("Midi Controller", have_name ? port_name.c_str() : "Select...")) {
			ImGui::Selectable("(NONE)", !have_name);
			midi_for_each_port([channel, control_port](midi_port port, const std::string &name) {
				std::string port_name;
				bool        have_name = midi_get_port_name(port, port_name);
				if (have_name) {
					if (ImGui::Selectable(port_name.c_str(), control_port == port)) {
						midi_open_port(port);
						midi_set_psg_channel_controller(channel, port);
					}
				}
			});
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		for (int i = 0; i < 16; ++i) {
			ImGui::SameLine();
			ImGui::PushID(i);
			bool selected = channel_bindings & (1 << i);
			if (ImGui::Checkbox("", &selected)) {
				midi_toggle_channel_control(channel, control_port, i);
			}
			ImGui::PopID();
		}

		ImGui::PopID();
	});
}
