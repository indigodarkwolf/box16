#include "midi_overlay.h"

#include "imgui/imgui.h"
#include "midi.h"

#include "vera/vera_psg.h"

void draw_midi_overlay()
{
	bool midi_logging = midi_logging_is_enabled();
	if (ImGui::Checkbox("Enable MIDI message logging", &midi_logging)) {
		midi_set_logging(midi_logging);
	}

	ImGui::TextDisabled("MIDI Devices");
	ImGui::Separator();
	ImGui::NewLine();

	midi_for_each_open_port([](midi_port_descriptor port, const std::string name) {
		if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			uint8_t unused_channels[MAX_MIDI_CHANNELS];
			uint8_t num_unused_channels = 0;

			for (uint8_t i = 0; i < MAX_MIDI_CHANNELS; ++i) {
				const midi_channel_settings *settings = midi_port_get_channel(port, i);
				if (settings == nullptr) {
					continue;
				}

				if (settings->playback_device == midi_playback_device::none) {
					unused_channels[num_unused_channels] = i;
					++num_unused_channels;
					continue;
				}

				ImGui::PushID(i);
				ImGui::Text("Channel %d", i);
				ImGui::TreePush("Device settings");

				if (ImGui::BeginCombo("Playback Device", midi_playback_device_name(settings->playback_device))) {
					const midi_playback_device devices[] = {
						midi_playback_device::none,
						midi_playback_device::vera_psg,
						midi_playback_device::ym2151
					};
					for (midi_playback_device d : devices) {
						if (ImGui::Selectable(midi_playback_device_name(d), d == settings->playback_device)) {
							midi_port_set_channel_playback_device(port, i, d);
						}
					}
					ImGui::EndCombo();
				}
				if (settings->playback_device == midi_playback_device::vera_psg) {
					static const char *waveforms[] = {
						"Pulse",
						"Sawtooth",
						"Triangle",
						"Noise"
					};
					int wf = settings->device.psg.waveform;
					if (ImGui::Combo("Waveform", &wf, waveforms, IM_ARRAYSIZE(waveforms))) {
						midi_port_set_channel_psg_waveform(port, i, (uint8_t)wf);
					}
				}

				ImGui::TreePop();
				ImGui::PopID();
			}

			if (num_unused_channels > 0) {
				ImGui::Text("%s", "Add Channel");
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 128.0f);
				ImGui::SetColumnWidth(1, 256.0f);

				ImGui::SetNextItemWidth(48.0f);
				static uint8_t channel_idx = 0;
				if (channel_idx > num_unused_channels) {
					channel_idx = num_unused_channels - 1;
				}
				char channel_label[4];
				sprintf(channel_label, "%d", unused_channels[channel_idx]);
				if (ImGui::BeginCombo("Channel", channel_label)) {
					for (uint8_t c = 0; c < num_unused_channels; ++c) {
						char channel_label[4];
						sprintf(channel_label, "%d", unused_channels[c]);
						if (ImGui::Selectable(channel_label, c == channel_idx)) {
							channel_idx = c;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::NextColumn();

				ImGui::SetNextItemWidth(96.0f);
				if (ImGui::BeginCombo("Playback Device", midi_playback_device_name(midi_playback_device::none))) {
					const midi_playback_device devices[] = {
						midi_playback_device::none,
						midi_playback_device::vera_psg,
						midi_playback_device::ym2151
					};
					for (midi_playback_device d : devices) {
						if (ImGui::Selectable(midi_playback_device_name(d), d == midi_playback_device::none)) {
							midi_port_set_channel_playback_device(port, unused_channels[channel_idx], d);
							channel_idx = 0;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::Columns(1);
			}
			ImGui::NewLine();
		}
	});
	{
		if (ImGui::BeginCombo("Open Midi Controller", "Select...")) {
			midi_for_each_port([](midi_port_descriptor port, const std::string &name) {
				if (ImGui::Selectable(name.c_str(), false)) {
					midi_open_port(port);
				}
			});
			ImGui::EndCombo();
		}
	}
}
