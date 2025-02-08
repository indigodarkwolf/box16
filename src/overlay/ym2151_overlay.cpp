#include "ym2151_overlay.h"

#include "bitutils.h"
#include "display.h"
#include "imgui/imgui.h"
#include "util.h"
#include "ym2151/ym2151.h"
#include <cstdio>

static void ym2151_reg_input(uint8_t *regs, uint8_t idx)
{
	ImGui::TableSetColumnIndex((idx & 0xf) + 1);
	if (ImGui::InputHex(idx, regs[idx])) {
		YM_debug_write(idx, regs[idx]);
	}
}

void draw_debugger_ym2151()
{
	uint8_t regs[256];
	uint8_t status = YM_read_status();
	for (int i = 0; i < 256; i++) {
		regs[i] = YM_debug_read(i);
	}
	if (ImGui::TreeNodeEx("Interface", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		uint8_t addr = YM_last_address();
		uint8_t data = YM_last_data();
		if (ImGui::InputHexLabel("Address", addr)) {
			YM_write(0, addr);
		}
		ImGui::SameLine();
		if (ImGui::InputHexLabel("Data", data)) {
			YM_write(1, data);
		}
		ImGui::SameLine();
		ImGui::InputHexLabel("Status", status);

		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Raw Bytes", ImGuiTreeNodeFlags_Framed)) {
		if (ImGui::BeginTable("ym raw bytes", 17, ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%Xx", 0);
			ym2151_reg_input(regs, 0x01); // TEST
			ym2151_reg_input(regs, 0x08); // KEYON
			ym2151_reg_input(regs, 0x0F); // NOISE
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%Xx", 1);
			ym2151_reg_input(regs, 0x10); // CLKA1
			ym2151_reg_input(regs, 0x11); // CLKA2
			ym2151_reg_input(regs, 0x12); // CLKB
			ym2151_reg_input(regs, 0x14); // CONTROL
			ym2151_reg_input(regs, 0x18); // LFRQ
			ym2151_reg_input(regs, 0x19); // PMD/AMD
			ym2151_reg_input(regs, 0x1B); // CT/W
			// no unused registers at this point
			for (int i = 2; i < 16; i++) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%Xx", i);
				for (int j = 0; j < 16; j++) {
					ym2151_reg_input(regs, i * 16 + j);
				}
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Timer & Control", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::BeginTable("ym timer & control", 7)) {
			struct {
				bool en, irq, ovf;
				int  reload, cur;
			} timer[2];
			bool csm = regs[0x14] & (1 << 7);
			bool ct1 = regs[0x1B] & (1 << 6);
			bool ct2 = regs[0x1B] & (1 << 7);

			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

			for (int i = 0; i < 2; i++) {
				const uint8_t EN_MASK  = 1 << (i + 0);
				const uint8_t IRQ_MASK = 1 << (i + 2);
				const uint8_t RES_MASK = 1 << (i + 4);
				const uint8_t OVF_MASK = 1 << (i + 0);
				const int     TIM_MAX  = i ? 255 : 1023;
				auto          tim      = &timer[i];
				tim->en                = regs[0x14] & EN_MASK;
				tim->irq               = regs[0x14] & IRQ_MASK;
				tim->ovf               = status & OVF_MASK;
				tim->reload            = i ? regs[0x12] : regs[0x10] * 4 + (regs[0x11] & 0x03);
				tim->cur               = YM_get_timer_counter(i);

				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text(i ? "Timer B" : "Timer A");
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("Enable", &tim->en)) {
					YM_debug_write(0x14, bit_set_or_res(regs[0x14], EN_MASK, tim->en));
				}
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("IRQ Enable", &tim->irq)) {
					YM_debug_write(0x14, bit_set_or_res(regs[0x14], IRQ_MASK, tim->irq));
				}
				ImGui::TableNextColumn();
				ImGui::Checkbox("Overflow", &tim->ovf);
				ImGui::TableNextColumn();
				if (ImGui::Button("Reset")) {
					YM_debug_write(0x14, regs[0x14] | RES_MASK);
				}
				ImGui::TableNextColumn();
				if (ImGui::SliderInt("Reload", &tim->reload, TIM_MAX, 0)) {
					if (i) {
						// timer b
						YM_debug_write(0x12, tim->reload);
					} else {
						// timer a
						YM_debug_write(0x10, tim->reload >> 2);
						YM_debug_write(0x11, regs[0x11] & ~0x03 | (tim->reload & 0x03));
					}
				}
				ImGui::TableNextColumn();
				char buf[5];
				std::snprintf(buf, 5, "%d", tim->cur);
				ImGui::ProgressBar(tim->cur / (float)TIM_MAX, ImVec2(0, 0), buf);
				ImGui::SameLine();
				ImGui::Text("Counter");

				ImGui::PopID();
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Checkbox("CSM", &csm)) {
				YM_debug_write(0x14, bit_set_or_res(regs[0x14], (uint8_t)(1 << 7), csm));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("When Timer A overflows, cause a Key-down event on all operators");
			}
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("CT1", &ct1)) {
				YM_debug_write(0x1B, bit_set_or_res(regs[0x1B], (uint8_t)(1 << 6), ct1));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("GPIO line 1 (not connected to anything in X16)");
			}
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("CT2", &ct2)) {
				YM_debug_write(0x1B, bit_set_or_res(regs[0x1B], (uint8_t)(1 << 7), ct2));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("GPIO line 2 (not connected to anything in X16)");
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("LFO & Noise", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		debugger_draw_ym_lfo_and_noise(regs);
		ImGui::TreePop();
	}

	static ym_channel_data channel[8];
	static ym_keyon_state  keyon[8];

	if (ImGui::TreeNodeEx("Channels", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		debugger_draw_ym_voices(regs, channel, keyon, IM_ARRAYSIZE(channel), [](uint8_t addr, uint8_t value) { YM_debug_write(addr, value); });
		ImGui::TreePop();
	}
}

void debugger_draw_ym_lfo_and_noise(uint8_t *regs)
{
	if (ImGui::BeginTable("ym lfo & noise", 2, ImGuiTableFlags_SizingStretchSame)) {
		static const char *waveforms[] = {
			"Sawtooth",
			"Square",
			"Triangle",
			"Noise"
		};
		const uint8_t LRES_MASK = (1 << 1);
		const uint8_t LW_MASK   = 0x03;
		bool          lres      = regs[0x01] & LRES_MASK;
		int           lw        = regs[0x1B] & LW_MASK;
		int           lfrq      = regs[0x18];

		ym_modulation_state mod_data;
		YM_get_modulation_state(mod_data);

		float lcnt = mod_data.LFO_phase;
		int   amd  = mod_data.amplitude_modulation;
		int   pmd  = mod_data.phase_modulation;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::BeginGroup();
		ImGui::Text("LFO");
		ImGui::SameLine(72);
		if (ImGui::Checkbox("Reset", &lres)) {
			YM_debug_write(0x01, bit_set_or_res(regs[0x01], LRES_MASK, lres));
		}
		ImGui::EndGroup();
		ImGui::TableNextColumn();
		if (ImGui::Combo("Waveform", &lw, waveforms, 4)) {
			YM_debug_write(0x1B, regs[0x1B] & ~LW_MASK | lw);
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::SliderInt("LFO Freq", &lfrq, 0, 255)) {
			YM_debug_write(0x18, lfrq);
		}
		ImGui::TableNextColumn();
		char buf[4];
		std::snprintf(buf, 4, "%d", (int)(lcnt * 256));
		ImGui::ProgressBar(lcnt, ImVec2(0, 0), buf);
		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::Text("Cur. Phase");

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::SliderInt("AMD", &amd, 0, 127)) {
			YM_debug_write(0x19, amd);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Amplitude Modulation (tremolo) Depth");
		}
		ImGui::TableNextColumn();
		if (ImGui::SliderInt("PMD", &pmd, 0, 127)) {
			YM_debug_write(0x19, pmd | 0x80);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pulse Modulation (vibrato) Depth\n");
		}

		const uint8_t NEN_MASK  = (1 << 7);
		const uint8_t NFRQ_MASK = 0x1F;
		bool          nen       = regs[0x0F] & NEN_MASK;
		int           nfrq      = regs[0x0F] & NFRQ_MASK;
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::BeginGroup();
		ImGui::Text("Noise");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("If Enabled, Voice 8, Operator 3 uses a noise\nwaveform instead of the usual sine wave");
		}
		ImGui::SameLine(72);
		if (ImGui::Checkbox("Enable", &nen)) {
			YM_debug_write(0x0F, bit_set_or_res(regs[0x0F], NEN_MASK, nen));
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("If Enabled, Channel 7, Operator 3 uses a noise\nwaveform instead of the usual sine wave");
		}
		ImGui::EndGroup();
		ImGui::TableNextColumn();
		if (ImGui::SliderInt("Frequency", &nfrq, 31, 0)) {
			YM_debug_write(0x0F, regs[0x0F] & ~NFRQ_MASK | nfrq);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Sets the frequency of the noise pattern on Chan7 OP3, ");
		}

		ImGui::EndTable();
	}
}

void debugger_draw_ym_voices(uint8_t *regs, ym_channel_data *channel, ym_keyon_state *keyons, int num_channels, std::function<void(uint8_t, uint8_t)> apply_byte)
{
	if (ImGui::BeginTable("ym channels", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed); // chan num
		ImGui::TableSetupColumn("", 0, 0.4f);                          // chan regs
		ImGui::TableSetupColumn("");                                   // slot regs
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed); // slot con

		for (int i = 0; i < num_channels; i++) {
			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d", i);

			debugger_draw_ym_voice(i, regs, channel[i], keyons ? &keyons[i] : nullptr, apply_byte);

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

void debugger_draw_ym_voice(int i, uint8_t *regs, ym_channel_data &ch, ym_keyon_state *keyon, std::function<void(uint8_t, uint8_t)> apply_byte)
{
	static const uint8_t slot_map[4] = { 0, 16, 8, 24 };

	const uint8_t confb       = 0x20 + i;
	const uint8_t kc          = 0x28 + i;
	const uint8_t kf          = 0x30 + i;
	const uint8_t amspms      = 0x38 + i;
	const uint8_t tmp_kc      = regs[kc] & 0x7f;
	const char   *regtip      = "REG:$%02X bits %d-%d";
	const char   *regtipbit   = "REG:$%02X bit %d";
	const char   *voicetip    = "%s\nREG:$%02X bits %d-%d";
	const char   *voicetipbit = "%s\nREG:$%02X bit %d";

	ch.l     = regs[confb] & (1 << 6);
	ch.r     = regs[confb] & (1 << 7);
	ch.con   = regs[confb] & 0x07;
	ch.fb    = (regs[confb] >> 3) & 0x07;
	ch.kc    = (tmp_kc - ((tmp_kc + 1) >> 2)) + (regs[kf] / 256.f);
	ch.ams   = regs[amspms] & 0x03;
	ch.pms   = (regs[amspms] >> 4) & 0x07;
	int fpkc = (int)(ch.kc * 256);

	// Channel
	ImGui::TableNextColumn();
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 8);
	if (ImGui::BeginTable("confb", 4)) {
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(12);
		if (ImGui::Checkbox("L", &ch.l)) {
			apply_byte(confb, bit_set_or_res(regs[confb], (uint8_t)(1 << 6), ch.l));
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(voicetipbit,"Audio Out Enable Left Channel",confb,6);
		}
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(12);
		if (ImGui::Checkbox("R", &ch.r)) {
			apply_byte(confb, bit_set_or_res(regs[confb], (uint8_t)(1 << 7), ch.r));
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(voicetipbit,"Audio Out Enable Right Channel",confb,7);
		}
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(12);
		if (ImGui::DragInt("CON", &ch.con, 1, 0, 7)) {
			apply_byte(confb, regs[confb] & ~0x07 | ch.con);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(voicetip,"Operator Connection Algorithm",confb,0,2);
		}
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-28);
		if (ImGui::SliderInt("FB", &ch.fb, 0, 7)) {
			apply_byte(confb, regs[confb] & ~0x38 | (ch.fb << 3));
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(voicetip,"Operator 0 Self-Feedback Level",confb,3,5);
		}
		ImGui::EndTable();
	}

	if (ImGui::BeginTable("amspms", 2)) {
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-28);
		if (ImGui::SliderInt("AMS", &ch.ams, 0, 3)) {
			apply_byte(amspms, regs[amspms] & ~0x03 | ch.ams);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(voicetip,"Amplitude Modulation Sensitivity",amspms,0,1);
		}
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-28);
		if (ImGui::SliderInt("PMS", &ch.pms, 0, 7)) {
			apply_byte(amspms, regs[amspms] & ~0x70 | (ch.pms << 4));
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(voicetip,"Phase Modulation Sensitivity",amspms,4,6);
		}
		ImGui::EndTable();
	}

	if (keyon) {
		const char notes[] = "C-C#D-D#E-F-F#G-G#A-A#B-";
		float      cents   = (fpkc & 0xFF) * 100.f / 256.f;
		if (cents > 50) {
			cents = cents - 100;
		}
		const uint8_t note = (fpkc >> 8) + (cents < 0) + 1;
		const uint8_t ni   = (note % 12) * 2;
		const uint8_t oct  = note / 12;
		// C#8 +00.0
		char kcinfo[12];
		std::snprintf(kcinfo, 12, "%c%c%d %+05.1f", notes[ni], notes[ni + 1], oct, cents);
		ImGui::SetNextItemWidth(-28);
		if (ImGui::SliderFloat("KC", &ch.kc, 0, 96, kcinfo, ImGuiSliderFlags_NoRoundToFormat)) {
			fpkc = std::min((int)(ch.kc * 256), (96 * 256) - 1);
			apply_byte(kc, (fpkc >> 8) * 4 / 3);
			apply_byte(kf, fpkc & 0xFF);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("KC=Keycode KF=Key Fraction\nKC REG:$%02X\nKF REG:$%02X",kc,kf);
		}

		ImGui::Button("KeyOn");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Click and hold to play a note.");
		}

		keyon->dkob_state = (keyon->dkob_state << 1) | (int)ImGui::IsItemActive();
		switch (keyon->dkob_state & 0b11) {
			case 0b01: // keyon checked slots
				apply_byte(0x08, i | (keyon->debug_kon[0] << 3) | (keyon->debug_kon[1] << 4) | (keyon->debug_kon[2] << 5) | (keyon->debug_kon[3] << 6));
				break;
			case 0b10: // keyoff all slots
				apply_byte(0x08, i);
				break;
		}
		ImGui::PushID("konslots");
		for (int j = 0; j < 4; j++) {
			ImGui::PushID(j);
			ImGui::SameLine();
			ImGui::Checkbox("", &keyon->debug_kon[j]);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Use Operator %d",j);
			}
			ImGui::PopID();
		}
		ImGui::PopID();
	}
	ImGui::PopStyleVar(3);

	// Slot
	ImGui::TableNextColumn();
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 6);
	if (ImGui::BeginTable("slot", 15)) {
		ImGui::TableNextColumn();
		ImGui::Text("%s", "Slot");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Operator Slot Number");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "DT1");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Detune 1\nFine pitch adjustment");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "DT2");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Detune 2\nCoarse pitch adjustment");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "MUL");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Frequency Multiplier\nModifies pitch by specific intervals");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "=Freq");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Current frequency produced by each operator");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "AR");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Attack Rate\nSpeed the volume rises from 0 to peak");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "D1R");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Decay Rate 1\nSpeed the volume falls from peak to sustain level");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "D1L");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Decay 1 Level (Sustain)\nVolume level at which decay rate switches to D2R");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "D2R");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Decay Rate 2\nSpeed the volume decays after sustain is reached.");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "RR");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Release Rate\nSpeed the volume falls to 0 when key released");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "KS");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Key Scaling\nSpeed at which the envelope progresses\nEffectiveness increases with note pitch");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "Env");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Current envelope state");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "TL");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Total Level (volume)\nAttenuates the operator's output\n(0=loudest, 127=silent)");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "AM");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Amplitude Modulation Enabled");
		}
		ImGui::TableNextColumn();
		ImGui::Text("%s", "Out");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Signal level output by operator");
		}
		ImGui::TableNextRow();
		for (int j = 0; j < 4; j++) {
			const int     slnum     = slot_map[j] + i;
			const uint8_t muldt1    = 0x40 + slnum;
			const uint8_t tl        = 0x60 + slnum;
			const uint8_t arks      = 0x80 + slnum;
			const uint8_t d1rame    = 0xA0 + slnum;
			const uint8_t d2rdt2    = 0xC0 + slnum;
			const uint8_t rrd1l     = 0xE0 + slnum;
			auto &        slot      = ch.slot[j];

			slot.mul = regs[muldt1] & 0x0F;
			slot.dt1 = (regs[muldt1] >> 4) & 0x07;
			slot.tl  = regs[tl] & 0x7F;
			slot.ar  = regs[arks] & 0x1F;
			slot.ks  = regs[arks] >> 6;
			slot.d1r = regs[d1rame] & 0x1F;
			slot.ame = regs[d1rame] & 0x80;
			slot.d2r = regs[d2rdt2] & 0x1F;
			slot.dt2 = regs[d2rdt2] >> 6;
			slot.rr  = regs[rrd1l] & 0x0F;
			slot.d1l = regs[rrd1l] >> 4;

			ym_slot_state slot_state;
			YM_get_slot_state(slnum, slot_state);

			ImGui::PushID(j);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d", slot_map[j] + i);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("dt1", &slot.dt1, 0, 7)) {
				apply_byte(muldt1, regs[muldt1] & ~0x70 | (slot.dt1 << 4));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,muldt1,4,6);
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("dt2", &slot.dt2, 0, 3)) {
				apply_byte(d2rdt2, regs[d2rdt2] & ~0xC0 | (slot.dt2 << 6));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,d2rdt2,6,7);
			}
			ImGui::TableNextColumn();
			char buf[11] = ".5";
			if (slot.mul > 0) {
				std::snprintf(buf, 11, "%d", slot.mul);
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("mul", &slot.mul, 0, 15, buf)) {
				apply_byte(muldt1, regs[muldt1] & ~0x0F | slot.mul);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,muldt1,0,3);
			}
			ImGui::TableNextColumn();
			ImGui::Text("%d", slot_state.frequency);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("ar", &slot.ar, 0, 31)) {
				apply_byte(arks, regs[arks] & ~0x1F | slot.ar);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,arks,0,4);
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("d1r", &slot.d1r, 0, 31)) {
				apply_byte(d1rame, regs[d1rame] & ~0x1F | slot.d1r);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,d1rame,0,4);
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("d1l", &slot.d1l, 15, 0)) {
				apply_byte(rrd1l, regs[rrd1l] & ~0xF0 | (slot.d1l << 4));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,rrd1l,4,7);
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("d2r", &slot.d2r, 0, 31)) {
				apply_byte(d2rdt2, regs[d2rdt2] & ~0x1F | slot.d2r);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,d2rdt2,0,4);
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("rr", &slot.rr, 0, 15)) {
				apply_byte(rrd1l, regs[rrd1l] & ~0x0F | slot.rr);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,rrd1l,0,3);
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("ks", &slot.ks, 0, 3)) {
				apply_byte(arks, regs[arks] & ~0xC0 | (slot.ks << 6));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,arks,6,7);
			}
			ImGui::TableNextColumn();
			char envstate_txt[] = " ";
			envstate_txt[0]     = " ADSR"[slot_state.env_state];
			ImGui::ProgressBar(slot_state.eg_output, ImVec2(-FLT_MIN, 0), envstate_txt);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::SliderInt("tl", &slot.tl, 127, 0)) {
				apply_byte(tl, regs[tl] & ~0x7F | slot.tl);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtip,tl,0,6);
			}
			ImGui::TableNextColumn();
			ImGui::PushID("amelia");
			if (ImGui::Checkbox("", &slot.ame)) {
				apply_byte(d1rame, bit_set_or_res(regs[d1rame], (uint8_t)0x80, slot.ame));
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(regtipbit,d1rame,7);
			}
			ImGui::PopID();
			float out = slot_state.final_env;
			char  buf2[5];
			std::snprintf(buf2, 5, "%d", (int)((1 - out) * 1024));
			ImGui::TableNextColumn();
			ImGui::ProgressBar(out, ImVec2(-FLT_MIN, 0), buf2);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Operator output value");
			}

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::PopStyleVar(3);

	// CON gfx
	ImGui::TableNextColumn();
	ImGui::Dummy(ImVec2(16, 15));
	// this is so ugly
	ImGui::Tile((display_icons)((int)ICON_FM_ALG + ch.con), ImVec2(16, 64));
}
