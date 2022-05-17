#include "psg_overlay.h"

#include <algorithm>
#include "bitutils.h"
#include "audio.h"
#include "vera/vera_psg.h"
#include "vera/vera_pcm.h"
#include "imgui/imgui.h"
#include "util.h"

static void draw_buffer_bytes_number(int num)
{
	ImGui::SameLine();
	if (num == 0 || num == 4095) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1)); // red
	} else if (num < 1024) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1)); // yellow
	} else {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text)); // unchanged
	}
	ImGui::Text("%d", num);
	ImGui::PopStyleColor();
}

static void draw_buffer_bytes_bar(ImDrawList *draw_list, ImVec2 pos, float height, float min_l, float max_l, float min_r, float max_r)
{
	draw_list->AddRectFilled(ImVec2(pos.x,     pos.y + (1 - max_l) * height / 2), 
	                         ImVec2(pos.x + 1, pos.y + (1 - min_l) * height / 2), IM_COL32(0,   230, 179, 170));
	draw_list->AddRectFilled(ImVec2(pos.x,     pos.y + (1 - max_r) * height / 2),
	                         ImVec2(pos.x + 1, pos.y + (1 - min_r) * height / 2), IM_COL32(230, 179,   0, 170));
}

static void draw_indicator_line(ImDrawList *draw_list, ImVec2 topleft, ImVec2 size, float posrat, ImU32 color)
{
	const float pos = topleft.x + size.x * posrat;
	draw_list->AddLine(ImVec2(pos, topleft.y), ImVec2(pos, topleft.y + size.y), color);
}

void draw_debugger_vera_psg()
{
	if (ImGui::BeginTable("psg mon", 8)) {
		ImGui::TableSetupColumn("Ch", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Raw Bytes", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Freq", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Wave", ImGuiTableColumnFlags_WidthFixed, 88);
		ImGui::TableSetupColumn("Width", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("L", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("R", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Vol", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (unsigned int i = 0; i < 16; ++i) {
			ImGui::TableNextRow();
			if (i == 0) {
				ImGui::TableSetColumnIndex(2);  // freq
				ImGui::PushItemWidth(-FLT_MIN); // Right-aligned
				ImGui::TableSetColumnIndex(3);  // wave
				ImGui::PushItemWidth(-FLT_MIN);
				ImGui::TableSetColumnIndex(4); // width
				ImGui::PushItemWidth(-FLT_MIN);
				ImGui::TableSetColumnIndex(7); // vol
				ImGui::PushItemWidth(-FLT_MIN);
				ImGui::TableSetColumnIndex(0);
			}
			else {
				ImGui::TableNextColumn();
			}

			ImGui::PushID(i);
			const psg_channel* channel = psg_get_channel(i);

			ImGui::Text("%d", i);

			ImGui::TableNextColumn();
			ImGui::PushID("raw");
			uint8_t ch_data[4];
			ch_data[0] = channel->freq & 0xff;
			ch_data[1] = channel->freq >> 8;
			ch_data[2] = channel->volume | (channel->left << 6) | (channel->right << 7);
			ch_data[3] = channel->pw | channel->waveform << 6;
			for (int j = 0; j < 4; ++j) {
				if (j) {
					ImGui::SameLine();
				}
				if (ImGui::InputHex(j, ch_data[j])) {
					psg_writereg(i * 4 + j, ch_data[j]);
				}
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			float freq = channel->freq;
			ImGui::PushID("freq");
			if (ImGui::SliderFloat("", &freq, 64, 0xffff, "%.0f", ImGuiSliderFlags_Logarithmic)) {
				psg_set_channel_frequency(i, (uint16_t)std::min(std::max(freq, 0.f), 65535.f));
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			static const char* waveforms[] = {
				"Pulse",
				"Sawtooth",
				"Triangle",
				"Noise"
			};
			int wf = channel->waveform;
			ImGui::PushID("waveforms");
			if (ImGui::Combo("", &wf, waveforms, IM_ARRAYSIZE(waveforms))) {
				psg_set_channel_waveform(i, wf);
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			int pulse_width = channel->pw;
			ImGui::PushID("pulse_width");
			if (ImGui::SliderInt("", &pulse_width, 0, 63, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				psg_set_channel_pulse_width(i, pulse_width);
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			bool left = channel->left;
			ImGui::PushID("left");
			if (ImGui::Checkbox("", &left)) {
				psg_set_channel_left(i, left);
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			bool right = channel->right;
			ImGui::PushID("right");
			if (ImGui::Checkbox("", &right)) {
				psg_set_channel_right(i, right);
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			int volume = channel->volume;
			ImGui::PushID("volume");
			if (ImGui::SliderInt("", &volume, 0, 63, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				psg_set_channel_volume(i, volume);
			}
			ImGui::PopID();

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (ImGui::TreeNodeEx("PCM FIFO", ImGuiTreeNodeFlags_DefaultOpen)) {
		const pcm_debug_info dbg      = pcm_get_debug_info();
		uint8_t              ctrl     = pcm_read_ctrl();
		uint8_t              rate     = pcm_read_rate();
		uint8_t              data     = 0;
		bool                 f_width  = ctrl & 0b00100000;
		bool                 f_stereo = ctrl & 0b00010000;

		ImGui::Text("Raw Bytes");
		ImGui::SameLine();
		if (ImGui::InputHex(0, ctrl)) {
			pcm_write_ctrl(ctrl);
		}
		ImGui::SameLine();
		if (ImGui::InputHex(1, rate)) {
			pcm_write_rate(rate);
		}
		ImGui::SameLine();
		// InputHex always returns true when it's active, but we only need one time write after edit here
		ImGui::InputHex(2, data);
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			pcm_write_fifo(data);
		}

		const ImVec2 padding    = ImGui::GetStyle().FramePadding;
		const float  avail      = ImGui::GetContentRegionAvail().x;
		const ImVec2 frame_size = ImVec2(avail, 80);
		const float  curleft    = ImGui::GetCursorPosX();
		ImGui::Text("Buffer Bytes");
		ImGui::BeginChild("bufbytesplot", frame_size, false, ImGuiWindowFlags_HorizontalScrollbar);
		{
			// TODO zooming
			const ImVec2 topleft = ImGui::GetCursorScreenPos();
			const ImVec2 vissize   = ImVec2(frame_size.x - padding.x * 2, frame_size.y - padding.y * 2);
			if (vissize.x > 0 && vissize.y > 0) {
				const ImVec2 topleft_v = ImVec2(topleft.x + padding.x, topleft.y + padding.y);
				const ImVec2 scrsize   = frame_size;
				const ImVec2 mouse_pos = ImGui::GetMousePos();
				const int    mouse_x   = (int)(mouse_pos.x - topleft_v.x);
				const bool   mouse_in  = mouse_pos.y > topleft_v.y && mouse_pos.y < (topleft_v.y + vissize.y);
				ImDrawList * draw_list = ImGui::GetWindowDrawList();
				const int    chanwidth = (f_width ? 2 : 1);
				const int    chancount = (f_stereo ? 2 : 1);
				const int    sampwidth = chancount * chanwidth;
				const int    maxsamps  = 4095 / sampwidth;
				const float  cursiz_x  = topleft_v.x + dbg.cursiz * vissize.x / 4095;

				ImGui::Dummy(scrsize);
				draw_list->AddRectFilled(topleft, ImVec2(topleft.x + scrsize.x, topleft.y + scrsize.y),
					ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)));
				draw_list->AddRectFilled(topleft_v, ImVec2(cursiz_x, topleft_v.y + vissize.y),
					ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered)));

				float     minval[2]{ 0, 0 };
				float     maxval[2]{ 0, 0 };
				float     pixacc = 0;
				int       baracc = 0;
				uint8_t   bytes[4];
				int       count    = 0;
				int       remain   = dbg.cursiz;
				int       curidx   = dbg.curidx;
				bool      ttshown  = false;
				bool      barexist = false;
				while (remain >= sampwidth) {
					int rawval[2]{ 0, 0 };
					const int ttidx = curidx;
					barexist        = true;
					for (int i = 0; i < sampwidth; i++) {
						bytes[i] = dbg.fifo[curidx++];
						if (curidx >= 4095) {
							curidx = 0;
						}
						remain--;
					}
					int j = 0;
					for (int i = 0; i < chancount; i++) {
						int   val;
						float valf;
						if (f_width) {
							val = bytes[j] + bytes[j + 1] * 256;
							if (val > 32768) {
								val -= 65536;
							}
							j += 2;
						} else {
							val = bytes[j];
							if (val > 128) {
								val -= 256;
							}
							j += 1;
						}
						valf      = val / (f_width ? 32768.f : 128.f);
						rawval[i] = val;
						if (valf < minval[i]) {
							minval[i] = valf;
						}
						if (valf > maxval[i]) {
							maxval[i] = valf;
						}
					}
					if (!f_stereo) {
						minval[1] = minval[0];
						maxval[1] = maxval[0];
					}
					if (!ttshown && mouse_in && mouse_x == baracc) {
						if (f_stereo) {
							ImGui::SetTooltip("L: %4d: %6d\nR: %4d: %6d", count, rawval[0], count + chanwidth, rawval[1]);
						} else {
							ImGui::SetTooltip("%4d: %6d", count, rawval[0]);
						}
						ttshown = true;
					}
					pixacc += vissize.x / maxsamps;
					if (pixacc >= (baracc + 1)) {
						while (pixacc >= (baracc + 1)) {
							draw_buffer_bytes_bar(draw_list, ImVec2(topleft_v.x + baracc, topleft_v.y), vissize.y,
								minval[0], maxval[0], minval[1], maxval[1]);
							baracc++;
						}
						minval[0] = minval[1] = maxval[0] = maxval[1] = 0;
						barexist = false;
					}
					count += sampwidth;
				}
				if (barexist) {
					draw_buffer_bytes_bar(draw_list, ImVec2(topleft_v.x + baracc, topleft_v.y), vissize.y,
						minval[0], maxval[0], minval[1], maxval[1]);
				}

				// indicators
				draw_indicator_line(draw_list, topleft_v, vissize, 1024.f / 4095.f, IM_COL32(255, 255, 0, 170));
				draw_indicator_line(draw_list, topleft_v, vissize, dbg.minsiz / 4095.f, IM_COL32(255, 255, 255, 170));
				draw_indicator_line(draw_list, topleft_v, vissize, dbg.maxsiz / 4095.f, IM_COL32(255, 255, 255, 170));
			}
			ImGui::EndChild();
		}
		if (ImGui::IsItemClicked()) {
			pcm_reset_debug_values();
		}

		ImGui::Text("Cur:");
		draw_buffer_bytes_number(dbg.cursiz);
		ImGui::SameLine(avail / 3 + curleft);
		ImGui::Text("Min:");
		draw_buffer_bytes_number(dbg.minsiz);
		ImGui::SameLine(avail * 2 / 3 + curleft);
		ImGui::Text("Max:");
		draw_buffer_bytes_number(dbg.maxsiz);

		if (ImGui::Checkbox("16-bit", &f_width)) {
			pcm_write_ctrl(bit_set_or_res(ctrl, (uint8_t)0b00100000, f_width));
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Stereo", &f_stereo)) {
			pcm_write_ctrl(bit_set_or_res(ctrl, (uint8_t)0b00010000, f_stereo));
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset FIFO")) {
			pcm_write_ctrl(ctrl | 0b10000000);
		}

		int rate_i = rate;
		int vol    = ctrl & 0xf;
		char rate_txt[15];
		float rate_hz = rate <= 128 ? (float)SAMPLERATE * rate / 128 : 0;
		snprintf(rate_txt, 15, "%d (%.0f Hz)", rate, rate_hz);
		ImGui::SetNextItemWidth(avail / 2 - 48);
		if (ImGui::SliderInt("Rate", &rate_i, 0, 128, rate_txt, ImGuiSliderFlags_AlwaysClamp)) {
			pcm_write_rate(rate_i);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(avail / 2 - 48);
		if (ImGui::SliderInt("Volume", &vol, 0, 15, "%d", ImGuiSliderFlags_AlwaysClamp)) {
			pcm_write_ctrl(ctrl & 0xf0 | vol);
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("PSG Output", ImGuiTreeNodeFlags_DefaultOpen)) {
		int16_t psg_buffer[2 * SAMPLES_PER_BUFFER];
		audio_get_psg_buffer(psg_buffer);
		{
			float left_samples[SAMPLES_PER_BUFFER];
			float right_samples[SAMPLES_PER_BUFFER];

			float* l = left_samples;
			float* r = right_samples;

			const int16_t* b = psg_buffer;
			for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
				*l = *b;
				++l;
				++b;
				*r = *b;
				++r;
				++b;
			}

			ImGui::PlotLines("Left", left_samples, SAMPLES_PER_BUFFER, 0, nullptr, INT16_MIN, INT16_MAX, ImVec2(0, 80.0f));
			ImGui::PlotLines("Right", right_samples, SAMPLES_PER_BUFFER, 0, nullptr, INT16_MIN, INT16_MAX, ImVec2(0, 80.0f));
		}

		ImGui::TreePop();
	}
}
