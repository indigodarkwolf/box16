#include "overlay.h"

#include <SDL.h>

#include <array>
#include <functional>
#include <nfd.h>
#include <string>
#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"

#include "cpu_visualization.h"
#include "disasm_overlay.h"
#include "ram_dump.h"
#include "util.h"
#include "vram_dump.h"

#include "audio.h"
#include "bitutils.h"
#include "cpu/fake6502.h"
#include "cpu/mnemonics.h"
#include "debugger.h"
#include "disasm.h"
#include "display.h"
#include "glue.h"
#include "joystick.h"
#include "keyboard.h"
#include "midi_overlay.h"
#include "options_menu.h"
#include "psg_overlay.h"
#include "smc.h"
#include "symbols.h"
#include "timing.h"
#include "vera/sdcard.h"
#include "vera/vera_video.h"
#include "ym2151_overlay.h"

bool Show_options = false;
#if defined(_DEBUG)
bool Show_imgui_demo = false;
#endif
bool Show_memory_dump_1    = false;
bool Show_memory_dump_2    = false;
bool Show_cpu_monitor      = false;
bool Show_disassembler     = false;
bool Show_breakpoints      = false;
bool Show_watch_list       = false;
bool Show_symbols_list     = false;
bool Show_symbols_files    = false;
bool Show_cpu_visualizer   = false;
bool Show_VRAM_visualizer  = false;
bool Show_VERA_monitor     = false;
bool Show_VERA_palette     = false;
bool Show_VERA_layers      = false;
bool Show_VERA_sprites     = false;
bool Show_VERA_PSG_monitor = false;
bool Show_YM2151_monitor   = false;
bool Show_midi_overlay     = false;
bool Show_display 		   = true;

bool display_focused       = false;

imgui_vram_dump vram_dump;

static void draw_debugger_cpu_status()
{
	ImGui::BeginTable("cpu status", 3, ImGuiTableFlags_Borders);
	{
		ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 140);
		ImGui::TableSetupColumn("CPU Stack", ImGuiTableColumnFlags_WidthFixed, 63);
		ImGui::TableSetupColumn("Smart Stack", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		ImGui::BeginTable("cpu regs", 1);
		{
			ImGui::TableNextColumn();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));

			char const *names[] = { "N", "V", "-", "B", "D", "I", "Z", "C" };
			uint8_t     mask    = 0x80;
			int         n       = 0;
			while (mask > 0) {
				ImGui::BeginGroup();
				ImGui::Text("%s", names[n]);
				if (ImGui::SmallButton(state6502.status & mask ? "1" : "0")) {
					state6502.status ^= mask;
				}
				mask >>= 1;
				++n;
				ImGui::EndGroup();
				ImGui::SameLine();
			}

			ImGui::NewLine();
			ImGui::NewLine();

			ImGui::PopStyleVar();

			ImGui::BeginGroup();
			{
				ImGui::InputHexLabel("A", state6502.a);
				ImGui::InputHexLabel("X", state6502.x);
				ImGui::InputHexLabel("Y", state6502.y);
			}
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			{
				ImGui::InputHexLabel("PC", state6502.pc);
				ImGui::InputHexLabel("SP", state6502.sp);
			}
			ImGui::EndGroup();

			ImGui::NewLine();
			ImGui::InputHexLabel("RAM Bank", RAM[0]);
			ImGui::InputHexLabel("ROM Bank", RAM[1]);

			ImGui::NewLine();

			auto registers = [&](int start, int end) {
				ImGui::PushItemWidth(width_uint16);

				char label[4] = "r0";
				for (int i = start; i <= end; ++i) {
					sprintf(label, i < 10 ? " r%d" : "r%d", i);
					ImGui::Text("%s", label);
					ImGui::SameLine();
					uint16_t value = (int)get_mem16(2 + (i << 1), 0);
					if (ImGui::InputHex(i, value)) {
						debug_write6502(2 + (i << 1), 0, value & 0xff);
						debug_write6502(3 + (i << 1), 0, value >> 8);
					}
				}

				ImGui::PopItemWidth();
			};

			ImGui::TextDisabled("API Registers");
			ImGui::NewLine();

			ImGui::BeginGroup();
			registers(0, 5);
			ImGui::EndGroup();
			ImGui::SameLine();

			ImGui::BeginGroup();
			registers(6, 10);
			ImGui::NewLine();
			registers(11, 15);
			ImGui::EndGroup();
		}
		ImGui::EndTable();

		ImGui::TableNextColumn();

		if (ImGui::BeginTable("cpu stack", 1, ImGuiTableFlags_ScrollY)) {
			for (uint16_t i = (uint16_t)state6502.sp + 0x100; i < 0x200; ++i) {
				uint8_t value = debug_read6502(i);
				ImGui::TableNextColumn();
				if (ImGui::InputHex(i, value)) {
					debug_write6502(i, 0, value);
				}
			}
			ImGui::EndTable();
		}

		ImGui::TableNextColumn();

		if (ImGui::BeginTable("smart stack", 1, ImGuiTableFlags_ScrollY)) {
			for (uint16_t i = state6502.sp_depth - 1; i < state6502.sp_depth; --i) {
				ImGui::TableNextColumn();
				auto do_label = [](uint16_t pc, uint8_t bank, bool allow_disabled) {
					char const *label = disasm_get_label(pc);
					bool        pushed = false;

					if (pc >= 0xa000) {
						if (label == nullptr) {
							ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
							char stack_line[256];
							snprintf(stack_line, sizeof(stack_line), "$%02X:$%04X", bank, pc);
							pushed = ImGui::Selectable(stack_line, false, 0, ImGui::CalcTextSize(stack_line));
							ImGui::PopStyleColor();
						} else {
							char stack_line[256];
							snprintf(stack_line, sizeof(stack_line), "$%02X:$%04X: %s", bank, pc, label);
							pushed = ImGui::Selectable(stack_line, false, 0, ImGui::CalcTextSize(stack_line));
						}
					} else {
						if (label == nullptr) {
							ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
							char stack_line[256];
							snprintf(stack_line, sizeof(stack_line), "$%04X", pc);
							pushed = ImGui::Selectable(stack_line, false, 0, ImGui::CalcTextSize(stack_line));
							ImGui::PopStyleColor();
						} else {
							char stack_line[256];
							snprintf(stack_line, sizeof(stack_line), "$%04X: %s", pc, label);
							pushed = ImGui::Selectable(stack_line, false, 0, ImGui::CalcTextSize(stack_line));
						}					
					}

					if (pushed) {
						disasm.set_dump_start(pc);
						if (pc >= 0xc000) {
							disasm.set_rom_bank(bank);
						} else if (pc >= 0xa000) {
							disasm.set_ram_bank(bank);
						}
					}
				};
				switch (stack6502[i].op_type) {
					case _stack_op_type::nmi:
						ImGui::PushStyleColor(ImGuiCol_TextDisabled, 0xFF003388);
						ImGui::PushStyleColor(ImGuiCol_Text, 0xFF0077FF);
						break;
					case _stack_op_type::irq:
						ImGui::PushStyleColor(ImGuiCol_TextDisabled, 0xFF007788);
						ImGui::PushStyleColor(ImGuiCol_Text, 0xFF00FFFF);
						break;
					case _stack_op_type::op:
						ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
						ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
						break;
					default:
						break;
				}
				ImGui::PushID(i);
				do_label(stack6502[i].dest_pc, stack6502[i].dest_bank, true);
				ImGui::PopID();
				ImGui::PopStyleColor(2);

				if (ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();

					if (ImGui::BeginTable("additional info table", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextDisabled("%s", "Source address:");
						ImGui::TableSetColumnIndex(1);
						do_label(stack6502[i].source_pc, stack6502[i].source_bank, false);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextDisabled("%s", "Destination address:");
						ImGui::TableSetColumnIndex(1);
						do_label(stack6502[i].dest_pc, stack6502[i].dest_bank, false);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextDisabled("%s", "Cause:");
						ImGui::TableSetColumnIndex(1);

						switch (stack6502[i].op_type) {
							case _stack_op_type::nmi:
								ImGui::Text("%s", "NMI");
								break;
							case _stack_op_type::irq:
								ImGui::Text("%s", "IRQ");
								break;
							case _stack_op_type::op:
								ImGui::Text("%s", mnemonics[stack6502[i].opcode]);
								break;
							default:
								break;
						}

						ImGui::EndTable();
					}

					ImGui::EndTooltip();
				}
			}
			ImGui::EndTable();
		}
	}
	ImGui::EndTable();
}

static void draw_debugger_cpu_visualizer()
{
	ImGui::PushItemWidth(128.0f);
	static const char *color_labels[] = {
		"PC Address",
		"CPU Op",
		"Rainbow (Test)"
	};

	int c = cpu_visualization_get_coloring();
	if (ImGui::BeginCombo("Colorization", color_labels[c])) {
		for (int i = 0; i < 3; ++i) {
			if (ImGui::Selectable(color_labels[i], i == c)) {
				cpu_visualization_set_coloring((cpu_visualization_coloring)i);
			}
		}
		ImGui::EndCombo();
	}

	static const char *vis_labels[] = {
		"None",
		"IRQ",
		"Scan-On",
		"Scan-Off",
	};

	int h = cpu_visualization_get_highlight();
	if (ImGui::BeginCombo("Highlight type", vis_labels[h])) {
		for (int i = 0; i < 4; ++i) {
			if (ImGui::Selectable(vis_labels[i], i == h)) {
				cpu_visualization_set_highlight((cpu_visualization_highlight)i);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	static icon_set vis;
	vis.load_memory(cpu_visualization_get_framebuffer(), SCAN_WIDTH, SCAN_HEIGHT, SCAN_WIDTH, SCAN_HEIGHT);

	ImVec2 vis_imsize(SCAN_WIDTH, SCAN_HEIGHT);
	ImGui::Image((void *)(intptr_t)vis.get_texture_id(), ImGui::GetContentRegionAvail(), vis.get_top_left(0), vis.get_bottom_right(0));
}

static void draw_debugger_vera_status()
{
	ImGui::BeginGroup();
	{
		ImGui::TextDisabled("VERA Settings");
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(0.0f, 19.0f));
		ImGui::Separator();

		// ImGuiInputTextFlags_ReadOnly

		char hex[7];

		{
			uint32_t value;

			value = vera_video_get_data_addr(0);
			if (ImGui::InputHexLabel<uint32_t, 24>("Data0 Address", value)) {
				vera_video_set_data_addr(0, value);
			}

			value = vera_video_get_data_addr(1);
			if (ImGui::InputHexLabel<uint32_t, 24>("Data1 Address", value)) {
				vera_video_set_data_addr(1, value);
			}

			ImGui::NewLine();

			value = vera_debug_video_read(3);
			if (ImGui::InputHexLabel<uint32_t, 24>("Data0", value)) {
				vera_video_space_write(vera_video_get_data_addr(0), value);
			}

			value = vera_debug_video_read(4);
			if (ImGui::InputHexLabel<uint32_t, 24>("Data1", value)) {
				vera_video_space_write(vera_video_get_data_addr(1), value);
			}
		}

		ImGui::NewLine();

		ImGui::PushItemWidth(width_uint8);
		{
			uint8_t dc_video       = vera_video_get_dc_video();
			uint8_t dc_video_start = dc_video;

			static constexpr const char *modes[] = { "Disabled", "VGA", "NTSC", "RGB interlaced, composite, via VGA connector" };

			ImGui::Text("Output Mode");
			ImGui::SameLine();

			if (ImGui::BeginCombo(modes[dc_video & 3], modes[dc_video & 3])) {
				for (uint8_t i = 0; i < 4; ++i) {
					const bool selected = ((dc_video & 3) == i);
					if (ImGui::Selectable(modes[i], selected)) {
						dc_video = (dc_video & ~3) | i;
					}
				}
				ImGui::EndCombo();
			}

			// Other dc_video flags
			{
				static constexpr struct {
					const char *name;
					uint8_t     flag;
				} video_options[] = { { "No Chroma", 0x04 }, { "Layer 0", 0x10 }, { "Layer 1", 0x20 }, { "Sprites", 0x40 } };

				for (auto &option : video_options) {
					bool selected = dc_video & option.flag;
					if (ImGui::Checkbox(option.name, &selected)) {
						dc_video ^= option.flag;
					}
				}
			}

			if (dc_video_start != dc_video) {
				vera_video_set_dc_video(dc_video);
			}
		}
		ImGui::NewLine();
		{
			ImGui::Text("Scale");
			ImGui::SameLine();

			sprintf(hex, "%02X", (int)vera_video_get_dc_hscale());
			if (ImGui::InputText("H", hex, 5, hex_flags)) {
				vera_video_set_dc_hscale(parse<8>(hex));
			}

			ImGui::SameLine();

			sprintf(hex, "%02X", (int)vera_video_get_dc_vscale());
			if (ImGui::InputText("V", hex, 3, hex_flags)) {
				vera_video_set_dc_vscale(parse<8>(hex));
			}
		}

		ImGui::Text("DC Borders");
		ImGui::Dummy(ImVec2(width_uint8, 0));
		ImGui::SameLine();
		ImGui::PushID("vstart");
		sprintf(hex, "%02X", (int)vera_video_get_dc_vstart());
		if (ImGui::InputText("", hex, 3, hex_flags)) {
			vera_video_set_dc_vstart(parse<8>(hex));
		}
		ImGui::PopID();
		ImGui::PushID("hstart");
		sprintf(hex, "%02X", (int)vera_video_get_dc_hstart());
		if (ImGui::InputText("", hex, 3, hex_flags)) {
			vera_video_set_dc_hstart(parse<8>(hex));
		}
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(width_uint8, 0));
		ImGui::SameLine();
		ImGui::PushID("hstop");
		sprintf(hex, "%02X", (int)vera_video_get_dc_hstop());
		if (ImGui::InputText("", hex, 3, hex_flags)) {
			vera_video_set_dc_hstop(parse<8>(hex));
		}
		ImGui::PopID();
		ImGui::Dummy(ImVec2(width_uint8, 0));
		ImGui::SameLine();
		ImGui::PushID("vstop");
		sprintf(hex, "%02X", (int)vera_video_get_dc_vstop());
		if (ImGui::InputText("", hex, 3, hex_flags)) {
			vera_video_set_dc_vstop(parse<8>(hex));
		}
		ImGui::PopID();

		ImGui::PopItemWidth();
	}
	ImGui::EndGroup();
}

static void draw_debugger_vera_palette()
{
	ImGui::BeginGroup();
	{
		ImGui::TextDisabled("Palette");
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(0.0f, 19.0f));
		ImGui::Separator();

		const uint32_t *palette = vera_video_get_palette_argb32();
		static ImVec4   backup_color;
		static ImVec4   picker_color;
		static int      picker_index = 0;

		for (int i = 0; i < 256; ++i) {
			const uint8_t *p = reinterpret_cast<const uint8_t *>(&palette[i]);
			ImVec4         c{ (float)(p[2]) / 255.0f, (float)(p[1]) / 255.0f, (float)(p[0]) / 255.0f, 1.0f };
			ImGui::PushID(i);
			if (ImGui::VERAColorButton("Color##3f", c, ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoAlpha, ImVec2(16, 16))) {
				ImGui::OpenPopup("palette_picker");
				backup_color = c;
				picker_color = c;
				picker_index = i;
			}

			if (ImGui::BeginPopup("palette_picker")) {
				if (ImGui::VERAColorPicker3("##picker", (float *)&picker_color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueWheel)) {
					c = picker_color;
				}
				ImGui::SameLine();

				ImGui::BeginGroup(); // Lock X position
				ImGui::Text("Current");
				ImGui::VERAColorButton("##current", c, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoAlpha, ImVec2(60, 40));
				ImGui::Text("Previous");
				if (ImGui::VERAColorButton("##previous", backup_color, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoAlpha, ImVec2(60, 40))) {
					c = backup_color;
				}

				float   *f = (float *)(&c);
				uint32_t nc;
				uint8_t *np = reinterpret_cast<uint8_t *>(&nc);
				np[0]       = (uint8_t)(f[3] * 15.0f);
				np[1]       = (uint8_t)(f[2] * 15.0f);
				np[2]       = (uint8_t)(f[1] * 15.0f);
				np[3]       = (uint8_t)(f[0] * 15.0f);
				nc |= nc << 4;
				vera_video_set_palette(picker_index, nc);
				c = { (float)(np[2]) / 255.0f, (float)(np[1]) / 255.0f, (float)(np[0]) / 255.0f, 1.0f };
				ImGui::EndGroup();
				ImGui::EndPopup();
			}
			ImGui::PopID();

			if (i % 16 != 15) {
				ImGui::SameLine();
			}
		}
	}
	ImGui::EndGroup();
}

static void vera_video_get_expanded_vram_with_wraparound_handling(uint32_t address, int bpp, uint8_t *dest, uint32_t dest_size)
{
	// vera_video_get_expanded_vram doesn't handle the case where the VRAM address is above 0x1FFFF and wraps back to 0
	while (dest_size > 0) {
		const uint32_t this_run = std::min((0x20000 - address) * 8 / bpp, dest_size);
		vera_video_get_expanded_vram(address, bpp, dest, this_run);
		address = 0;
		dest += this_run;
		dest_size -= this_run;
	}
}

template <typename T>
constexpr T ceil_div_int(T a, T b)
{
	return (a + b - 1) / b;
}

static const ImVec2 fit_size(float src_w, float src_h, float dst_w, float dst_h)
{
	const float aspect = src_w / src_h;
	if (aspect > 1)
		return ImVec2(dst_w, dst_h / aspect);
	else
		return ImVec2(dst_w * aspect, dst_h);
}

static const std::array<ImVec2, 2> sprite_to_uvs(int id, float width, float height)
{
	float y = id / 128.f;
	return std::array<ImVec2, 2>{ ImVec2(0, y), ImVec2(width / 64.f, y + height / 64.f / 128.f) };
}

static const void add_selection_rect(ImDrawList *draw_list, float x, float y, float width, float height)
{
	const float x2 = x + width;
	const float y2 = y + height;
	draw_list->AddRect(ImVec2(x - 2, y - 2), ImVec2(x2 + 2, y2 + 2), IM_COL32_BLACK);
	draw_list->AddRect(ImVec2(x - 1, y - 1), ImVec2(x2 + 1, y2 + 1), IM_COL32_WHITE);
}

static void draw_debugger_vera_sprite()
{
	auto to_size_bits = [](int a) -> int {
		if (a >= 64)
			return 3;
		if (a >= 32)
			return 2;
		if (a >= 16)
			return 1;
		return 0;
	};

	static struct SpriteListItem {
		vera_video_sprite_properties prop;
		bool                         off_screen;
	} sprites[128];
	std::vector<int> sprite_table_entries;

	static icon_set        sprite_preview;
	static uint32_t        sprite_pixels[64 * 64 * 128];
	static uint8_t         buf_pixels[64 * 64];
	static uint32_t        palette[256]{ 0 };
	static const uint32_t *palette_argb = vera_video_get_palette_argb32();

	static uint8_t  sprite_id  = 0;
	static uint64_t sprite_sig = 0;

	static bool reload         = true;
	static bool hide_disabled  = false;
	static bool hide_offscreen = false;
	static bool show_entire    = false;
	static bool show_depths[4]{ false, true, true, true };

	static float screen_width  = (float)(vera_video_get_dc_hstop() - vera_video_get_dc_hstart()) * vera_video_get_dc_hscale() / 32.f;
	static float screen_height = (float)(vera_video_get_dc_vstop() - vera_video_get_dc_vstart()) * vera_video_get_dc_vscale() / 64.f;

	// initial work, scan all sprites data and render
	sprite_table_entries.clear();
	// skip color 0, it will always be transparent
	for (int i = 1; i < 256; i++) {
		palette[i] = (palette_argb[i] << 8) | 0xFF;
	}
	for (int i = 0; i < 128; i++) {
		auto spr = &sprites[i];
		memcpy(&spr->prop, vera_video_get_sprite_properties(i), sizeof(vera_video_sprite_properties));
		const uint8_t width  = spr->prop.sprite_width;
		const uint8_t height = spr->prop.sprite_height;
		const bool    hflip  = spr->prop.hflip;
		const bool    vflip  = spr->prop.vflip;
		uint16_t      box[4]{
			     (uint16_t)((spr->prop.sprite_x) & 0x3FF),
			     (uint16_t)((spr->prop.sprite_x + width) & 0x3FF),
			     (uint16_t)((spr->prop.sprite_y) & 0x3FF),
			     (uint16_t)((spr->prop.sprite_y + height) & 0x3FF),
		}; // l, r, t, b
		// this might sounds hacky but it works
		if (box[1] < box[0])
			box[0] = 0;
		if (box[3] < box[2])
			box[2] = 0;
		spr->off_screen = (box[0] >= screen_width && box[1] >= screen_width) || (box[2] >= screen_height && box[3] >= screen_height);
		if (!((hide_disabled && (spr->prop.sprite_zdepth == 0)) || (hide_offscreen && spr->off_screen)))
			sprite_table_entries.push_back(i);

		uint32_t *dstpix = &sprite_pixels[i * 64 * 64];
		int       src    = 0;
		vera_video_get_expanded_vram_with_wraparound_handling(spr->prop.sprite_address, spr->prop.color_mode ? 8 : 4, buf_pixels, width * height);
		for (int i = 0; i < height; i++) {
			int dst     = vflip ? (height - i - 1) * 64 : i * 64;
			int dst_add = 1;
			if (hflip) {
				dst += width - 1;
				dst_add = -1;
			}
			if (spr->prop.color_mode) {
				for (int j = 0; j < width; j++) {
					uint8_t val = buf_pixels[src++];
					dstpix[dst] = palette[val];
					dst += dst_add;
				}
			} else {
				for (int j = 0; j < width; j++) {
					uint8_t val = buf_pixels[src++];
					if (val) {
						val += spr->prop.palette_offset;
					}
					dstpix[dst] = palette[val];
					dst += dst_add;
				}
			}
		}
	}
	sprite_preview.load_memory(sprite_pixels, 64, 64 * 128, 64, 64 * 128);

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 0));
	if (ImGui::BeginTable("sprite debugger", 2, ImGuiTableFlags_Resizable)) {

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		// overview
		ImGui::BeginGroup();
		ImGui::TextDisabled("Preview");

		ImVec2 avail = ImGui::GetContentRegionAvail();
		avail.y -= 24;
		ImGui::BeginChild("sprite overview", avail, false, ImGuiWindowFlags_HorizontalScrollbar);
		{
			const ImVec2 scrsize   = show_entire ? ImVec2(1024, 1024) : ImVec2(screen_width, screen_height);
			ImDrawList  *draw_list = ImGui::GetWindowDrawList();
			const ImVec2 topleft   = ImGui::GetCursorScreenPos();
			ImGui::Dummy(scrsize);
			const ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
			ImVec2       winsize = ImGui::GetWindowSize();
			winsize.x            = std::min(scrsize.x, winsize.x);
			winsize.y            = std::min(scrsize.y, winsize.y);
			ImVec2 wintopleft    = topleft;
			wintopleft.x += scroll.x;
			wintopleft.y += scroll.y;
			ImVec2 winbotright(wintopleft.x + winsize.x, wintopleft.y + winsize.y);

			void *tex = (void *)(intptr_t)sprite_preview.get_texture_id();

			draw_list->AddRectFilled(topleft, ImVec2(topleft.x + screen_width, topleft.y + screen_height), IM_COL32(0x7F, 0x7F, 0x7F, 0x7F));
			draw_list->PushClipRect(wintopleft, winbotright, true);
			ImGui::SetCursorScreenPos(topleft);
			ImGui::BeginChild("i need to really clip this", scrsize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
			for (int z = 0; z < 4; z++) {
				if (!show_depths[z])
					continue;
				for (int i = 127; i >= 0; i--) {
					if (i == sprite_id)
						continue; // drawn later
					auto spr = &sprites[i];
					if (spr->prop.sprite_zdepth != z)
						continue;
					if (!show_entire && spr->off_screen)
						continue;
					const ImVec2 pos((spr->prop.sprite_x & 0x3FFu) + topleft.x, (spr->prop.sprite_y & 0x3FFu) + topleft.y);
					const ImVec2 size(spr->prop.sprite_width, spr->prop.sprite_height);
					const auto   uv = sprite_to_uvs(i, size.x, size.y);
					ImGui::PushID(i);
					for (int j = 0; j < 4; j++) {
						ImVec2 pos_tmp = pos;
						if (j & 1)
							pos_tmp.x -= 1024;
						if (j & 2)
							pos_tmp.y -= 1024;
						ImGui::PushID(j);
						draw_list->AddImage(tex, pos_tmp, ImVec2(pos_tmp.x + size.x, pos_tmp.y + size.y), uv[0], uv[1]);
						ImGui::SetCursorScreenPos(pos_tmp);
						if (ImGui::InvisibleButton("", size)) {
							sprite_id = i;
						}
						ImGui::PopID();
					}
					ImGui::PopID();
				}
			}
			// draw currently selected sprite last, guaranteed to always be on top
			auto         spr = &sprites[sprite_id];
			const ImVec2 pos((spr->prop.sprite_x & 0x3FFu) + topleft.x, (spr->prop.sprite_y & 0x3FFu) + topleft.y);
			const ImVec2 size(spr->prop.sprite_width, spr->prop.sprite_height);
			if (show_depths[spr->prop.sprite_zdepth] && (show_entire || !spr->off_screen)) {
				const auto uv        = sprite_to_uvs(sprite_id, size.x, size.y);
				auto       add_image = [draw_list, tex, pos, size, uv](float add_x, float add_y) {
                    draw_list->AddImage(tex, ImVec2(pos.x + add_x, pos.y + add_y), ImVec2(pos.x + add_x + size.x, pos.y + add_y + size.y), uv[0], uv[1]);
				};
				add_image(0, 0);
				add_image(0, -1024);
				add_image(-1024, 0);
				add_image(-1024, -1024);
			}
			ImGui::EndChild();
			draw_list->PopClipRect();

			if (show_entire) {
				// - 0
				// 1 1
				ImU32 col = IM_COL32(0, 0, 0, 0x7F);
				draw_list->AddRectFilled(ImVec2(topleft.x + screen_width, topleft.y), ImVec2(topleft.x + 1024, topleft.y + screen_height), col);
				draw_list->AddRectFilled(ImVec2(topleft.x, topleft.y + screen_height), ImVec2(topleft.x + 1024, topleft.y + 1024), col);
			}

			if (show_entire || !spr->off_screen) {
				add_selection_rect(draw_list, pos.x, pos.y, size.x, size.y);
				add_selection_rect(draw_list, pos.x, pos.y - 1024, size.x, size.y);
				add_selection_rect(draw_list, pos.x - 1024, pos.y, size.x, size.y);
				add_selection_rect(draw_list, pos.x - 1024, pos.y - 1024, size.x, size.y);
			}

			ImGui::EndChild();
		}
		ImGui::Text("Show Depths:");
		ImGui::SameLine();
		ImGui::Checkbox("0", &show_depths[0]);
		ImGui::SameLine();
		ImGui::Checkbox("1", &show_depths[1]);
		ImGui::SameLine();
		ImGui::Checkbox("2", &show_depths[2]);
		ImGui::SameLine();
		ImGui::Checkbox("3", &show_depths[3]);
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::Checkbox("Show Entire Sprite Plane", &show_entire);
		ImGui::EndGroup();

		ImGui::TableNextColumn();

		// sprites table
		// TODO sorting
		const ImVec4 normal_col   = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		const ImVec4 disabled_col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
		const float  height_avail = ImGui::GetContentRegionAvail().y;
		ImGui::TextDisabled("Sprite List");
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		if (ImGui::BeginTable("sprites", 11, ImGuiTableFlags_BordersInner | ImGuiTableFlags_ScrollY /* | ImGuiTableFlags_Sortable */, ImVec2(0.f, height_avail - 84))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 16); // thumbnail
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 20);
			ImGui::TableSetupColumn("X");
			ImGui::TableSetupColumn("Y");
			ImGui::TableSetupColumn("W");
			ImGui::TableSetupColumn("H");
			ImGui::TableSetupColumn("Base");
			ImGui::TableSetupColumn("Pri.");
			ImGui::TableSetupColumn("Pal.");
			ImGui::TableSetupColumn("Flags");
			ImGui::TableSetupColumn("Coll.");
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin((int)sprite_table_entries.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					const int     id     = sprite_table_entries[row];
					const int     dst    = 0x1FC00 + 8 * id;
					const uint8_t b1     = vera_video_space_read(dst + 1);
					const uint8_t b6     = vera_video_space_read(dst + 6);
					const uint8_t b7     = vera_video_space_read(dst + 7);
					auto          spr    = &sprites[id];
					int           width  = spr->prop.sprite_width;
					int           height = spr->prop.sprite_height;

					const bool hidden = spr->prop.sprite_zdepth == 0 || spr->off_screen;
					ImGui::PushID(id);
					ImGui::PushStyleColor(ImGuiCol_Text, hidden ? disabled_col : normal_col);
					ImGui::TableNextRow();
					// Thumbnail
					ImGui::TableNextColumn();
					void        *tex    = (void *)(intptr_t)sprite_preview.get_texture_id();
					const float  flt_w  = (float)width;
					const float  flt_h  = (float)height;
					const ImVec2 th_pos = ImGui::GetCursorScreenPos();
					const ImVec2 size   = fit_size(flt_w, flt_h, 16, 16);
					const auto   uv     = sprite_to_uvs(id, flt_w, flt_h);
					ImGui::Dummy(ImVec2(16, 16));
					if (ImGui::IsItemHovered()) {
						const ImVec2 zoomed_size = fit_size(flt_w, flt_h, 128, 128);
						ImGui::BeginTooltip();
						ImGui::Image(tex, zoomed_size, uv[0], uv[1]);
						ImGui::EndTooltip();
					}
					ImGui::SetCursorScreenPos(th_pos);
					ImGui::Image(tex, size, uv[0], uv[1]);
					// #
					ImGui::TableNextColumn();
					char idx_txt[4];
					sprintf(idx_txt, "%d", id);
					// SpanAllColumns flag currently makes selectable has more precedence than all edit widgets
					if (ImGui::Selectable(idx_txt, sprite_id == id /*, ImGuiSelectableFlags_SpanAllColumns */)) {
						sprite_id = id;
					}
					// X
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					// data type is signed to allow wrapping to "negative" offset
					if (ImGui::InputScalar("xx", ImGuiDataType_S16, &spr->prop.sprite_x, nullptr, nullptr, "%d")) {
						vera_video_space_write(dst + 2, spr->prop.sprite_x & 0xFF);
						vera_video_space_write(dst + 3, spr->prop.sprite_x >> 8);
					}
					// Y
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputScalar("yy", ImGuiDataType_S16, &spr->prop.sprite_y, nullptr, nullptr, "%d")) {
						vera_video_space_write(dst + 4, spr->prop.sprite_y & 0xFF);
						vera_video_space_write(dst + 5, spr->prop.sprite_y >> 8);
					}
					// Width
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputInt("wid", &width, 0, 0)) {
						vera_video_space_write(dst + 7, b7 & ~0x30 | (to_size_bits(width) << 4));
					};
					// Height
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputInt("hei", &height, 0, 0)) {
						vera_video_space_write(dst + 7, b7 & ~0xC0 | (to_size_bits(height) << 6));
					};
					// Base
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputScalar("bas", ImGuiDataType_U32, &spr->prop.sprite_address, nullptr, nullptr, "%X", ImGuiInputTextFlags_CharsHexadecimal)) {
						spr->prop.sprite_address &= 0x1FFE0;
						vera_video_space_write(dst + 0, (spr->prop.sprite_address >> 5) & 0xFF);
						vera_video_space_write(dst + 1, (spr->prop.sprite_address >> 13) | (b1 & 0x80));
					}
					// Priority
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputScalar("pri", ImGuiDataType_U8, &spr->prop.sprite_zdepth, nullptr, nullptr, "%d")) {
						if (spr->prop.sprite_zdepth >= 3)
							spr->prop.sprite_zdepth = 3;
						vera_video_space_write(dst + 6, b6 & ~0x0C | (spr->prop.sprite_zdepth << 2));
					}
					// Palette
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					uint8_t pal = spr->prop.palette_offset / 16;
					if (ImGui::InputScalar("pal", ImGuiDataType_U8, &pal, nullptr, nullptr, "%d")) {
						if (pal >= 15)
							pal = 15;
						vera_video_space_write(dst + 7, b7 & ~0x0F | pal);
					}
					// Flags
					ImGui::TableNextColumn();
					const uint8_t mask_8       = (1 << 7);
					const uint8_t mask_h       = (1 << 0);
					const uint8_t mask_v       = (1 << 1);
					char          flags_txt[4] = "";
					if (b1 & mask_8)
						strcat(flags_txt, "8");
					if (b6 & mask_h)
						strcat(flags_txt, "H");
					if (b6 & mask_v)
						strcat(flags_txt, "V");
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputText("flg", flags_txt, 4)) {
						uint8_t b1_new = b1 & ~mask_8;
						uint8_t b6_new = b6 & ~mask_h & ~mask_v;
						if (strchr(flags_txt, '8'))
							b1_new |= mask_8;
						if (strchr(flags_txt, 'H') || strchr(flags_txt, 'h'))
							b6_new |= mask_h;
						if (strchr(flags_txt, 'V') || strchr(flags_txt, 'v'))
							b6_new |= mask_v;
						vera_video_space_write(dst + 1, b1_new);
						vera_video_space_write(dst + 6, b6_new);
					}
					// Collision
					ImGui::TableNextColumn();
					// a bit of hack is done here with decimal digits
					uint16_t coll = 0;
					if (spr->prop.sprite_collision_mask & 0x10)
						coll += 1;
					if (spr->prop.sprite_collision_mask & 0x20)
						coll += 10;
					if (spr->prop.sprite_collision_mask & 0x40)
						coll += 100;
					if (spr->prop.sprite_collision_mask & 0x80)
						coll += 1000;
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::InputScalar("coll", ImGuiDataType_U16, &coll, nullptr, nullptr, "%04d")) {
						uint8_t val = b6 & ~0xF0;
						if (coll % 10 != 0)
							val |= 0x10;
						if ((coll / 10) % 10 != 0)
							val |= 0x20;
						if ((coll / 100) % 10 != 0)
							val |= 0x40;
						if ((coll / 1000) % 10 != 0)
							val |= 0x80;
						vera_video_space_write(dst + 6, val);
					}

					ImGui::PopStyleColor();
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleVar(2);

		ImGui::Checkbox("Hide Disabled", &hide_disabled);
		ImGui::SameLine();
		ImGui::Checkbox("Hide Off-screen", &hide_offscreen);

		// Raw Bytes section, just to keep some people happy
		ImGui::BeginGroup();
		{
			const uint32_t addr = 0x1FC00 + 8 * sprite_id;
			uint8_t        sprite_data[8];
			ImGui::TextDisabled("Raw Bytes (Selected Sprite)");
			ImGui::Text("#%d:", sprite_id);
			ImGui::SameLine(40);
			vera_video_space_read_range(sprite_data, addr, 8);
			for (int i = 0; i < 8; ++i) {
				if (i != 0)
					ImGui::SameLine();
				if (ImGui::InputHex(i, sprite_data[i])) {
					vera_video_space_write(addr + i, sprite_data[i]);
				}
			}
		}
		ImGui::EndGroup();

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();
}

class vram_visualizer
{
public:
	void draw_preview()
	{
		ImGui::BeginGroup();
		ImGui::TextDisabled("Preview");

		ImVec2 avail = ImGui::GetContentRegionAvail();
		avail.x -= 256;
		ImGui::BeginChild("tiles", avail, false, ImGuiWindowFlags_HorizontalScrollbar);
		{
			if (!active_exist || active.tile_height == 0 || active.view_size == 0) {
				ImGui::EndChild();
				ImGui::EndGroup();
				return;
			}
			const int    scale              = 2;
			const int    tile_height        = active.tile_height;
			const int    tile_width_scaled  = tile_width * scale;
			const int    tile_height_scaled = tile_height * scale;
			const int    view_columns       = active.view_columns;
			const int    view_rows          = ceil_div_int((int)num_tiles, view_columns);
			const int    total_width        = tile_width * view_columns * scale;
			const int    total_height       = view_rows * active.tile_height * scale;
			ImDrawList  *draw_list          = ImGui::GetWindowDrawList();
			const ImVec2 topleft            = ImGui::GetCursorScreenPos();
			// since the view range can be very large, the dummy square is drawn first to provide a scroll range
			// then the preview is partially rendered later
			ImGui::Dummy(ImVec2((float)total_width, (float)total_height));
			const ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
			ImVec2       winsize = ImGui::GetWindowSize();
			winsize.x            = std::min((float)total_width, winsize.x);
			winsize.y            = std::min((float)total_height, winsize.y);
			ImVec2 wintopleft    = topleft;
			wintopleft.x += scroll.x;
			wintopleft.y += scroll.y;
			ImVec2 winbotright(wintopleft.x + winsize.x, wintopleft.y + winsize.y);
			ImVec2 mouse_pos = ImGui::GetMousePos();
			mouse_pos.x -= topleft.x;
			mouse_pos.y -= topleft.y;
			const int starting_tile_x = (int)floorf(scroll.x / tile_width_scaled);
			const int starting_tile_y = (int)floorf(scroll.y / tile_height_scaled);
			const int tiles_count_x   = (int)ceilf((scroll.x + winsize.x) / tile_width_scaled) - starting_tile_x;
			const int tiles_count_y   = (int)ceilf((scroll.y + winsize.y) / tile_height_scaled) - starting_tile_y;
			const int render_width    = tiles_count_x * tile_width;
			const int render_height   = tiles_count_y * tile_height;

			// capture ram
			uint32_t              palette[256];
			const uint32_t       *palette_argb = vera_video_get_palette_argb32();
			std::vector<uint8_t>  data((size_t)view_columns * view_rows * tile_size, 0);
			std::vector<uint32_t> pixels((size_t)tiles_count_x * tiles_count_y * tile_width * tile_height, 0);
			uint8_t              *data_   = data.data();
			uint32_t             *pixels_ = pixels.data();
			for (int i = 0; i < 256; i++) {
				// convert argb to rgba
				palette[i] = (palette_argb[i] << 8) | 0xFF;
			}
			switch (active.mem_source) {
				case 1:
					for (uint32_t i = 0; i < active.view_size; i++)
						data_[i] = debug_read6502(active.view_address + i);
					break;
				case 2:
					for (uint32_t i = 0; i < active.view_size; i++) {
						const uint32_t addr = active.view_address + i;
						data_[i]            = debug_read6502((addr & 0x1FFF) + 0xA000, addr >> 13);
					}
					break;
				default:
					vera_video_space_read_range(data_, active.view_address, active.view_size);
			}
			static const int shifts[4][8] = {
				{ 7, 6, 5, 4, 3, 2, 1, 0 },
				{ 6, 4, 2, 0, 6, 4, 2, 0 },
				{ 4, 0, 4, 0, 4, 0, 4, 0 },
				{ 0, 0, 0, 0, 0, 0, 0, 0 },
			};
			const uint32_t fg_col     = palette[active.view_fg_col];
			const uint32_t bg_col     = palette[active.view_bg_col];
			const int     *shift      = shifts[active.color_depth];
			const int      bpp_mod    = (8 >> active.color_depth) - 1;
			const uint8_t  bpp_mask   = (1 << bpp) - 1;
			const uint8_t  pal_offset = active.view_pal * 16;
			int            src        = 0;
			for (int mi = 0; mi < tiles_count_y; mi++) {
				for (int mj = 0; mj < tiles_count_x; mj++) {
					int       src = (mj + starting_tile_x + (mi + starting_tile_y) * active.view_columns) * tile_size;
					const int dst = mj * tile_width + mi * tile_height * render_width;
					for (int ti = 0; ti < tile_height; ti++) {
						int dst2 = dst + ti * render_width;
						for (int tj = 0; tj < (int)tile_width; tj += 8) {
							if (src >= (int)active.view_size)
								break;
							uint8_t buf;
							if (active.color_depth == 0) {
								// 1bpp
								buf = data_[src++];
								for (int k = 0; k < 8; k++) {
									pixels_[dst2++] = buf & 0x80 ? fg_col : bg_col;
									buf <<= 1;
								}
							} else {
								for (int k = 0; k < 8; k++) {
									if ((k & bpp_mod) == 0)
										buf = data_[src++];
									uint8_t col = (buf >> shift[k]) & bpp_mask;
									if (col > 0 && col < 16)
										col += pal_offset;
									pixels_[dst2++] = palette[col];
								}
							}
						}
					}
				}
			}
			tiles_preview.load_memory(pixels.data(), render_width, render_height, render_width, render_height);

			if (ImGui::IsItemHovered()) {
				if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					cur_tile = ((int)mouse_pos.x / tile_width_scaled) + ((int)mouse_pos.y / tile_height_scaled) * view_columns;
				}
			}
			draw_list->PushClipRect(wintopleft, winbotright, true);
			draw_list->AddImage((void *)(intptr_t)tiles_preview.get_texture_id(), ImVec2(topleft.x + (float)(starting_tile_x * tile_width_scaled), (float)(topleft.y + starting_tile_y * tile_height_scaled)), ImVec2(topleft.x + (float)((starting_tile_x + tiles_count_x) * tile_width_scaled), topleft.y + (float)((starting_tile_y + tiles_count_y) * tile_height_scaled)));
			if (show_grid) {
				const uint32_t col  = IM_COL32(0x08, 0x7F, 0xF6, 0xFF);
				float          hcnt = starting_tile_x * tile_width_scaled + topleft.x;
				while (hcnt < winbotright.x) {
					draw_list->AddLine(ImVec2(hcnt, wintopleft.y), ImVec2(hcnt, winbotright.y), col);
					hcnt += tile_width_scaled;
				}
				float vcnt = starting_tile_y * tile_height_scaled + topleft.y;
				while (vcnt < winbotright.y) {
					draw_list->AddLine(ImVec2(wintopleft.x, vcnt), ImVec2(winbotright.x, vcnt), col);
					vcnt += tile_height_scaled;
				}
			}
			draw_list->PopClipRect();
			// selected tile indicator
			const float sel_x = (cur_tile % view_columns) * tile_width_scaled + topleft.x;
			const float sel_y = (cur_tile / view_columns) * tile_height_scaled + topleft.y;
			add_selection_rect(draw_list, sel_x, sel_y, (float)tile_width_scaled, (float)tile_height_scaled);
			ImGui::EndChild();
		}
		ImGui::EndGroup();
	}

	void draw_preview_widgets()
	{
		ImGui::BeginGroup();
		ImGui::TextDisabled("Graphics Properties");

		ImGui::PushItemWidth(128.0f);

		static const char *source_txts[] = { "VERA Memory", "CPU Memory", "High RAM" };
		if (ImGui::BeginCombo("Source", source_txts[active.mem_source])) {
			for (int i = 0; i < 3; i++) {
				const bool selected = active.mem_source == i;
				if (ImGui::Selectable(source_txts[i], selected))
					active.mem_source = i;
				if (selected)
					ImGui::SetItemDefaultFocus();
				if (i == 0)
					ImGui::Separator();
			}
			ImGui::EndCombo();
		}
		static const char *depths_txt[]{ "1", "2", "4", "8" };
		ImGui::Combo("Color Depth", &active.color_depth, depths_txt, 4);
		static const char *tile_width_txt[]{ "8", "16", "32", "64", "320", "640" };
		ImGui::Combo("Tile Width", &active.tile_w_sel, tile_width_txt, 6);
		ImGui::InputInt("Tile Height", &active.tile_height, 8, 16);
		if (active.color_depth == 0) {
			ImGui::InputInt("FG Color", &active.view_fg_col, 1, 16);
			ImGui::InputInt("BG Color", &active.view_bg_col, 1, 16);
		} else {
			ImGui::InputInt("Palette", &active.view_pal, 1, 4);
		}
		ImGui::NewLine();
		// could use InputInt here but there's no way to trim off leading zeros
		const uint32_t _0x800   = 0x800;
		const uint32_t _0x10000 = 0x10000;
		const uint32_t old_size = active.view_size;
		ImGui::InputScalar("Address", ImGuiDataType_U32, &active.view_address, &_0x800, &active.view_size, "%X", ImGuiInputTextFlags_CharsHexadecimal);
		if (ImGui::InputScalar("Size", ImGuiDataType_U32, &active.view_size, &_0x800, &_0x10000, "%X", ImGuiInputTextFlags_CharsHexadecimal)) {
			if (active.view_size > 1 && old_size == 1)
				active.view_size--;
		}
		ImGui::InputInt("Columns", &active.view_columns, 1, 4);
		ImGui::Checkbox("Show Tile Grid", &show_grid);

		// load settings
		ImGui::NewLine();
		ImGui::TextDisabled("Settings");
		bool save_clicked = ImGui::Button("Save");
		ImGui::SameLine();
		if (ImGui::Button("Load") && saved_exist) {
			active = saved;
		}
		if (ImGui::Button("Layer 0")) {
			import_settings_from_layer(0);
		}
		ImGui::SameLine();
		if (ImGui::Button("Layer 1") || !active_exist) {
			active_exist = true;
			import_settings_from_layer(1);
		}
		if (ImGui::Button("Sprite")) {
			auto      spr       = vera_video_get_sprite_properties(sprite_to_import);
			const int spr_size  = (spr->sprite_width * spr->sprite_height) >> (1 - spr->color_mode);
			active.mem_source   = 0;
			active.color_depth  = spr->color_mode ? 3 : 2;
			active.tile_w_sel   = spr->sprite_width_log2 - 3;
			active.tile_height  = spr->sprite_height;
			active.view_pal     = spr->palette_offset / 16;
			active.view_address = spr->sprite_address % spr_size;
			active.view_size    = 0x20000 - active.view_address;
			active.view_columns = 128 >> spr->sprite_width_log2;
			cur_tile            = (spr->sprite_address - active.view_address) / spr_size;
		}
		ImGui::SameLine();
		const uint8_t _1  = 1;
		const uint8_t _16 = 16;
		ImGui::PushID(0);
		if (ImGui::InputScalar("", ImGuiDataType_U8, &sprite_to_import, &_1, &_16, "%d")) {
			if (sprite_to_import > 127)
				sprite_to_import = 127;
		}
		ImGui::PopID();

		// validate settings
		const uint32_t   max_mem_sizes[]{ 0x20000, 0x10000, (uint32_t)Options.num_ram_banks * 8192 };
		static const int row_sizes[]{ 1, 2, 4, 8, 40, 80 };
		const uint32_t   max_mem_size = max_mem_sizes[active.mem_source];
		active.tile_height            = std::min(std::max(active.tile_height, 0), 1024);
		active.view_fg_col            = std::min(std::max(active.view_fg_col, 0), 255);
		active.view_bg_col            = std::min(std::max(active.view_bg_col, 0), 255);
		active.view_pal               = std::min(std::max(active.view_pal, 0), 15);
		active.view_size              = std::min(std::max(active.view_size, 1u), max_mem_size);
		active.view_address           = std::min(std::max(active.view_address, 0u), max_mem_size - active.view_size);
		active.view_columns           = std::min(std::max(active.view_columns, 0), 256);

		bpp        = 1 << active.color_depth;
		tile_width = row_sizes[active.tile_w_sel] * 8;
		tile_size  = row_sizes[active.tile_w_sel] * active.tile_height * bpp;
		num_tiles  = (tile_size > 0) ? ceil_div_int(active.view_size, tile_size) : 1;

		// save settings
		if (save_clicked) {
			saved       = active;
			saved_exist = true;
		}

		ImGui::NewLine();
		const int selected_addr = active.view_address + row_sizes[active.tile_w_sel] * active.tile_height * (1 << active.color_depth) * cur_tile;
		ImGui::LabelText("Tile Address", "%05X", selected_addr);

		ImGui::PopItemWidth();
		ImGui::EndGroup();
	}

	void import_settings_from_layer(int layer)
	{
		auto props          = vera_video_get_layer_properties(layer);
		active.mem_source   = 0;
		active.color_depth  = props->color_depth;
		active.view_address = props->tile_base;
		if (props->bitmap_mode) {
			active.tile_w_sel   = props->tilew == 320 ? 4 : 5;
			active.tile_height  = 8;
			active.view_size    = props->tilew * props->bits_per_pixel * 480 / 8;
			active.view_columns = 1;
			const uint8_t pal   = vera_video_get_layer_data(layer)[4] & 0x0F;
			if (active.color_depth == 0) {
				active.view_fg_col = pal * 16 + 1;
				active.view_bg_col = 0;
			} else {
				active.view_pal = pal;
			}
		} else {
			active.tile_w_sel   = props->tilew_log2 - 3;
			active.tile_height  = props->tileh;
			active.view_columns = 16;
			active.view_size    = props->tilew * props->tileh * props->bits_per_pixel / 8;
			active.view_size *= props->color_depth == 0 ? 256 : 1024;
		}
	}

private:
	icon_set tiles_preview;
	uint16_t tile_palette_offset = 0;
	uint8_t  sprite_to_import    = 0;
	uint32_t cur_tile            = 0;

	struct setting {
		int      mem_source;
		int      color_depth;
		int      tile_w_sel;
		int      tile_height;
		int      view_fg_col;
		int      view_bg_col;
		int      view_pal;
		uint32_t view_address;
		uint32_t view_size;
		int      view_columns;
	} active{ 0, 0, 0, 8, 1, 0, 0, 0, 0, 0 };
	setting saved;
	bool    active_exist;
	bool    saved_exist;
	bool    show_grid;

	// cached values
	uint8_t  bpp;
	uint32_t tile_width;
	uint32_t tile_size;
	uint32_t num_tiles;
};

class tmap_visualizer
{
public:
	void draw_preview()
	{
		capture_vram();

		ImGui::BeginGroup();
		ImGui::TextDisabled("Preview");

		ImVec2 avail = ImGui::GetContentRegionAvail();
		avail.x -= 256;
		avail.y -= 24;
		ImGui::BeginChild("tiles", avail, false, ImGuiWindowFlags_HorizontalScrollbar);
		{
			const ImVec2 topleft = ImGui::GetCursorScreenPos();
			ImGui::Image((void *)(intptr_t)tiles_preview.get_texture_id(), ImVec2(total_width, total_height));
			if (!bitmap_mode) {
				const ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
				ImDrawList  *draw_list = ImGui::GetWindowDrawList();
				ImVec2       winsize   = ImGui::GetWindowSize();
				winsize.x              = std::min((float)total_width, winsize.x);
				winsize.y              = std::min((float)total_height, winsize.y);
				ImVec2 wintopleft      = topleft;
				wintopleft.x += scroll.x;
				wintopleft.y += scroll.y;
				ImVec2 winbotright(wintopleft.x + winsize.x, wintopleft.y + winsize.y);
				ImVec2 mouse_pos = ImGui::GetMousePos();
				mouse_pos.x -= topleft.x;
				mouse_pos.y -= topleft.y;
				if (ImGui::IsItemHovered()) {
					if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						cur_tile = ((int)mouse_pos.x / tile_width) + ((int)mouse_pos.y / tile_height) * map_width;
					}
				}
				draw_list->PushClipRect(wintopleft, winbotright, true);
				if (!bitmap_mode && show_grid) {
					const uint32_t col  = IM_COL32(0x08, 0x7F, 0xF6, 0xFF);
					float          hcnt = floorf(scroll.x / tile_width) * tile_width + topleft.x;
					while (hcnt < winbotright.x) {
						draw_list->AddLine(ImVec2(hcnt, wintopleft.y), ImVec2(hcnt, winbotright.y), col);
						hcnt += tile_width;
					}
					float vcnt = floorf(scroll.y / tile_height) * tile_height + topleft.y;
					while (vcnt < winbotright.y) {
						draw_list->AddLine(ImVec2(wintopleft.x, vcnt), ImVec2(winbotright.x, vcnt), col);
						vcnt += tile_height;
					}
				}
				if (!bitmap_mode && show_scroll) {
					auto screen_rect = [this, draw_list](float start_x, float start_y) -> void {
						const ImVec2 p0(start_x, start_y);
						const ImVec2 p1(start_x + screen_width, start_y + screen_height);
						draw_list->AddRectFilled(p0, p1, IM_COL32(0xFF, 0xFF, 0xFF, 0x55));
						draw_list->AddRect(p0, p1, IM_COL32(0x4C, 0x4C, 0x4C, 0xFF));
					};
					const float base_x = topleft.x + scroll_x;
					const float base_y = topleft.y + scroll_y;
					screen_rect(base_x - total_width, base_y - total_height);
					screen_rect(base_x - total_width, base_y);
					screen_rect(base_x, base_y - total_height);
					screen_rect(base_x, base_y);
				}
				draw_list->PopClipRect();
				// selected tile indicator
				const float sel_x = (cur_tile % map_width) * tile_width + topleft.x;
				const float sel_y = (cur_tile / map_width) * tile_height + topleft.y;
				add_selection_rect(draw_list, sel_x, sel_y, tile_width, tile_height);
			}
			ImGui::EndChild();
		}
		ImGui::Checkbox("Show Tile Grid", &show_grid);
		ImGui::SameLine();
		ImGui::Checkbox("Show Scroll Overlay", &show_scroll);

		ImGui::EndGroup();
	}

	void capture_vram()
	{
		uint8_t               tile_data[640 * 480]; // 640*480 > 16*16*1024
		std::vector<uint32_t> pixels;
		uint32_t              palette[256];
		const uint32_t       *palette_argb = vera_video_get_palette_argb32();

		for (int i = 0; i < 256; i++) {
			// convert argb to rgba
			palette[i] = (palette_argb[i] << 8) | 0xFF;
		}

		// get DC registers and determine a screen size
		screen_width  = (float)(vera_video_get_dc_hstop() - vera_video_get_dc_hstart()) * vera_video_get_dc_hscale() / 32.f;
		screen_height = (float)(vera_video_get_dc_vstop() - vera_video_get_dc_vstart()) * vera_video_get_dc_vscale() / 64.f;

		if (bitmap_mode) {
			const uint32_t num_dots = tile_width * 480;
			pixels.resize(num_dots);
			vera_video_get_expanded_vram_with_wraparound_handling(tile_base, bpp, tile_data, num_dots);

			for (uint32_t i = 0; i < num_dots; i++) {
				uint8_t tdat = tile_data[i];
				if (tdat > 0 && tdat < 16) { // 8bpp quirk handling
					tdat += palette_offset;
					if (t256c) {
						tdat |= 0x80;
					}
				}
				pixels[i] = palette[tdat];
			}
		} else {
			const uint32_t num_dots = total_width * total_height;
			uint8_t        map_data[256 * 256 * 2];
			pixels.resize(num_dots);
			vera_video_get_expanded_vram_with_wraparound_handling(tile_base, bpp, tile_data, tile_width * tile_height * 1024);
			vera_video_space_read_range(map_data, map_base, map_width * map_height * 2);

			int tidx = 0;
			if (bpp == 1) {
				// 1bpp tile mode is ""special""
				for (int mi = 0; mi < map_height; mi++) {
					uint32_t dst = mi * tile_height * total_width;
					for (int mj = 0; mj < map_width; mj++) {
						const uint16_t tinfo = map_data[tidx] + (map_data[tidx + 1] << 8);
						const uint16_t tnum  = tinfo & 0xFF;
						const uint32_t fg_px = palette[t256c ? (tinfo >> 8) : ((tinfo >> 8) & 0x0F)];
						const uint32_t bg_px = palette[t256c ? 0 : (tinfo >> 12)];
						uint32_t       src   = tnum * tile_width * tile_height;
						for (int ti = 0; ti < tile_height; ti++) {
							uint32_t dst2 = dst + ti * total_width;
							for (int tj = 0; tj < tile_width; tj++) {
								pixels[dst2++] = tile_data[src++] ? fg_px : bg_px;
							}
						}
						dst += tile_width;
						tidx += 2;
					}
				}
			} else {
				for (int mi = 0; mi < map_height; mi++) {
					uint32_t dst = mi * tile_height * total_width;
					for (int mj = 0; mj < map_width; mj++) {
						const uint16_t tinfo    = map_data[tidx] + (map_data[tidx + 1] << 8);
						const bool     hflip    = tinfo & (1 << 10);
						const bool     vflip    = tinfo & (1 << 11);
						const uint16_t tnum     = tinfo & 0x3FF;
						const uint8_t  pal      = (tinfo >> 12) * 16;
						const int      src2_add = hflip ? -1 : 1;
						uint32_t       src      = tnum * tile_width * tile_height;
						if (hflip)
							src += tile_width - 1;
						for (int ti = 0; ti < tile_height; ti++) {
							uint32_t src2 = vflip ? (src + (tile_height - ti - 1) * tile_width) : (src + ti * tile_width);
							uint32_t dst2 = dst + ti * total_width;
							for (int tj = 0; tj < tile_width; tj++) {
								uint8_t tdat = tile_data[src2];
								src2 += src2_add;
								if (tdat > 0 && tdat < 16) {
									tdat += pal;
									if (t256c) {
										tdat |= 0x80;
									}
								}
									
								pixels[dst2++] = palette[tdat];
							}
						}
						dst += tile_width;
						tidx += 2;
					}
				}
			}
		}
		if (pixels.data() != nullptr) {
			tiles_preview.load_memory(pixels.data(), total_width, total_height, total_width, total_height);
		}
	}

	void set_params(const vera_video_layer_properties &props, int palette_offset_)
	{
		// Max height for bitmap mode is currently 480.
		// Although the theoretical maximum is 1016 (HSTOP = 255, HSCALE = 255),
		// there's currently no real hardware information about going above 480 lines
		bitmap_mode    = props.bitmap_mode;
		t256c          = props.text_mode_256c;
		bpp            = props.bits_per_pixel;
		tile_base      = props.tile_base;
		tile_width     = props.tilew;
		tile_height    = props.tileh;
		map_base       = props.map_base;
		map_width      = 1 << props.mapw_log2;
		map_height     = 1 << props.maph_log2;
		total_width    = bitmap_mode ? tile_width : tile_width * map_width;
		total_height   = bitmap_mode ? 480 : tile_height * map_height;
		scroll_x       = total_width > 0 ? (props.hscroll % total_width) : 0;
		scroll_y       = total_height > 0 ? (props.vscroll % total_height) : 0;
		palette_offset = palette_offset_;

		if (!bitmap_mode && cur_tile >= map_width * map_height)
			cur_tile = 0;
	}

	uint16_t get_selected_tile()
	{
		return cur_tile;
	}

private:
	icon_set tiles_preview;

	bool     bitmap_mode;
	bool     t256c;
	int      bpp;
	int      palette_offset;
	uint32_t tile_base;
	uint16_t tile_width;
	uint16_t tile_height;
	uint32_t map_base;
	uint16_t map_width;
	uint16_t map_height;
	uint16_t total_width;
	uint16_t total_height;
	uint16_t scroll_x;
	uint16_t scroll_y;

	float screen_width;
	float screen_height;

	uint16_t cur_tile = 0;
	bool     show_grid;
	bool     show_scroll;
};

static void draw_debugger_vera_layer()
{
	static int             layer_id;
	static uint64_t        layer_sig;
	static tmap_visualizer viz;

	static const ImU8  incr_one8  = 1;
	static const ImU8  incr_hex8  = 16;
	static const ImU16 incr_one16 = 1;
	static const ImU16 incr_ten16 = 10;
	static const ImU16 incr_hex16 = 16;
	static const ImU32 incr_map   = 1 << 9;
	static const ImU32 fast_map   = incr_map << 4;
	static const ImU32 incr_tile  = 1 << 11;
	static const ImU32 fast_tile  = incr_tile << 4;

	static bool reload = true;

	ImGui::Text("Layer");
	ImGui::SameLine();
	ImGui::RadioButton("0", &layer_id, 0);
	ImGui::SameLine();
	ImGui::RadioButton("1", &layer_id, 1);

	uint8_t layer_data[8];
	memcpy(layer_data, vera_video_get_layer_data(layer_id), 7);
	layer_data[7] = 0;

	vera_video_layer_properties layer_props;
	memcpy(&layer_props, vera_video_get_layer_properties(layer_id), sizeof(vera_video_layer_properties));

	if (layer_sig != *reinterpret_cast<const uint64_t *>(layer_data)) {
		layer_sig = *reinterpret_cast<const uint64_t *>(layer_data);
	}

	// vera_video_layer_properties doesn't provide bitmap color index right now
	viz.set_params(layer_props, (layer_data[4] & 0x0F) * 16);
	viz.draw_preview();

	ImGui::SameLine();

	ImGui::BeginGroup();
	{
		ImGui::TextDisabled("Raw Bytes");

		for (int i = 0; i < 7; ++i) {
			if (i) {
				ImGui::SameLine();
			}
			if (ImGui::InputHex(i, layer_data[i])) {
				vera_video_write(0x0D + 7 * layer_id + i, layer_data[i]);
			}
		}

		ImGui::PushItemWidth(128.0f);
		ImGui::NewLine();
		ImGui::TextDisabled("Layer Properties");

		auto get_byte = [&layer_props](int b) -> uint8_t {
			switch (b) {
				case 0:
					return ((layer_props.maph_log2 - 5) << 6) | ((layer_props.mapw_log2 - 5) << 4) | (layer_props.text_mode_256c ? 0x8 : 0) | (layer_props.bitmap_mode ? 0x4 : 0) | layer_props.color_depth;
				case 1:
					return layer_props.map_base >> 9;
				case 2:
					return ((layer_props.tile_base >> 11) << 2) | (layer_props.tileh_log2 == 4 ? 0x2 : 0) | (layer_props.tilew_log2 == 4 ? 0x1 : 0);
				case 3:
					return layer_props.hscroll & 0xff;
				case 4:
					return layer_props.hscroll >> 8;
				case 5:
					return layer_props.vscroll & 0xff;
				case 6:
					return layer_props.vscroll >> 8;
				default:
					return 0;
			}
		};

		static const char *depths_txt[]{ "1", "2", "4", "8" };
		int                depth = layer_props.color_depth;
		if (ImGui::Combo("Color Depth", &depth, depths_txt, 4)) {
			layer_props.color_depth = depth;
			vera_video_write(0x0D + 7 * layer_id, get_byte(0));
		}
		if (ImGui::Checkbox("Bitmap Layer", &layer_props.bitmap_mode)) {
			vera_video_write(0x0D + 7 * layer_id, get_byte(0));
		}

		if (layer_props.bitmap_mode) {
			if (ImGui::InputScalar("Tile Base", ImGuiDataType_U32, &layer_props.tile_base, &incr_tile, &fast_tile, "%05X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_write(0x0F + 7 * layer_id, get_byte(2));
			}
			static const char *bm_widths_txt[]{ "320", "640" };
			int                bm_width = layer_props.tilew == 640;
			if (ImGui::Combo("Bitmap Width", &bm_width, bm_widths_txt, 2)) {
				vera_video_write(0x0F + 7 * layer_id, layer_data[2] & ~0x01 | bm_width);
			}
			uint8_t palofs = (layer_data[4] & 0x0F) << 4;
			if (ImGui::InputScalar("Palette Offset", ImGuiDataType_U8, &palofs, &incr_hex8, &incr_hex8, "%d")) {
				vera_video_write(0x11 + 7 * layer_id, layer_data[4] & ~0x0F | (palofs >> 4));
			}
		} else {
			if (ImGui::Checkbox("256-color text", &layer_props.text_mode_256c)) {
				vera_video_write(0x0D + 7 * layer_id, get_byte(0));
			}
			static const char *map_sizes_txt[]{ "32", "64", "128", "256" };
			int                mapw_log2 = layer_props.mapw_log2 - 5;
			if (ImGui::Combo("Map Width", &mapw_log2, map_sizes_txt, 4)) {
				layer_props.mapw_log2 = mapw_log2 + 5;
				vera_video_write(0x0D + 7 * layer_id, get_byte(0));
			}
			int maph_log2 = layer_props.maph_log2 - 5;
			if (ImGui::Combo("Map Height", &maph_log2, map_sizes_txt, 4)) {
				if (maph_log2 > 3)
					maph_log2 = 3;
				layer_props.maph_log2 = maph_log2 + 5;
				vera_video_write(0x0D + 7 * layer_id, get_byte(0));
			}
			if (ImGui::InputScalar("Map Base", ImGuiDataType_U32, &layer_props.map_base, &incr_map, &fast_map, "%05X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_write(0x0E + 7 * layer_id, get_byte(1));
			}
			bool tile16h = layer_props.tileh_log2 > 3;
			if (ImGui::Checkbox("16-pixel tile height", &tile16h)) {
				layer_props.tileh_log2 = tile16h ? 4 : 3;
				vera_video_write(0x0F + 7 * layer_id, get_byte(2));
			}
			bool tile16w = layer_props.tilew_log2 > 3;
			if (ImGui::Checkbox("16-pixel tile width", &tile16w)) {
				layer_props.tilew_log2 = tile16w ? 4 : 3;
				vera_video_write(0x0F + 7 * layer_id, get_byte(2));
			}
			if (ImGui::InputScalar("Tile Base", ImGuiDataType_U32, &layer_props.tile_base, &incr_tile, &fast_tile, "%05X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_write(0x0F + 7 * layer_id, get_byte(2));
			}
			if (ImGui::InputScalar("H-Scroll", ImGuiDataType_U16, &layer_props.hscroll, &incr_one16, &incr_ten16, "%03X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_write(0x10 + 7 * layer_id, get_byte(3));
				vera_video_write(0x11 + 7 * layer_id, get_byte(4));
			}
			if (ImGui::InputScalar("V-Scroll", ImGuiDataType_U16, &layer_props.vscroll, &incr_one16, &incr_ten16, "%03X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_write(0x12 + 7 * layer_id, get_byte(5));
				vera_video_write(0x13 + 7 * layer_id, get_byte(6));
			}

			ImGui::NewLine();
			ImGui::TextDisabled("Tile Properties");

			const uint16_t tile_idx  = viz.get_selected_tile();
			const uint32_t tile_addr = (layer_props.map_base + tile_idx * 2) & 0x1FFFF;
			uint16_t       tile_data = vera_video_space_read(tile_addr) + (vera_video_space_read(tile_addr + 1) << 8);
			ImGui::LabelText("Position", "%d, %d", tile_idx % (1 << layer_props.mapw_log2), tile_idx / (1 << layer_props.maph_log2));
			ImGui::LabelText("Address", "%05X", tile_addr);
			if (ImGui::InputScalar("Raw Value", ImGuiDataType_U16, &tile_data, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_space_write(tile_addr, tile_data & 255);
				vera_video_space_write(tile_addr + 1, tile_data >> 8);
			}
			const uint16_t tile_num_mask = layer_props.color_depth == 0 ? 0xFF : 0x3FF;
			uint16_t       tile_num      = tile_data & tile_num_mask;
			if (ImGui::InputScalar("Tile Number", ImGuiDataType_U16, &tile_num, &incr_one16, &incr_hex16, "%d")) {
				if (tile_num > tile_num_mask)
					tile_num = tile_num_mask;
				const uint16_t val = tile_data & ~tile_num_mask | tile_num;
				vera_video_space_write(tile_addr, val & 255);
				vera_video_space_write(tile_addr + 1, val >> 8);
			}
			ImGui::LabelText("Data Address", "%05X", (layer_props.tile_base + tile_num * layer_props.tilew * layer_props.tileh * layer_props.bits_per_pixel / 8) & 0x1FFFF);
			// bpp-dependent properties, thanks VERA
			const uint8_t tile_data_hi = tile_data >> 8;
			if (layer_props.color_depth == 0) {
				if (layer_props.text_mode_256c) {
					uint8_t fg = tile_data_hi;
					if (ImGui::InputScalar("Color", ImGuiDataType_U8, &fg, &incr_one8, &incr_hex8, "%d")) {
						vera_video_space_write(tile_addr + 1, fg);
					}
				} else {
					uint8_t fg = tile_data_hi & 0x0F;
					uint8_t bg = tile_data_hi >> 4;
					if (ImGui::InputScalar("FG Color", ImGuiDataType_U8, &fg, &incr_one8, &incr_hex8, "%d")) {
						if (fg > 15)
							fg = 15;
						vera_video_space_write(tile_addr + 1, tile_data_hi & ~0x0F | fg);
					}
					if (ImGui::InputScalar("BG Color", ImGuiDataType_U8, &bg, &incr_one8, &incr_hex8, "%d")) {
						if (bg > 15)
							bg = 15;
						vera_video_space_write(tile_addr + 1, tile_data_hi & ~0xF0 | (bg << 4));
					}
				}
			} else {
				uint8_t pal   = tile_data_hi >> 4;
				bool    hflip = tile_data_hi & (1 << 2);
				bool    vflip = tile_data_hi & (1 << 3);
				if (ImGui::InputScalar("Palette", ImGuiDataType_U8, &pal, &incr_one8, &incr_hex8, "%d")) {
					if (pal > 15)
						pal = 15;
					vera_video_space_write(tile_addr + 1, tile_data_hi & ~0xF0 | (pal << 4));
				}
				if (ImGui::Checkbox("Horizontal Flip", &hflip)) {
					vera_video_space_write(tile_addr + 1, bit_set_or_res(tile_data_hi, (uint8_t)(1 << 2), hflip));
				}
				if (ImGui::Checkbox("Vertical Flip", &vflip)) {
					vera_video_space_write(tile_addr + 1, bit_set_or_res(tile_data_hi, (uint8_t)(1 << 3), vflip));
				}
			}
		}

		ImGui::PopItemWidth();
	}
	ImGui::EndGroup();
}

static void draw_debugger_vram_visualizer()
{
	static vram_visualizer viz;
	viz.draw_preview();
	ImGui::SameLine();
	viz.draw_preview_widgets();
}

static void draw_breakpoints()
{
	ImGui::BeginGroup();
	{
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
		if (ImGui::TreeNodeEx("Breakpoints", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginTable("breakpoints", 7)) {
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("R", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 64);
				ImGui::TableSetupColumn("Bank", ImGuiTableColumnFlags_WidthFixed, 48);
				ImGui::TableSetupColumn("Symbol");
				ImGui::TableHeadersRow();

				const auto &breakpoints = debugger_get_breakpoints();
				for (auto &[address, bank] : breakpoints) {
					ImGui::PushID(address);
					ImGui::PushID(bank);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (ImGui::TileButton(ICON_REMOVE)) {
						debugger_remove_breakpoint(address, bank);
						ImGui::PopID();
						ImGui::PopID();
						break;
					}

					int c = 0;

					ImGui::TableNextColumn();
					ImGui::PushID(c++);
					if (debugger_breakpoint_is_active(address, bank, DEBUG6502_READ)) {
						if (ImGui::TileButton(ICON_CHECKED)) {
							debugger_deactivate_breakpoint(address, bank, DEBUG6502_READ);
						}
					} else {
						if (ImGui::TileButton(ICON_UNCHECKED)) {
							debugger_activate_breakpoint(address, bank, DEBUG6502_READ);
						}
					}
					ImGui::PopID();

					ImGui::TableNextColumn();
					ImGui::PushID(c++);
					if (debugger_breakpoint_is_active(address, bank, DEBUG6502_WRITE)) {
						if (ImGui::TileButton(ICON_CHECKED)) {
							debugger_deactivate_breakpoint(address, bank, DEBUG6502_WRITE);
						}
					} else {
						if (ImGui::TileButton(ICON_UNCHECKED)) {
							debugger_activate_breakpoint(address, bank, DEBUG6502_WRITE);
						}
					}
					ImGui::PopID();

					ImGui::TableNextColumn();
					ImGui::PushID(c++);
					if (debugger_breakpoint_is_active(address, bank, DEBUG6502_EXEC)) {
						if (ImGui::TileButton(ICON_CHECKED)) {
							debugger_deactivate_breakpoint(address, bank, DEBUG6502_EXEC);
						}
					} else {
						if (ImGui::TileButton(ICON_UNCHECKED)) {
							debugger_activate_breakpoint(address, bank, DEBUG6502_EXEC);
						}
					}
					ImGui::PopID();

					ImGui::TableNextColumn();
					char addr_text[5];
					sprintf(addr_text, "%04X", address);
					if (ImGui::Selectable(addr_text, false, ImGuiSelectableFlags_AllowDoubleClick)) {
						disasm.set_dump_start(address);
						if (address >= 0xc000) {
							disasm.set_rom_bank(bank);
						} else if (address >= 0xa000) {
							disasm.set_ram_bank(bank);
						}
					}

					ImGui::TableNextColumn();
					if (address < 0xa000) {
						ImGui::Text("--");
					} else {
						ImGui::Text("%s %02X", address < 0xc000 ? "RAM" : "ROM", bank);
					}

					ImGui::TableNextColumn();
					for (auto &sym : symbols_find(address)) {
						if (ImGui::Selectable(sym.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
							disasm.set_dump_start(address);
							if (address >= 0xc000) {
								disasm.set_rom_bank(bank);
							} else if (address >= 0xa000) {
								disasm.set_ram_bank(bank);
							}
						}
					}

					ImGui::PopID();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			static uint16_t new_address = 0;
			static uint8_t  new_bank    = 0;
			ImGui::InputHexLabel("New Address", new_address);
			ImGui::SameLine();
			ImGui::InputHexLabel("Bank", new_bank);
			ImGui::SameLine();
			if (ImGui::Button("Add")) {
				debugger_add_breakpoint(new_address, new_bank);
			}

			ImGui::Dummy(ImVec2(0, 5));
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();
	}
	ImGui::EndGroup();
}

static void draw_watch_list()
{
	ImGui::BeginGroup();
	{
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
		if (ImGui::TreeNodeEx("Watch List", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			static bool show_hex = true;
			ImGui::Checkbox("Show Hex Values", &show_hex);

			if (ImGui::BeginTable("watch list", 6)) {
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 64);
				ImGui::TableSetupColumn("Bank", ImGuiTableColumnFlags_WidthFixed, 48);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 64);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 88);
				ImGui::TableSetupColumn("Symbol");
				ImGui::TableHeadersRow();

				const auto &watchlist = debugger_get_watchlist();
				for (auto &[address, bank, size] : watchlist) {
					ImGui::PushID(address);
					ImGui::PushID(bank);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (ImGui::TileButton(ICON_REMOVE)) {
						debugger_remove_watch(address, bank, size);
						ImGui::PopID();
						ImGui::PopID();
						break;
					}

					ImGui::TableNextColumn();
					char addr_text[5];
					sprintf(addr_text, "%04X", address);
					if (ImGui::Selectable(addr_text, false, ImGuiSelectableFlags_AllowDoubleClick)) {
						disasm.set_dump_start(address);
						if (address >= 0xc000) {
							disasm.set_rom_bank(bank);
						} else if (address >= 0xa000) {
							disasm.set_ram_bank(bank);
						}
					}

					ImGui::TableNextColumn();
					if (address < 0xa000) {
						ImGui::Text("--");
					} else {
						ImGui::Text("%s %02X", address < 0xc000 ? "RAM" : "ROM", bank);
					}

					ImGui::TableNextColumn();
					uint8_t new_size = size;
					if (ImGui::InputCombo(0, Debugger_size_types, new_size)) {
						const uint16_t new_address = address;
						const uint8_t  new_bank    = bank;
						debugger_remove_watch(address, bank, size);
						debugger_add_watch(new_address, new_bank, new_size);
						ImGui::PopID();
						ImGui::PopID();
						break;
					}

					ImGui::TableNextColumn();
					const uint8_t type_size = (size & 3) + 1;
					const bool    is_signed = (size & 4);
					union {
						uint32_t u;
						uint8_t  b[4];
					} value;

					value.u = 0;
					{
						uint8_t i = 0;
						for (; i < type_size; ++i) {
							value.b[i] = debug_read6502(address + i, bank);
						}
						if (is_signed && (value.b[i - 1] & 0x80)) {
							for (; i < 4; ++i) {
								value.b[i] = 0xff;
							}
						}
					}

					bool edited = false;
					if (show_hex) {
						switch (type_size) {
							case 1:
								edited = ImGui::InputHex<uint32_t, 8>(1, value.u);
								break;
							case 2:
								edited = ImGui::InputHex<uint32_t, 16>(1, value.u);
								break;
							case 3:
								edited = ImGui::InputHex<uint32_t, 24>(1, value.u);
								break;
							case 4:
								edited = ImGui::InputHex<uint32_t, 32>(1, value.u);
								break;
						}
					} else if (is_signed) {
						ImGui::PushItemWidth(88.0f);
						edited = ImGui::InputScalar("", ImGuiDataType_S32, &value.u, 0, 0, "%d");
						ImGui::PopItemWidth();
					} else {
						ImGui::PushItemWidth(88.0f);
						edited = ImGui::InputScalar("", ImGuiDataType_U32, &value.u, 0, 0, "%u");
						ImGui::PopItemWidth();
					}

					if (edited) {
						for (uint8_t i = 0; i < type_size; ++i) {
							debug_write6502(address + i, bank, value.b[i]);
						}
					}

					ImGui::TableNextColumn();
					for (auto &sym : symbols_find(address)) {
						if (ImGui::Selectable(sym.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
							disasm.set_dump_start(address);
							if (address >= 0xc000) {
								disasm.set_rom_bank(bank);
							} else if (address >= 0xa000) {
								disasm.set_ram_bank(bank);
							}
						}
					}

					ImGui::PopID();
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			static uint16_t new_address = 0;
			static uint8_t  new_bank    = 0;
			static uint8_t  size_type   = 0;
			ImGui::InputHexLabel("New Address", new_address);
			ImGui::SameLine();
			ImGui::InputHexLabel("Bank", new_bank);
			ImGui::SameLine();
			ImGui::InputCombo("Type", Debugger_size_types, size_type);

			//{
			//	ImGui::Text("Type");
			//	ImGui::SameLine();

			//	ImGui::PushID("size type");
			//	ImGui::PushItemWidth(hex_widths[7]);
			//	if (ImGui::BeginCombo("", Debugger_size_types[size_type])) {
			//		for (uint8_t i = 0; i < Num_debugger_size_types; ++i) {
			//			if (ImGui::Selectable(Debugger_size_types[i], (size_type == i))) {
			//				size_type = i;
			//			}
			//		}
			//		ImGui::EndCombo();
			//	}
			//	ImGui::PopItemWidth();
			//	ImGui::PopID();
			//}

			if (ImGui::Button("Add")) {
				debugger_add_watch(new_address, new_bank, size_type);
			}

			ImGui::Dummy(ImVec2(0, 5));
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();
	}
	ImGui::EndGroup();
}

static void draw_symbols_list()
{
	ImGui::BeginGroup();
	{
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
		if (ImGui::TreeNodeEx("Loaded Symbols", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			static char symbol_filter[64] = "";
			ImGui::InputText("Filter", symbol_filter, 64);

			static bool     selected      = false;
			static uint16_t selected_addr = 0;
			static uint8_t  selected_bank = 0;
			if (ImGui::BeginListBox("Filtered Symbols")) {
				int  id                   = 0;
				bool any_selected_visible = false;

				auto search_filter_contains = [&](const char *value) -> bool {
					char filter[64];
					strcpy(filter, symbol_filter);
					char *token    = strtok(filter, " ");
					bool  included = true;
					while (token != nullptr) {
						if (strstr(value, token) == nullptr) {
							included = false;
							break;
						}
						token = strtok(nullptr, " ");
					}
					return included;
				};

				symbols_for_each([&](uint16_t address, symbol_bank_type bank, const std::string &name) {
					if (search_filter_contains(name.c_str())) {
						ImGui::PushID(id++);
						bool is_selected = selected && (selected_addr == address) && (selected_bank == bank);
						char display_name[128];
						sprintf(display_name, "%04x %s", address, name.c_str());
						if (ImGui::Selectable(display_name, is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
							selected      = true;
							selected_addr = address;
							selected_bank = bank;
							is_selected   = true;

							if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
								disasm.set_dump_start(address);
								disasm.set_rom_bank(bank);
							}
						}
						any_selected_visible = any_selected_visible || is_selected;
						ImGui::PopID();
					}
				});
				selected = any_selected_visible;
				ImGui::EndListBox();
			}

			if (ImGui::Button("Add Breakpoint at Symbol") && selected) {
				debugger_add_breakpoint(selected_addr, selected_bank);
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Watch at Symbol") && selected) {
				debugger_add_watch(selected_addr, selected_bank, 1);
			}

			ImGui::Dummy(ImVec2(0, 5));
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();
	}
	ImGui::EndGroup();
}

static void draw_symbols_files()
{
	ImGui::BeginGroup();
	{
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
		if (ImGui::TreeNodeEx("Loaded Symbol Files", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginTable("symbols", 3)) {
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 16);
				ImGui::TableSetupColumn("Path");
				ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
				ImGui::TableSetColumnIndex(1);

				const auto &files = symbols_get_loaded_files();

				if (symbols_file_all_are_visible()) {
					if (ImGui::TileButton(ICON_CHECKED)) {
						for (auto &file : files) {
							symbols_hide_file(file);
						}
					}
				} else if (symbols_file_any_is_visible()) {
					if (ImGui::TileButton(ICON_CHECK_UNCERTAIN)) {
						for (auto &file : files) {
							symbols_hide_file(file);
						}
					}
				} else {
					if (ImGui::TileButton(ICON_UNCHECKED)) {
						for (auto &file : files) {
							symbols_show_file(file);
						}
					}
				}

				for (auto file : files) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::PushID(file.c_str());
					if (ImGui::TileButton(ICON_REMOVE)) {
						symbols_unload_file(file);
						ImGui::PopID();
						break;
					}

					ImGui::TableNextColumn();
					if (symbols_file_is_visible(file)) {
						if (ImGui::TileButton(ICON_CHECKED)) {
							symbols_hide_file(file);
						}
					} else {
						if (ImGui::TileButton(ICON_UNCHECKED)) {
							symbols_show_file(file);
						}
					}
					ImGui::PopID();

					ImGui::TableNextColumn();
					ImGui::Text("%s", file.c_str());
				}
				ImGui::EndTable();
			}

			static uint8_t ram_bank = 0;
			if (ImGui::Button("Load Symbols")) {
				char *open_path = nullptr;
				if (NFD_OpenDialog("sym", nullptr, &open_path) == NFD_OKAY && open_path != nullptr) {
					symbols_load_file(open_path, ram_bank);
				}
			}

			ImGui::InputHexLabel("Bank", ram_bank);

			ImGui::Dummy(ImVec2(0, 5));
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();
	}
	ImGui::EndGroup();
}

static void draw_debugger_controls()
{
	bool paused  = debugger_is_paused();
	bool shifted = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

	static bool stop_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STOP_DISABLED : ICON_STOP, !paused, &stop_hovered) || (shifted && ImGui::IsKeyPressed(ImGuiKey_F5))) {
		debugger_pause_execution();
		disasm.follow_pc();
	}
	if (!paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Pause execution (Shift+F5)");
	}

	ImGui::SameLine();

	static bool run_hovered = false;
	if (ImGui::TileButton(paused ? ICON_RUN : ICON_RUN_DISABLED, paused, &run_hovered) || (!shifted && ImGui::IsKeyPressed(ImGuiKey_F5))) {
		debugger_continue_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Run (F5)");
	}
	ImGui::SameLine();

	static bool step_over_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STEP_OVER : ICON_STEP_OVER_DISABLED, paused, &step_over_hovered) || (!shifted && ImGui::IsKeyPressed(ImGuiKey_F10))) {
		debugger_step_over_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step Over (F10)");
	}
	ImGui::SameLine();

	static bool step_into_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STEP_INTO : ICON_STEP_INTO_DISABLED, paused, &step_into_hovered) || (!shifted && ImGui::IsKeyPressed(ImGuiKey_F11))) {
		debugger_step_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step Into (F11)");
	}
	ImGui::SameLine();

	static bool step_out_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STEP_OUT : ICON_STEP_OUT_DISABLED, paused, &step_out_hovered) || (shifted && ImGui::IsKeyPressed(ImGuiKey_F11))) {
		debugger_step_out_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step Out (Shift+F11)");
	}
	ImGui::SameLine();

	static bool set_breakpoint_hovered = false;
	const bool  breakpoint_exists      = debugger_has_breakpoint(state6502.pc, memory_get_current_bank(state6502.pc));
	const bool  breakpoint_active      = debugger_breakpoint_is_active(state6502.pc, memory_get_current_bank(state6502.pc));
	if (ImGui::TileButton(paused ? ICON_ADD_BREAKPOINT : ICON_UNCHECKED_DISABLED, paused, &set_breakpoint_hovered) || (!shifted && ImGui::IsKeyPressed(ImGuiKey_F9))) {
		if (breakpoint_active) {
			debugger_deactivate_breakpoint(state6502.pc, memory_get_current_bank(state6502.pc));
		} else if (breakpoint_exists) {
			debugger_remove_breakpoint(state6502.pc, memory_get_current_bank(state6502.pc));
		} else {
			debugger_add_breakpoint(state6502.pc, memory_get_current_bank(state6502.pc));
		}
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggle Breakpoint (F9)");
	}
	ImGui::SameLine();

	char cycles_raw[32];
	int  digits = sprintf(cycles_raw, "%" SDL_PRIu64, debugger_step_clocks());

	char  cycles_formatted[32];
	char *r = cycles_raw;
	char *f = cycles_formatted;
	while (*r != '\0') {
		*f = *r;
		++r;
		++f;
		--digits;
		if ((digits > 0) && (digits % 3 == 0)) {
			*f = ',';
			++f;
		}
	}
	*f = '\0';

	if (paused) {
		ImGui::Text("%s cycles%s", cycles_formatted, debugger_step_interrupted() ? " (Interrupted)" : "");
	} else {
		ImGui::TextDisabled("%s cycles%s", cycles_formatted, debugger_step_interrupted() ? " (Interrupted)" : "");
	}
}

static void draw_menu_bar()
{
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open TXT file")) {
				char *open_path = nullptr;
				if (NFD_OpenDialog("txt", nullptr, &open_path) == NFD_OKAY && open_path != nullptr) {
					keyboard_add_file(open_path);
				}
			}

			if (ImGui::MenuItem("Options")) {
				Show_options = true;
			}

			if (ImGui::MenuItem("Exit")) {
				SDL_Event evt;
				evt.type = SDL_QUIT;
				SDL_PushEvent(&evt);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Machine")) {
			if (ImGui::MenuItem("Reset", Options.no_keybinds ? nullptr : "Ctrl-R")) {
				machine_reset();
			}
			if (ImGui::MenuItem("NMI")) {
				nmi6502();
				debugger_interrupt();
			}
			if (ImGui::MenuItem("Save Dump", Options.no_keybinds ? nullptr : "Ctrl-S")) {
				machine_dump("user menu request");
			}
			if (ImGui::BeginMenu("Controller Ports")) {
				joystick_for_each_slot([](int slot, int instance_id, SDL_GameController *controller) {
					const char *name = nullptr;
					if (controller != nullptr) {
						name = SDL_GameControllerName(controller);
					}
					if (name == nullptr) {
						name = "(No Controller)";
					}

					char label[256];
					snprintf(label, 256, "%d: %s", slot, name);
					label[255] = '\0';

					if (ImGui::BeginMenu(label)) {
						if (ImGui::RadioButton("(No Controller)", instance_id == -1)) {
							if (instance_id >= 0) {
								joystick_slot_remap(slot, -1);
							}
						}

						joystick_for_each([slot](int instance_id, SDL_GameController *controller, int current_slot) {
							const char *name = nullptr;
							if (controller != nullptr) {
								name = SDL_GameControllerName(controller);
							}
							if (name == nullptr) {
								name = "(No Controller)";
							}

							char label[256];
							snprintf(label, 256, "%s (%d)", name, instance_id);
							label[255] = '\0';

							if (ImGui::RadioButton(label, slot == current_slot)) {
								if (slot != current_slot) {
									joystick_slot_remap(slot, instance_id);
								}
							}
						});
						ImGui::EndMenu();
					}
				});
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("SD Card")) {
				if (ImGui::MenuItem("Open")) {
					char *open_path = nullptr;
					if (NFD_OpenDialog("bin;img;sdcard", nullptr, &open_path) == NFD_OKAY && open_path != nullptr) {
						sdcard_set_file(open_path);
					}
				}

				bool sdcard_attached = sdcard_is_attached();
				if (ImGui::Checkbox("Attach card", &sdcard_attached)) {
					if (sdcard_attached) {
						sdcard_attach();
					} else {
						sdcard_detach();
					}
				}
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Change CWD")) {
				char *open_path = nullptr;
				if (NFD_PickFolder("", &open_path) == NFD_OKAY && open_path != nullptr) {
					Options.fsroot_path = open_path;
				}
			}

			ImGui::Separator();

			ImGui::SetNextItemWidth(69.0f);
			if (ImGui::InputInt("Set Warp Factor", &Options.warp_factor)) {
				Options.warp_factor = std::clamp(Options.warp_factor, 0, 16);
				if (Options.warp_factor == 0) {
					vera_video_set_cheat_mask(0);
				} else {
					vera_video_set_cheat_mask((1 << (Options.warp_factor - 1)) - 1);
				}
			}
			bool audio_enabled = !Options.no_sound;
			if (ImGui::Checkbox("Enable Audio", &audio_enabled)) {
				if (audio_enabled) {
					audio_init(Options.audio_dev_name.size() > 0 ? Options.audio_dev_name.c_str() : nullptr, Options.audio_buffers);
				} else {
					audio_close();
				}
				Options.no_sound = !audio_enabled;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows")) {
			ImGui::Checkbox("Display", &Show_display);
			if (ImGui::BeginMenu("CPU Debugging")) {
				ImGui::Checkbox("Memory Dump 1", &Show_memory_dump_1);
				ImGui::Checkbox("Memory Dump 2", &Show_memory_dump_2);
				ImGui::Checkbox("CPU Monitor (Ctrl-Alt-C)", &Show_cpu_monitor);
				ImGui::Checkbox("Disassembler (Ctrl-Alt-D)", &Show_disassembler);
				if (ImGui::Checkbox("CPU Visualizer", &Show_cpu_visualizer)) {
					cpu_visualization_enable(Show_cpu_visualizer);
				}
				ImGui::Checkbox("Breakpoints (Ctrl-Alt-B)", &Show_breakpoints);
				ImGui::Checkbox("Watch List (Ctrl-Alt-W)", &Show_watch_list);
				ImGui::Checkbox("Symbols List (Ctrl-Alt-S)", &Show_symbols_list);
				ImGui::Checkbox("Symbols Files", &Show_symbols_files);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("VERA Debugging")) {
				ImGui::Checkbox("Tile Visualizer", &Show_VRAM_visualizer);
				ImGui::Checkbox("VERA Monitor", &Show_VERA_monitor);
				ImGui::Checkbox("Palette", &Show_VERA_palette);
				ImGui::Checkbox("Layer Settings", &Show_VERA_layers);
				ImGui::Checkbox("Sprite Settings", &Show_VERA_sprites);
				ImGui::EndMenu();
			}
			ImGui::Checkbox("PSG Monitor", &Show_VERA_PSG_monitor);
			ImGui::Checkbox("YM2151 Monitor", &Show_YM2151_monitor);
			ImGui::Separator();

			if (ImGui::BeginMenu("Safety Frame")) {
				static constexpr const char   *modes[]   = { "Disabled", "VGA", "NTSC", "RGB interlaced, composite, via VGA connector" };
				static constexpr const uint8_t num_modes = sizeof(modes) / sizeof(modes[0]);

				for (uint8_t i = 0; i < num_modes; ++i) {
					bool safety_frame = vera_video_safety_frame_is_enabled(i);
					if (ImGui::Checkbox(modes[i], &safety_frame)) {
						vera_video_enable_safety_frame(i, safety_frame);
					}
				}
				ImGui::EndMenu();
			}

			ImGui::Checkbox("MIDI Control", &Show_midi_overlay);

#if defined(_DEBUG)
			if (ImGui::Checkbox("Show ImGui Demo", &Show_imgui_demo)) {
				// Nothing to do.
			}
#endif
			ImGui::EndMenu();
		}

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 116.0f);
		ImGui::Tile(ICON_POWER_LED_OFF);
		if (power_led > 0) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 116.0f);
			ImGui::Tile(ICON_POWER_LED_ON, (float)power_led / 255.0f);
		}
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 96.0f);
		ImGui::Tile(ICON_ACTIVITY_LED_OFF);
		if (activity_led > 0) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 96.0f);
			ImGui::Tile(ICON_ACTIVITY_LED_ON, (float)activity_led / 255.0f);
		}

		enum class timing_type {
			emulated,
			gpu_fps
		};

		static timing_type Display_timing = timing_type::emulated;

		switch (Display_timing) {
			case timing_type::emulated:
				if (Timing_perf >= 1000) {
					ImGui::Text("Speed: %dX", Timing_perf / 100);
				} else {
					ImGui::Text("Speed: %d%%", Timing_perf);
				}
				break;
			case timing_type::gpu_fps:
				ImGui::Text("FPS: %2.2f", display_get_fps());
				break;
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			switch (Display_timing) {
				case timing_type::emulated:
					Display_timing = timing_type::gpu_fps;
					break;
				case timing_type::gpu_fps:
					Display_timing = timing_type::emulated;
					break;
			}
		}
		ImGui::EndMainMenuBar();
	}
}

static ImVec2 get_integer_scale_window_size(ImVec2 avail) {
	float width            = 480.f * display_get_aspect_ratio();
	float title_bar_height = ImGui::GetFrameHeight();
	float scale;
	if (avail.x < avail.y) {
		scale = avail.x / width;
	} else {
		scale = avail.y / 480.f;
	}
	if (scale < 1) {
		scale = floorf(1.f / std::max(scale, 0.125f));
		return ImVec2(width / scale, 480.f / scale + title_bar_height);
	} else {
		scale = floorf(scale);
		return ImVec2(width * scale, 480.f * scale + title_bar_height);
	}
}

void overlay_draw()
{
	ImGuiIO & io = ImGui::GetIO();
	if (mouse_captured) {
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
	} else {
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}

	draw_menu_bar();
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGuiID dock_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	if (Show_options) {
		if (ImGui::Begin("Options", &Show_options)) {
			draw_options_menu();
		}
		ImGui::End();
	}

	if (Show_memory_dump_1) {
		if (ImGui::Begin("Memory 1", &Show_memory_dump_1)) {
			memory_dump_1.draw();
		}
		ImGui::End();
	}

	if (Show_memory_dump_2) {
		if (ImGui::Begin("Memory 2", &Show_memory_dump_2)) {
			memory_dump_2.draw();
		}
		ImGui::End();
	}

	if (Show_cpu_monitor) {
		if (ImGui::Begin("CPU Monitor", &Show_cpu_monitor, ImGuiWindowFlags_NoScrollbar)) {
			draw_debugger_cpu_status();
		}
		ImGui::End();
	}

	if (Show_disassembler) {
		if (ImGui::Begin("Disassembler", &Show_disassembler)) {
			draw_debugger_controls();
			disasm.draw();
		}
		ImGui::End();
	}

	if (Show_breakpoints) {
		if (ImGui::Begin("Breakpoints", &Show_breakpoints)) {
			draw_breakpoints();
		}
		ImGui::End();
	}

	if (Show_watch_list) {
		if (ImGui::Begin("Watch list", &Show_watch_list)) {
			draw_watch_list();
		}
		ImGui::End();
	}

	if (Show_symbols_list) {
		if (ImGui::Begin("Symbols list", &Show_symbols_list)) {
			draw_symbols_list();
		}
		ImGui::End();
	}

	if (Show_symbols_files) {
		if (ImGui::Begin("Symbols files", &Show_symbols_files)) {
			draw_symbols_files();
		}
		ImGui::End();
	}

	if (Show_cpu_visualizer) {
		ImGui::SetNextWindowSize(ImVec2(816, 607), ImGuiCond_Once);
		if (ImGui::Begin("CPU Visualizer", &Show_cpu_visualizer)) {
			cpu_visualization_enable(Show_cpu_visualizer);
			draw_debugger_cpu_visualizer();
		} else {
			cpu_visualization_enable(Show_cpu_visualizer);
		}
		ImGui::End();
	}

	if (Show_VRAM_visualizer) {
		if (ImGui::Begin("Tile Visualizer", &Show_VRAM_visualizer)) {
			draw_debugger_vram_visualizer();
		}
		ImGui::End();
	}

	if (Show_VERA_monitor) {
		if (ImGui::Begin("VERA Monitor", &Show_VERA_monitor)) {
			vram_dump.draw();
			ImGui::SameLine();
			draw_debugger_vera_status();
		}
		ImGui::End();
	}

	if (Show_VERA_palette) {
		if (ImGui::Begin("Palette", &Show_VERA_palette)) {
			draw_debugger_vera_palette();
		}
		ImGui::End();
	}

	if (Show_VERA_layers) {
		if (ImGui::Begin("Layer Settings", &Show_VERA_layers)) {
			draw_debugger_vera_layer();
		}
		ImGui::End();
	}

	if (Show_VERA_sprites) {
		if (ImGui::Begin("Sprite Settings", &Show_VERA_sprites)) {
			draw_debugger_vera_sprite();
		}
		ImGui::End();
	}

#if defined(_DEBUG)
	if (Show_imgui_demo) {
		ImGui::ShowDemoWindow();
	}
#endif

	if (Show_VERA_PSG_monitor) {
		if (ImGui::Begin("VERA PSG", &Show_VERA_PSG_monitor)) {
			draw_debugger_vera_psg();
		}
		ImGui::End();
	}

	if (Show_YM2151_monitor) {
		if (ImGui::Begin("YM2151", &Show_YM2151_monitor)) {
			draw_debugger_ym2151();
		}
		ImGui::End();
	}

	if (Show_midi_overlay) {
		if (ImGui::Begin("MIDI Control", &Show_midi_overlay)) {
			draw_midi_overlay();
		}
		ImGui::End();
	}

	// Display should be the last one so it gets focused on startup
	if (Show_display) {
		float        title_bar_height = ImGui::GetFrameHeight();
#ifdef __APPLE__
		const char * window_text      = mouse_captured ? "Display (Cmd+M to release mouse)###display" : "Display###display";
#else
		const char * window_text      = mouse_captured ? "Display (Ctrl+M to release mouse)###display" : "Display###display";
#endif
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(80, 60), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::SetNextWindowDockID(dock_id, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(window_text, &Show_display)) {
			display_focused = ImGui::IsWindowFocused();
			// Shift + click on title bar to resize to the nearest integer scale
			if(ImGui::IsKeyDown(ImGuiKey_ModShift) && ImGui::IsItemClicked()) {
				ImGui::SetWindowSize(get_integer_scale_window_size(ImGui::GetContentRegionAvail()));
			}
			display_video();
		} else {
			display_focused = false;
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}
}

bool imgui_overlay_has_focus()
{
	return ImGui::IsAnyItemFocused();
}
