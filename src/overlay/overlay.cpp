#include "overlay.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>
#include <nfd.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

#include "cpu_visualization.h"
#include "disasm.h"
#include "ram_dump.h"
#include "util.h"
#include "vram_dump.h"

#include "audio.h"
#include "cpu/fake6502.h"
#include "debugger.h"
#include "display.h"
#include "glue.h"
#include "joystick.h"
#include "keyboard.h"
#include "options_menu.h"
#include "midi_overlay.h"
#include "smc.h"
#include "symbols.h"
#include "timing.h"
#include "vera/sdcard.h"
#include "vera/vera_psg.h"
#include "vera/vera_video.h"
#include "ym2151/ym2151.h"

bool Show_options          = false;
bool Show_imgui_demo       = false;
bool Show_memory_dump_1    = false;
bool Show_memory_dump_2    = false;
bool Show_cpu_monitor      = false;
bool Show_cpu_visualizer   = false;
bool Show_VRAM_visualizer  = false;
bool Show_VERA_monitor     = false;
bool Show_VERA_palette     = false;
bool Show_VERA_layers      = false;
bool Show_VERA_sprites     = false;
bool Show_VERA_PSG_monitor = false;
bool Show_YM2151_monitor   = false;
bool Show_midi_overlay     = false;

imgui_vram_dump vram_dump;

static void draw_debugger_cpu_status()
{
	ImGui::BeginGroup();
	{
		ImGui::TextDisabled("Status");
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(0.0f, 19.0f));
		ImGui::Separator();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));

		char const *names[] = { "N", "V", "-", "B", "D", "I", "Z", "C" };
		uint8_t     mask    = 0x80;
		int         n       = 0;
		while (mask > 0) {
			ImGui::BeginGroup();
			ImGui::Text("%s", names[n]);
			if (ImGui::SmallButton(status & mask ? "1" : "0")) {
				status ^= mask;
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
			ImGui::InputHexLabel("A", a);
			ImGui::InputHexLabel("X", x);
			ImGui::InputHexLabel("Y", y);
		}
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		{
			ImGui::InputHexLabel("PC", pc);
			ImGui::InputHexLabel("SP", sp);
		}
		ImGui::EndGroup();

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
		ImGui::Separator();

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
	ImGui::EndGroup();
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
	if (ImGui::BeginCombo("Hightlight type", vis_labels[h])) {
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
	ImGui::Image((void *)(intptr_t)vis.get_texture_id(), vis_imsize, vis.get_top_left(0), vis.get_bottom_right(0));
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
		static int      picker_index = 0;

		for (int i = 0; i < 256; ++i) {
			const uint8_t *p = reinterpret_cast<const uint8_t *>(&palette[i]);
			ImVec4         c{ (float)(p[2]) / 255.0f, (float)(p[1]) / 255.0f, (float)(p[0]) / 255.0f, 1.0f };
			ImGui::PushID(i);
			if (ImGui::ColorButton("Color##3f", c, ImGuiColorEditFlags_NoBorder, ImVec2(16, 16))) {
				ImGui::OpenPopup("palette_picker");
				backup_color = c;
				picker_index = i;
			}

			if (ImGui::BeginPopup("palette_picker")) {
				ImGui::ColorPicker3("##picker", (float *)&c, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueWheel);
				ImGui::SameLine();

				ImGui::BeginGroup(); // Lock X position
				ImGui::Text("Current");
				ImGui::ColorButton("##current", c, ImGuiColorEditFlags_NoPicker, ImVec2(60, 40));
				ImGui::Text("Previous");
				if (ImGui::ColorButton("##previous", backup_color, ImGuiColorEditFlags_NoPicker, ImVec2(60, 40))) {
					c = backup_color;
				}

				float *  f = (float *)(&c);
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
constexpr T bit_set_or_res(T val, T mask, bool cond)
{
	return cond ? (val | mask) : (val & ~mask);
}

static void draw_debugger_vera_sprite()
{
	static icon_set sprite_preview;
	static uint8_t  uncompressed_vera_memory[64 * 64];
	static uint32_t sprite_pixels[64 * 64];

	static ImU8     sprite_id  = 0;
	static uint64_t sprite_sig = 0;

	static const ImU8  incr_one8  = 1;
	static const ImU8  incr_hex8  = 16;
	static const ImU16 incr_one16 = 1;
	static const ImU16 incr_ten16 = 10;
	static const ImU16 incr_hex16 = 16;
	static const ImU16 incr_addr  = 32;
	static const ImU16 fast_addr  = 32 * 16;

	static bool reload = true;

	ImGui::BeginGroup();
	{
		if (ImGui::InputScalar("Sprite", ImGuiDataType_U8, &sprite_id, &incr_one8, nullptr, "%d")) {
			reload = true;
		}

		uint8_t sprite_data[8];
		memcpy(sprite_data, vera_video_get_sprite_data(sprite_id), 8);

		vera_video_sprite_properties sprite_props;
		memcpy(&sprite_props, vera_video_get_sprite_properties(sprite_id), sizeof(vera_video_sprite_properties));

		if (sprite_sig != *reinterpret_cast<const uint64_t *>(sprite_data)) {
			sprite_sig = *reinterpret_cast<const uint64_t *>(sprite_data);
			reload     = true;
		}

		const uint32_t num_dots = 1 << (sprite_props.sprite_width_log2 + sprite_props.sprite_height_log2);
		vera_video_get_expanded_vram_with_wraparound_handling(sprite_props.sprite_address, 4 << sprite_props.color_mode, uncompressed_vera_memory, num_dots);
		const uint32_t *palette = vera_video_get_palette_argb32();
		for (uint32_t i = 0; i < num_dots; ++i) {
			sprite_pixels[i] = (palette[uncompressed_vera_memory[i] + sprite_props.palette_offset] << 8) | 0xff;
		}
		if (reload) {
			sprite_preview.load_memory(sprite_pixels, sprite_props.sprite_width, sprite_props.sprite_height, sprite_props.sprite_width, sprite_props.sprite_height);
			reload = false;
		} else {
			sprite_preview.update_memory(sprite_pixels);
		}

		ImGui::BeginGroup();
		{
			ImGui::TextDisabled("Sprite Preview");
			ImGui::Image((void *)(intptr_t)sprite_preview.get_texture_id(), ImVec2(128.0f, 128.0f), sprite_preview.get_top_left(0), sprite_preview.get_bottom_right(0));
		}
		ImGui::EndGroup();
		ImGui::SameLine();
		ImGui::BeginGroup();
		{
			ImGui::TextDisabled("Raw Bytes");

			for (int i = 0; i < 8; ++i) {
				if (i) {
					ImGui::SameLine();
				}
				if (ImGui::InputHex(i, sprite_data[i])) {
					vera_video_space_write(0x1FC00 + 8 * sprite_id + i, sprite_data[i]);
				}
			}
		}
		ImGui::NewLine();
		{
			ImGui::PushItemWidth(128.0f);

			ImGui::TextDisabled("Sprite Properties");

			if (ImGui::InputScalar("VRAM Addr", ImGuiDataType_U16, &sprite_props.sprite_address, &incr_addr, &fast_addr, "%05X", ImGuiInputTextFlags_CharsHexadecimal)) {
				vera_video_space_write(0x1FC00 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_address >> 5));
				vera_video_space_write(0x1FC01 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_address >> 13) | (sprite_props.color_mode << 7));
			}
			bool eight_bit = sprite_props.color_mode;
			if (ImGui::Checkbox("8bit Color", &eight_bit)) {
				vera_video_space_write(0x1FC01 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_address >> 13) | (sprite_props.color_mode << 7));
			}
			if (ImGui::InputScalar("Pos X", ImGuiDataType_U16, &sprite_props.sprite_x, &incr_one16, &incr_ten16, "%d")) {
				vera_video_space_write(0x1FC02 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_x));
				vera_video_space_write(0x1FC03 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_x >> 8));
			}
			if (ImGui::InputScalar("Pos Y", ImGuiDataType_U16, &sprite_props.sprite_y, &incr_one16, &incr_ten16, "%d")) {
				vera_video_space_write(0x1FC04 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_y));
				vera_video_space_write(0x1FC05 + 8 * sprite_id, (uint8_t)(sprite_props.sprite_y >> 8));
			}
			if (ImGui::Checkbox("h-flip", &sprite_props.hflip)) {
				vera_video_space_write(0x1FC06 + 8 * sprite_id, (uint8_t)(sprite_props.hflip ? 0x1 : 0) | (uint8_t)(sprite_props.vflip ? 0x2 : 0) | (uint8_t)((sprite_props.sprite_zdepth & 3) << 2) | (uint8_t)((sprite_props.sprite_collision_mask & 0xf) << 4));
			}
			if (ImGui::Checkbox("v-flip", &sprite_props.vflip)) {
				vera_video_space_write(0x1FC06 + 8 * sprite_id, (uint8_t)(sprite_props.hflip ? 0x1 : 0) | (uint8_t)(sprite_props.vflip ? 0x2 : 0) | (uint8_t)((sprite_props.sprite_zdepth & 3) << 2) | (uint8_t)((sprite_props.sprite_collision_mask & 0xf) << 4));
			}
			if (ImGui::InputScalar("Z-depth", ImGuiDataType_U8, &sprite_props.sprite_zdepth, &incr_one8, nullptr, "%d")) {
				vera_video_space_write(0x1FC06 + 8 * sprite_id, (uint8_t)(sprite_props.hflip ? 0x1 : 0) | (uint8_t)(sprite_props.vflip ? 0x2 : 0) | (uint8_t)((sprite_props.sprite_zdepth & 3) << 2) | (uint8_t)((sprite_props.sprite_collision_mask & 0xf) << 4));
			}
			if (ImGui::InputScalar("Collision", ImGuiDataType_U8, &sprite_props.sprite_collision_mask, &incr_hex8, nullptr, "%1x")) {
				vera_video_space_write(0x1FC06 + 8 * sprite_id, (uint8_t)(sprite_props.hflip ? 0x1 : 0) | (uint8_t)(sprite_props.vflip ? 0x2 : 0) | (uint8_t)((sprite_props.sprite_zdepth & 3) << 2) | (uint8_t)((sprite_props.sprite_collision_mask & 0xf0)));
			}
			if (ImGui::InputScalar("Palette Offset", ImGuiDataType_U16, &sprite_props.palette_offset, &incr_hex16, nullptr, "%d")) {
				vera_video_space_write(0x1FC07 + 8 * sprite_id, (uint8_t)((sprite_props.palette_offset >> 4) & 0xf) | (uint8_t)(((sprite_props.sprite_width_log2 - 3) & 0x3) << 4) | (uint8_t)(((sprite_props.sprite_height_log2 - 3) & 0x3) << 6));
			}
			if (ImGui::InputScalar("Width", ImGuiDataType_U8, &sprite_props.sprite_width_log2, &incr_one8, nullptr, "%d")) {
				vera_video_space_write(0x1FC07 + 8 * sprite_id, (uint8_t)((sprite_props.palette_offset >> 4) & 0xf) | (uint8_t)(((sprite_props.sprite_width_log2 - 3) & 0x3) << 4) | (uint8_t)(((sprite_props.sprite_height_log2 - 3) & 0x3) << 6));
			}
			if (ImGui::InputScalar("Height", ImGuiDataType_U8, &sprite_props.sprite_height_log2, &incr_one8, nullptr, "%d")) {
				vera_video_space_write(0x1FC07 + 8 * sprite_id, (uint8_t)((sprite_props.palette_offset >> 4) & 0xf) | (uint8_t)(((sprite_props.sprite_width_log2 - 3) & 0x3) << 4) | (uint8_t)(((sprite_props.sprite_height_log2 - 3) & 0x3) << 6));
			}
			ImGui::PopItemWidth();
		}
		ImGui::EndGroup();
	}
	ImGui::EndGroup();
}

class vram_visualizer
{
public:
	void draw_preview()
	{
		capture_vram();

		ImGui::BeginGroup();
		{
			ImGui::TextDisabled("Preview");

			const int tiles_per_row    = 128 >> graphics_props.tilew_log2;
			const int tiles_per_column = 128 >> graphics_props.tileh_log2;
			ImGui::BeginChild("tiles", ImVec2(256.0f + (5 * 16) + 10, 256.0f + (5 * 16)));
			{
				if (graphics_props.bitmap_mode) {
					ImVec2 tile_imsize(256.0f + (5 * 16), 256.0f + (5 * 16));
					ImGui::Image((void *)(intptr_t)tiles_preview.get_texture_id(), tile_imsize, tiles_preview.get_top_left(0), tiles_preview.get_bottom_right(0));
				} else {
					ImVec2 custom_spacing((float)(5 << (graphics_props.tilew_log2 - 3)), (float)(5 << (graphics_props.tileh_log2 - 3)));
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, custom_spacing);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));

					ImVec2 tile_imsize((float)(graphics_props.tilew << 1), (float)(graphics_props.tileh << 1));

					ImGuiListClipper clipper;
					clipper.Begin(tiles_per_column, graphics_props.tileh + custom_spacing.y);

					while (clipper.Step()) {
						uint16_t start_tile = clipper.DisplayStart * tiles_per_row;
						uint16_t end_tile   = clipper.DisplayEnd * tiles_per_row;
						if (end_tile > 1024) {
							end_tile = 1024;
						}
						for (int i = start_tile; i < end_tile; ++i) {
							if (i % tiles_per_row) {
								ImGui::SameLine();
							}
							ImGui::Image((void *)(intptr_t)tiles_preview.get_texture_id(), tile_imsize, tiles_preview.get_top_left(i), tiles_preview.get_bottom_right(i));
						}
					}
					clipper.End();

					ImGui::PopStyleVar();
					ImGui::PopStyleVar();
				}
				ImGui::EndChild();
			}
			ImGui::PushItemWidth(128.0f);

			const ImU16 incr_hex16 = 16;
			ImGui::InputScalar("Preview Palette Offset", ImGuiDataType_U16, &tile_palette_offset, &incr_hex16, nullptr, "%d");
			ImGui::PopItemWidth();
		}
		ImGui::EndGroup();
	}

	void draw_preview_widgets()
	{
		const ImU32 incr_tile = 1 << (graphics_props.tilew_log2 + graphics_props.tileh_log2 - (3 - graphics_props.color_depth));
		const ImU32 fast_tile = incr_tile << 4;

		ImGui::BeginGroup();
		{
			ImGui::TextDisabled("Graphics Properties");

			ImGui::PushItemWidth(128.0f);

			if (ImGui::InputLog2("Bits per pixel", &graphics_props.color_depth, "%d")) {
				if (graphics_props.color_depth > 3) {
					graphics_props.color_depth = 3;
				}
			}
			if (ImGui::Checkbox("Bitmap Data", &graphics_props.bitmap_mode)) {
				if (graphics_props.bitmap_mode) {
					graphics_props.tilew = 320;
				} else {
					graphics_props.tileh_log2 = 3;
					graphics_props.tileh      = 1 << graphics_props.tileh_log2;
					graphics_props.tileh_max  = graphics_props.tileh - 1;

					graphics_props.tilew_log2 = 3;
					graphics_props.tilew      = 1 << graphics_props.tilew_log2;
					graphics_props.tilew_max  = graphics_props.tilew - 1;
				}
			}
			if (graphics_props.bitmap_mode) {
				static const char *labels[] = { "320", "640" };
				static const int   widths[] = { 320, 640 };
				if (ImGui::BeginCombo("Bitmap width", graphics_props.tilew == 320 ? labels[0] : labels[1])) {
					for (int i = 0; i < 2; ++i) {
						if (ImGui::Selectable(labels[i], graphics_props.tilew == widths[i])) {
							graphics_props.tilew = widths[i];
						}
					}
					ImGui::EndCombo();
				}
			} else {
				if (ImGui::InputLog2("Gfx Height", &graphics_props.tileh_log2, "%d")) {
					if (graphics_props.tileh_log2 < 3) {
						graphics_props.tileh_log2 = 3;
					} else if (graphics_props.tileh_log2 > 6) {
						graphics_props.tileh_log2 = 6;
					}
					graphics_props.tileh     = 1 << graphics_props.tileh_log2;
					graphics_props.tileh_max = graphics_props.tileh - 1;
				}

				if (ImGui::InputLog2("Gfx Width", &graphics_props.tilew_log2, "%d")) {
					if (graphics_props.tilew_log2 < 3) {
						graphics_props.tilew_log2 = 3;
					} else if (graphics_props.tilew_log2 > 6) {
						graphics_props.tilew_log2 = 6;
					}
					graphics_props.tilew     = 1 << graphics_props.tilew_log2;
					graphics_props.tilew_max = graphics_props.tilew - 1;
				}
			}

			if (ImGui::InputScalar("Gfx Base", ImGuiDataType_U32, &graphics_props.tile_base, &incr_tile, &fast_tile, "%05X", ImGuiInputTextFlags_CharsHexadecimal)) {
				graphics_props.tile_base %= 0x20000;
			}

			ImGui::PopItemWidth();
		}
		ImGui::EndGroup();
	}

	void capture_vram()
	{
		uint8_t  uncompressed_vera_memory[0x20000];
		uint32_t tile_pixels[0x20000];

		if (graphics_props.bitmap_mode) {
			const uint32_t num_dots = graphics_props.tilew * 256;
			vera_video_get_expanded_vram_with_wraparound_handling(graphics_props.tile_base, 1 << graphics_props.color_depth, uncompressed_vera_memory, num_dots);

			const uint32_t *palette = vera_video_get_palette_argb32();
			for (uint32_t i = 0; i < num_dots; ++i) {
				tile_pixels[i] = (palette[uncompressed_vera_memory[i] + tile_palette_offset] << 8) | 0xff;
			}

			tiles_preview.load_memory(tile_pixels, graphics_props.tilew, 256, graphics_props.tilew, 256);
		} else {
			constexpr const uint32_t num_dots = 128 * 256;
			vera_video_get_expanded_vram_with_wraparound_handling(graphics_props.tile_base, 1 << graphics_props.color_depth, uncompressed_vera_memory, num_dots);

			const uint32_t *palette = vera_video_get_palette_argb32();
			for (uint32_t i = 0; i < num_dots; ++i) {
				tile_pixels[i] = (palette[uncompressed_vera_memory[i] + tile_palette_offset] << 8) | 0xff;
			}

			tiles_preview.load_memory(tile_pixels, graphics_props.tilew, num_dots >> graphics_props.tilew_log2, graphics_props.tilew, graphics_props.tileh);
		}
	}

	void set_params(const vera_video_layer_properties &props)
	{
		graphics_props.color_depth = props.color_depth;
		graphics_props.tile_base   = props.tile_base;
		graphics_props.bitmap_mode = props.bitmap_mode;
		graphics_props.tilew       = props.tilew;
		graphics_props.tileh       = props.tileh;
		graphics_props.tilew_log2  = props.tilew_log2;
		graphics_props.tileh_log2  = props.tileh_log2;
		graphics_props.tilew_max   = props.tilew_max;
		graphics_props.tileh_max   = props.tileh_max;
	}

private:
	icon_set tiles_preview;
	uint16_t tile_palette_offset = 0;

	struct {
		uint8_t  color_depth;
		uint32_t tile_base;

		bool bitmap_mode;

		uint16_t tilew;
		uint16_t tileh;
		uint8_t  tilew_log2;
		uint8_t  tileh_log2;

		uint16_t tilew_max;
		uint16_t tileh_max;

	} graphics_props{
		0, //uint8_t  color_depth;
		0, //uint32_t tile_base;

		false, //bool bitmap_mode;

		8, //uint16_t tilew;
		8, //uint16_t tileh;
		3, //uint8_t  tilew_log2;
		3, //uint8_t  tileh_log2;

		7, //uint16_t tilew_max;
		7, //uint16_t tileh_max;
	};
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
				ImDrawList * draw_list = ImGui::GetWindowDrawList();
				const ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
				ImVec2       winsize    = ImGui::GetWindowSize();
				ImVec2       wintopleft = topleft;
				wintopleft.x += scroll.x;
				wintopleft.y += scroll.y;
				ImVec2 winbotright(wintopleft.x + std::min((float)total_width, winsize.x), wintopleft.y + std::min((float)total_height, winsize.y));
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
					float          hcnt = std::floorf(scroll.x / tile_width) * tile_width + topleft.x;
					while (hcnt < winbotright.x) {
						draw_list->AddLine(ImVec2(hcnt, wintopleft.y), ImVec2(hcnt, winbotright.y), col);
						hcnt += tile_width;
					}
					float vcnt = std::floorf(scroll.y / tile_height) * tile_height + topleft.y;
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
				draw_list->AddRect(ImVec2(sel_x - 2, sel_y - 2), ImVec2(sel_x + tile_width + 2, sel_y + tile_height + 2), IM_COL32_BLACK);
				draw_list->AddRect(ImVec2(sel_x - 1, sel_y - 1), ImVec2(sel_x + tile_width + 1, sel_y + tile_height + 1), IM_COL32_WHITE);
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
		const uint32_t *      palette_argb = vera_video_get_palette_argb32();

		for (int i = 0; i < 256; i++) {
			// convert argb to rgba
			palette[i] = (palette_argb[i] << 8) | 0xff;
		}

		// get DC registers and determine a screen size
		screen_width = (float)(vera_video_get_dc_hstop() - vera_video_get_dc_hstart()) * vera_video_get_dc_hscale() / 32.f;
		screen_height = (float)(vera_video_get_dc_vstop() - vera_video_get_dc_vstart()) * vera_video_get_dc_vscale() / 64.f;

		if (bitmap_mode) {
			const uint32_t num_dots = tile_width * 480;
			pixels.resize(num_dots);
			vera_video_get_expanded_vram_with_wraparound_handling(tile_base, bpp, tile_data, num_dots);

			for (uint32_t i = 0; i < num_dots; i++) {
				uint8_t tdat = tile_data[i];
				if (tdat > 0 && tdat < 16) // 8bpp quirk handling
					tdat += palette_offset;
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
						const uint16_t tinfo = map_data[tidx] + (map_data[tidx + 1] << 8);
						const bool     hflip = tinfo & (1 << 10);
						const bool     vflip = tinfo & (1 << 11);
						const uint16_t tnum  = tinfo & 0x3FF;
						const uint8_t  pal   = (tinfo >> 12) * 16;
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
								if (tdat > 0 && tdat < 16)
									tdat += pal;
								pixels[dst2++] = palette[tdat];
							}
						}
						dst += tile_width;
						tidx += 2;
					}
				}
			}
		}
		tiles_preview.load_memory(pixels.data(), total_width, total_height, total_width, total_height);
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
		scroll_x       = props.hscroll % total_width;
		scroll_y       = props.vscroll % total_height;
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
	viz.capture_vram();
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
			if (ImGui::BeginTable("breakpoints", 5, ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 27);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 27);
				ImGui::TableSetupColumn("Address");
				ImGui::TableSetupColumn("Bank");
				ImGui::TableSetupColumn("Symbol");
				ImGui::TableHeadersRow();

				const auto &breakpoints = debugger_get_breakpoints();
				for (auto &[address, bank] : breakpoints) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (ImGui::TileButton(ICON_REMOVE)) {
						debugger_remove_breakpoint(address, bank);
						break;
					}

					ImGui::TableNextColumn();
					if (debugger_breakpoint_is_active(address, bank)) {
						if (ImGui::TileButton(ICON_CHECKED)) {
							debugger_deactivate_breakpoint(address, bank);
						}
					} else {
						if (ImGui::TileButton(ICON_UNCHECKED)) {
							debugger_activate_breakpoint(address, bank);
						}
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
			if (ImGui::ListBoxHeader("Filtered Symbols")) {
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
							}
						}
						any_selected_visible = any_selected_visible || is_selected;
						ImGui::PopID();
					}
				});
				selected = any_selected_visible;
				ImGui::ListBoxFooter();
			}

			if (ImGui::Button("Add Breakpoint at Symbol") && selected) {
				debugger_add_breakpoint(selected_addr, selected_bank);
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
			if (ImGui::BeginTable("symbols", 3, ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 27);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 27);
				ImGui::TableSetupColumn("Path");
				ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
				ImGui::TableSetColumnIndex(1);

				const auto &files = symbols_get_loaded_files();

				if (symbols_file_all_are_visible()) {
					if (ImGui::TileButton(ICON_CHECKED)) {
						for (auto file : files) {
							symbols_hide_file(file);
						}
					}
				} else if (symbols_file_any_is_visible()) {
					if (ImGui::TileButton(ICON_CHECK_UNCERTAIN)) {
						for (auto file : files) {
							symbols_hide_file(file);
						}
					}
				} else {
					if (ImGui::TileButton(ICON_UNCHECKED)) {
						for (auto file : files) {
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
	bool shifted = ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT) || ImGui::IsKeyDown(SDL_SCANCODE_RSHIFT);

	static bool stop_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STOP_DISABLED : ICON_STOP, !paused, &stop_hovered) || (shifted && ImGui::IsKeyPressed(SDL_SCANCODE_F5))) {
		debugger_pause_execution();
		disasm.follow_pc();
	}
	if (!paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Pause execution (Shift+F5)");
	}

	ImGui::SameLine();

	static bool run_hovered = false;
	if (ImGui::TileButton(paused ? ICON_RUN : ICON_RUN_DISABLED, paused, &run_hovered) || (!shifted && ImGui::IsKeyPressed(SDL_SCANCODE_F5))) {
		debugger_continue_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Run (F5)");
	}
	ImGui::SameLine();

	static bool step_over_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STEP_OVER : ICON_STEP_OVER_DISABLED, paused, &step_over_hovered) || (!shifted && ImGui::IsKeyPressed(SDL_SCANCODE_F10))) {
		debugger_step_over_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step Over (F10)");
	}
	ImGui::SameLine();

	static bool step_into_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STEP_INTO : ICON_STEP_INTO_DISABLED, paused, &step_into_hovered) || (!shifted && ImGui::IsKeyPressed(SDL_SCANCODE_F11))) {
		debugger_step_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step Into (F11)");
	}
	ImGui::SameLine();

	static bool step_out_hovered = false;
	if (ImGui::TileButton(paused ? ICON_STEP_OUT : ICON_STEP_OUT_DISABLED, paused, &step_out_hovered) || (shifted && ImGui::IsKeyPressed(SDL_SCANCODE_F11))) {
		debugger_step_out_execution();
		disasm.follow_pc();
	}
	if (paused && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step Out (Shift+F11)");
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

static void draw_debugger_vera_psg()
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

		static char chtxt[3];
		for (unsigned int i = 0; i < 16; ++i) {
			ImGui::TableNextRow();
			if (i == 0) {
				ImGui::TableSetColumnIndex(2); // freq
				ImGui::PushItemWidth(-FLT_MIN); // Right-aligned
				ImGui::TableSetColumnIndex(3); // wave
				ImGui::PushItemWidth(-FLT_MIN);
				ImGui::TableSetColumnIndex(4); // width
				ImGui::PushItemWidth(-FLT_MIN);
				ImGui::TableSetColumnIndex(7); // vol
				ImGui::PushItemWidth(-FLT_MIN);
				ImGui::TableSetColumnIndex(0);
			} else {
				ImGui::TableNextColumn();
			}

			std::sprintf(chtxt, "%d", i);
			ImGui::PushID(i);
			const psg_channel *channel = psg_get_channel(i);

			ImGui::Text(chtxt);

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
			if (ImGui::SliderFloat("", &freq, 0, 0xffff, "%.0f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp)) {
				psg_set_channel_frequency(i, (uint16_t)freq);
			}
			ImGui::PopID();

			ImGui::TableNextColumn();
			static const char *waveforms[] = {
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

	int16_t psg_buffer[2 * SAMPLES_PER_BUFFER];
	audio_get_psg_buffer(psg_buffer);
	{
		float left_samples[SAMPLES_PER_BUFFER];
		float right_samples[SAMPLES_PER_BUFFER];

		float *l = left_samples;
		float *r = right_samples;

		const int16_t *b = psg_buffer;
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
}

static void ym2151_reg_input(uint8_t *regs, uint8_t idx)
{
	ImGui::TableSetColumnIndex((idx & 0xf) + 1);
	if (ImGui::InputHex(idx, regs[idx])) {
		YM_debug_write(idx, regs[idx]);
	}
}

static void draw_debugger_ym2151()
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
			static const char *row_txts[16] = {
				"0x", "1x", "2x", "3x", "4x", "5x", "6x", "7x", "8x", "9x", "Ax", "Bx", "Cx", "Dx", "Ex", "Fx"
			};
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(row_txts[0]);
			ym2151_reg_input(regs, 0x01); // TEST
			ym2151_reg_input(regs, 0x08); // KEYON
			ym2151_reg_input(regs, 0x0F); // NOISE
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(row_txts[1]);
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
				ImGui::Text(row_txts[i]);
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
				if (ImGui::SliderInt("Reload", &tim->reload, 0, TIM_MAX, "%d", ImGuiSliderFlags_AlwaysClamp)) {
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
				std::sprintf(buf, "%d", tim->cur);
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
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("CT1", &ct1)) {
				YM_debug_write(0x1B, bit_set_or_res(regs[0x1B], (uint8_t)(1 << 6), ct1));
			}
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("CT2", &ct2)) {
				YM_debug_write(0x1B, bit_set_or_res(regs[0x1B], (uint8_t)(1 << 7), ct2));
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("LFO & Noise", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
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
			float         lcnt      = YM_get_LFO_phase();
			int           amd       = YM_get_AMD();
			int           pmd       = YM_get_PMD();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("LFO");
			ImGui::SameLine(72);
			if (ImGui::Checkbox("Reset", &lres)) {
				YM_debug_write(0x01, bit_set_or_res(regs[0x01], LRES_MASK, lres));
			}
			ImGui::TableNextColumn();
			if (ImGui::Combo("Waveform", &lw, waveforms, 4)) {
				YM_debug_write(0x1B, regs[0x1B] & ~LW_MASK | lw);
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::SliderInt("LFO Freq", &lfrq, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				YM_debug_write(0x18, lfrq);
			}
			ImGui::TableNextColumn();
			char buf[4];
			std::sprintf(buf, "%d", (int)(lcnt * 256));
			ImGui::ProgressBar(lcnt, ImVec2(0, 0), buf);
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::Text("Cur. Phase");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::SliderInt("AMD", &amd, 0, 127, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				YM_debug_write(0x19, amd);
			}
			ImGui::TableNextColumn();
			if (ImGui::SliderInt("PMD", &pmd, 0, 127, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				YM_debug_write(0x19, pmd | 0x80);
			}

			const uint8_t NEN_MASK  = (1 << 7);
			const uint8_t NFRQ_MASK = 0x1F;
			bool          nen       = regs[0x0F] & NEN_MASK;
			int           nfrq      = regs[0x0F] & NFRQ_MASK;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Noise");
			ImGui::SameLine(72);
			if (ImGui::Checkbox("Enable", &nen)) {
				YM_debug_write(0x0F, bit_set_or_res(regs[0x0F], NEN_MASK, nen));
			}
			ImGui::TableNextColumn();
			if (ImGui::SliderInt("Frequency", &nfrq, 31, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				YM_debug_write(0x0F, regs[0x0F] & ~NFRQ_MASK | nfrq);
			}

			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Channels", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::BeginTable("ym channels", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInner)) {
			static const uint8_t slot_map[4] = { 0, 16, 8, 24 };
			static struct {
				int  con, fb;
				bool l, r;
				float kc;
				int  ams, pms;
				bool  debug_kon[4] = { 1, 1, 1, 1};
				int   dkob_state   = 0;
				struct {
					int  dt1, dt2, mul;
					int  ar, d1r, d1l, d2r, rr, ks;
					int  tl;
					bool ame;
				} slot[4];
			} channel[8];

			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed); // chan num
			ImGui::TableSetupColumn("", 0, 0.4f); // chan regs
			ImGui::TableSetupColumn(""); // slot regs
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed); // slot con

			for (int i = 0; i < 8; i++) {
				const uint8_t confb  = 0x20 + i;
				const uint8_t kc     = 0x28 + i;
				const uint8_t kf     = 0x30 + i;
				const uint8_t amspms = 0x38 + i;
				const uint8_t tmp_kc = regs[kc] & 0x7f;
				auto          ch     = &channel[i];

				ch->l    = regs[confb] & (1 << 6);
				ch->r    = regs[confb] & (1 << 7);
				ch->con  = regs[confb] & 0x07;
				ch->fb   = (regs[confb] >> 3) & 0x07;
				ch->kc   = (tmp_kc - ((tmp_kc + 1) >> 2)) + (regs[kf] / 256.f);
				ch->ams  = regs[amspms] & 0x03;
				ch->pms  = (regs[amspms] >> 4) & 0x07;
				int fpkc = (int)(ch->kc * 256);

				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%d", i);

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
					if (ImGui::Checkbox("L", &ch->l)) {
						YM_debug_write(confb, bit_set_or_res(regs[confb], (uint8_t)(1 << 6), ch->l));
					}
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(12);
					if (ImGui::Checkbox("R", &ch->r)) {
						YM_debug_write(confb, bit_set_or_res(regs[confb], (uint8_t)(1 << 7), ch->r));
					}
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(12);
					if (ImGui::DragInt("CON", &ch->con, 1, 0, 7, "%d", ImGuiSliderFlags_AlwaysClamp)) {
						YM_debug_write(confb, regs[confb] & ~0x07 | ch->con);
					}
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-28);
					if (ImGui::SliderInt("FB", &ch->fb, 0, 7, "%d", ImGuiSliderFlags_AlwaysClamp)) {
						YM_debug_write(confb, regs[confb] & ~0x38 | (ch->fb << 3));
					}
					ImGui::EndTable();
				}

				if (ImGui::BeginTable("amspms", 2)) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-28);
					if (ImGui::SliderInt("AMS", &ch->ams, 0, 3, "%d", ImGuiSliderFlags_AlwaysClamp)) {
						YM_debug_write(amspms, regs[amspms] & ~0x03 | ch->ams);
					}
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-28);
					if (ImGui::SliderInt("PMS", &ch->pms, 0, 7, "%d", ImGuiSliderFlags_AlwaysClamp)) {
						YM_debug_write(amspms, regs[amspms] & ~0x70 | (ch->pms << 4));
					}
					ImGui::EndTable();
				}

				const char    notes[] = "C-C#D-D#E-F-F#G-G#A-A#B-";
				float         cents   = (fpkc & 0xFF) * 100.f / 256.f;
				if (cents > 50) {
					cents = cents - 100;
				}
				const uint8_t note    = (fpkc >> 8) + (cents < 0) + 1;
				const uint8_t ni      = (note % 12) * 2;
				const uint8_t oct     = note / 12;
				// C#8 +00.0
				char kcinfo[12];
				std::sprintf(kcinfo, "%c%c%d %+05.1f", notes[ni], notes[ni + 1], oct, cents);
				ImGui::SetNextItemWidth(-28);
				if (ImGui::SliderFloat("KC", &ch->kc, 0, 96, kcinfo, ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_AlwaysClamp)) {
					fpkc = std::min((int)(ch->kc * 256), (96 * 256) - 1);
					YM_debug_write(kc, (fpkc >> 8) * 4 / 3);
					YM_debug_write(kf, fpkc & 0xFF);
				}

				ImGui::Button("KeyOn");
				ch->dkob_state = (ch->dkob_state << 1) | (int)ImGui::IsItemActive();
				switch (ch->dkob_state & 0b11) {
					case 0b01: // keyon checked slots
						YM_debug_write(0x08, i | (ch->debug_kon[0] << 3) | (ch->debug_kon[1] << 4) | (ch->debug_kon[2] << 5) | (ch->debug_kon[3] << 6));
						break;
					case 0b10: // keyoff all slots
						YM_debug_write(0x08, i);
						break;
				}
				ImGui::PushID("konslots");
				for (int j = 0; j < 4; j++) {
					ImGui::PushID(j);
					ImGui::SameLine();
					ImGui::Checkbox("", &ch->debug_kon[j]);
					ImGui::PopID();
				}
				ImGui::PopID();
				ImGui::PopStyleVar(3);
				
				// Slot
				ImGui::TableNextColumn();
				ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 2));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 0));
				ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 6);
				if (ImGui::BeginTable("slot", 15)) {
					ImGui::TableSetupColumn("Sl", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("DT1");
					ImGui::TableSetupColumn("DT2");
					ImGui::TableSetupColumn("MUL");
					ImGui::TableSetupColumn("=Freq", ImGuiTableColumnFlags_WidthFixed, 44);
					ImGui::TableSetupColumn("AR");
					ImGui::TableSetupColumn("D1R");
					ImGui::TableSetupColumn("D1L");
					ImGui::TableSetupColumn("D2R");
					ImGui::TableSetupColumn("RR");
					ImGui::TableSetupColumn("KS");
					ImGui::TableSetupColumn("Env");
					ImGui::TableSetupColumn("TL");
					ImGui::TableSetupColumn("AM", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Out");
					ImGui::TableHeadersRow();

					for (int j = 0; j < 4; j++) {
						const int     slnum  = slot_map[j] + i;
						const uint8_t muldt1 = 0x40 + slnum;
						const uint8_t tl     = 0x60 + slnum;
						const uint8_t arks   = 0x80 + slnum;
						const uint8_t d1rame = 0xA0 + slnum;
						const uint8_t d2rdt2 = 0xC0 + slnum;
						const uint8_t rrd1l  = 0xE0 + slnum;
						auto          slot   = &(channel[i].slot[j]);

						slot->mul = regs[muldt1] & 0x0F;
						slot->dt1 = (regs[muldt1] >> 4) & 0x07;
						slot->tl  = regs[tl] & 0x7F;
						slot->ar  = regs[arks] & 0x1F;
						slot->ks  = regs[arks] >> 6;
						slot->d1r = regs[d1rame] & 0x1F;
						slot->ame = regs[d1rame] & 0x80;
						slot->d2r = regs[d2rdt2] & 0x1F;
						slot->dt2 = regs[d2rdt2] >> 6;
						slot->rr  = regs[rrd1l] & 0x0F;
						slot->d1l = regs[rrd1l] >> 4;

						ImGui::PushID(j);
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::Text("%d", slot_map[j] + i);
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("dt1", &slot->dt1, 0, 7, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(muldt1, regs[muldt1] & ~0x70 | (slot->dt1 << 4));
						}
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("dt2", &slot->dt2, 0, 3, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(d2rdt2, regs[d2rdt2] & ~0xC0 | (slot->dt2 << 6));
						}
						ImGui::TableNextColumn();
						char buf[11] = ".5";
						if (slot->mul > 0) {
							std::sprintf(buf, "%d", slot->mul);
						}
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("mul", &slot->mul, 0, 15, buf, ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(muldt1, regs[muldt1] & ~0x0F | slot->mul);
						}
						ImGui::TableNextColumn();
						ImGui::Text("%d", YM_get_freq(slnum));
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("ar", &slot->ar, 31, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(arks, regs[arks] & ~0x1F | slot->ar);
						}
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("d1r", &slot->d1r, 31, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(d1rame, regs[d1rame] & ~0x1F | slot->d1r);
						}
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("d1l", &slot->d1l, 15, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(rrd1l, regs[rrd1l] & ~0xF0 | (slot->d1l << 4));
						}
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("d2r", &slot->d2r, 31, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(d2rdt2, regs[d2rdt2] & ~0x1F | slot->d2r);
						}
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("rr", &slot->rr, 15, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(rrd1l, regs[rrd1l] & ~0x0F | slot->rr);
						}
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("ks", &slot->ks, 0, 3, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(arks, regs[arks] & ~0xC0 | (slot->ks << 6));
						}
						ImGui::TableNextColumn();
						char envstate_txt[] = " ";
						envstate_txt[0]     = " ADSR"[YM_get_env_state(slnum)];
						ImGui::ProgressBar(YM_get_EG_output(slnum), ImVec2(-FLT_MIN, 0), envstate_txt);
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (ImGui::SliderInt("tl", &slot->tl, 127, 0, "%d", ImGuiSliderFlags_AlwaysClamp)) {
							YM_debug_write(tl, regs[tl] & ~0x7F | slot->tl);
						}
						ImGui::TableNextColumn();
						ImGui::PushID("amelia");
						if (ImGui::Checkbox("", &slot->ame)) {
							YM_debug_write(d1rame, bit_set_or_res(regs[d1rame], (uint8_t) 0x80, slot->ame));
						}
						ImGui::PopID();
						float out = YM_get_final_env(slnum);
						char  buf2[5];
						std::sprintf(buf2, "%d", (int)((1 - out) * 1024));
						ImGui::TableNextColumn();
						ImGui::ProgressBar(out, ImVec2(-FLT_MIN, 0), buf2);

						ImGui::PopID();
					}
					ImGui::EndTable();
				}
				ImGui::PopStyleVar(3);

				// CON gfx
				ImGui::TableNextColumn();
				ImGui::Dummy(ImVec2(16, 15));
				// this is so ugly
				ImGui::Tile((display_icons)((int)ICON_FM_ALG + ch->con), ImVec2(16, 64));

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
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
				machine_dump();
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
					strcpy(Options.hyper_path, open_path);
				}
			}

			ImGui::Separator();

			bool warp_mode = Options.warp_factor > 0;
			if (ImGui::Checkbox("Enable Warp Mode", &warp_mode)) {
				if (Options.warp_factor > 0) {
					Options.warp_factor = 0;
					vera_video_set_cheat_mask(0);
				} else {
					Options.warp_factor = 1;
					vera_video_set_cheat_mask(0x3f);
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows")) {
			if (ImGui::BeginMenu("CPU Debugging")) {
				ImGui::Checkbox("Memory Dump 1", &Show_memory_dump_1);
				ImGui::Checkbox("Memory Dump 2", &Show_memory_dump_2);
				ImGui::Checkbox("ASM Monitor", &Show_cpu_monitor);
				if (ImGui::Checkbox("CPU Visualizer", &Show_cpu_visualizer)) {
					cpu_visualization_enable(Show_cpu_visualizer);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("VERA Debugging")) {
				ImGui::Checkbox("VRAM Visualizer", &Show_VRAM_visualizer);
				ImGui::Checkbox("VERA Monitor", &Show_VERA_monitor);
				ImGui::Checkbox("Palette", &Show_VERA_palette);
				ImGui::Checkbox("Layer Settings", &Show_VERA_layers);
				ImGui::Checkbox("Sprite Settings", &Show_VERA_sprites);
				ImGui::EndMenu();
			}
			ImGui::Checkbox("PSG Monitor", &Show_VERA_PSG_monitor);
			ImGui::Checkbox("YM2151 Monitor", &Show_YM2151_monitor);
			ImGui::Separator();

			bool safety_frame = vera_video_safety_frame_is_enabled();
			if (ImGui::Checkbox("Show Safety Frame", &safety_frame)) {
				vera_video_enable_safety_frame(safety_frame);
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
		ImGui::Text("Speed: %d%%", Timing_perf);
		ImGui::EndMainMenuBar();
	}
}

void overlay_draw()
{
	draw_menu_bar();

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
		if (ImGui::Begin("ASM Monitor", &Show_cpu_monitor)) {
			draw_debugger_controls();
			disasm.draw();
			ImGui::SameLine();
			draw_debugger_cpu_status();
			draw_breakpoints();
			draw_symbols_list();
			draw_symbols_files();
		}
		ImGui::End();
	}

	if (Show_cpu_visualizer) {
		if (ImGui::Begin("CPU Visualizer", &Show_cpu_visualizer)) {
			cpu_visualization_enable(Show_cpu_visualizer);
			draw_debugger_cpu_visualizer();
		} else {
			cpu_visualization_enable(Show_cpu_visualizer);
		}
		ImGui::End();
	}

	if (Show_VRAM_visualizer) {
		if (ImGui::Begin("VRAM Visualizer", &Show_VRAM_visualizer)) {
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

	if (Show_imgui_demo) {
		ImGui::ShowDemoWindow();
	}

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
}

bool imgui_overlay_has_focus()
{
	return ImGui::IsAnyItemFocused();
}