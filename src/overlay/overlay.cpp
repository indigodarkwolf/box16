#include "overlay.h"

#include <SDL.h>

#include <functional>
#include <nfd.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

#include "disasm.h"
#include "ram_dump.h"
#include "util.h"
#include "vram_dump.h"

#include "cpu/fake6502.h"
#include "debugger.h"
#include "display.h"
#include "glue.h"
#include "joystick.h"
#include "keyboard.h"
#include "smc.h"
#include "symbols.h"
#include "timing.h"
#include "vera/sdcard.h"
#include "vera/vera_video.h"

bool Show_imgui_demo    = false;
bool Show_memory_dump_1 = false;
bool Show_memory_dump_2 = false;
bool Show_monitor       = false;
bool Show_VERA_monitor  = false;

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
			ImGui::Text(names[n]);
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
				ImGui::Text(label);
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

static void draw_breakpoints()
{
	ImGui::BeginGroup();
	{
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
		if (ImGui::TreeNodeEx("Breakpoints", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Columns(5);
			ImGui::SetColumnWidth(0, 27);
			ImGui::SetColumnWidth(1, 27);
			ImGui::SetColumnWidth(2, ImGui::CalcTextSize("Address  ").x);
			ImGui::SetColumnWidth(3, ImGui::CalcTextSize("Bank      ").x);

			ImGui::Dummy(ImVec2(10, 10));
			ImGui::NextColumn();

			ImGui::Dummy(ImVec2(10, 10));
			ImGui::NextColumn();

			ImGui::Text("Address");
			ImGui::NextColumn();

			ImGui::Text("Bank");
			ImGui::NextColumn();

			ImGui::Text("Symbol");
			ImGui::NextColumn();

			ImGui::Separator();

			const auto &breakpoints = debugger_get_breakpoints();
			for (auto [address, bank] : breakpoints) {
				if (ImGui::TileButton(ICON_REMOVE)) {
					debugger_remove_breakpoint(address, bank);
					break;
				}
				ImGui::NextColumn();

				if (debugger_breakpoint_is_active(address, bank)) {
					if (ImGui::TileButton(ICON_CHECKED)) {
						debugger_deactivate_breakpoint(address, bank);
					}
				} else {
					if (ImGui::TileButton(ICON_UNCHECKED)) {
						debugger_activate_breakpoint(address, bank);
					}
				}
				ImGui::NextColumn();

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

				ImGui::NextColumn();

				if (address < 0xa000) {
					ImGui::Text("--");
				} else {
					ImGui::Text("%s %02X", address < 0xc000 ? "RAM" : "ROM", bank);
				}
				ImGui::NextColumn();

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
				ImGui::NextColumn();
			}

			ImGui::Columns(1);
			ImGui::Separator();

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
						if (ImGui::Selectable(name.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
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
			ImGui::Columns(3);
			ImGui::SetColumnWidth(0, 27);
			ImGui::SetColumnWidth(1, 27);

			ImGui::Dummy(ImVec2(16, 16));
			ImGui::NextColumn();

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
			ImGui::NextColumn();

			ImGui::Text("Path");
			ImGui::NextColumn();

			ImGui::Separator();

			for (auto file : files) {
				ImGui::PushID(file.c_str());
				if (ImGui::TileButton(ICON_REMOVE)) {
					symbols_unload_file(file);
					ImGui::PopID();
					break;
				}
				ImGui::NextColumn();

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
				ImGui::NextColumn();

				ImGui::Text("%s", file.c_str());
				ImGui::NextColumn();
			}

			ImGui::Columns(1);
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

			if (ImGui::MenuItem("Save Options")) {
				save_options(true);
			}

			if (ImGui::MenuItem("Exit")) {
				SDL_Event evt;
				evt.type = SDL_QUIT;
				SDL_PushEvent(&evt);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Machine")) {
			if (ImGui::MenuItem("Reset", "Ctrl-R")) {
				machine_reset();
			}
			if (ImGui::MenuItem("NMI")) {
				nmi6502();
				debugger_interrupt();
			}
			if (ImGui::MenuItem("Save Dump", "Ctrl-S")) {
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
					if (NFD_OpenDialog("bin,img,sdcard", nullptr, &open_path) == NFD_OKAY && open_path != nullptr) {
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
			if (ImGui::BeginMenu("Debugging")) {
				ImGui::Checkbox("Memory Dump 1", &Show_memory_dump_1);
				ImGui::Checkbox("Memory Dump 2", &Show_memory_dump_2);
				ImGui::Checkbox("CPU Monitor", &Show_monitor);
				ImGui::Checkbox("VERA Monitor", &Show_VERA_monitor);
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::Checkbox("Show ImGui Demo", &Show_imgui_demo)) {
				// Nothing to do.
			}
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

	if (Show_memory_dump_1) {
		if (ImGui::Begin("Memory 1")) {
			memory_dump_1.draw();
		}
		ImGui::End();
	}

	if (Show_memory_dump_2) {
		if (ImGui::Begin("Memory 2")) {
			memory_dump_2.draw();
		}
		ImGui::End();
	}

	if (Show_monitor) {
		if (ImGui::Begin("CPU Monitor")) {
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

	if (Show_VERA_monitor) {
		if (ImGui::Begin("VERA Monitor")) {
			vram_dump.draw();
			ImGui::SameLine();
			draw_debugger_vera_status();
		}
		ImGui::End();
	}

	if (Show_imgui_demo) {
		ImGui::ShowDemoWindow();
	}
}

bool imgui_overlay_has_focus()
{
	return ImGui::IsAnyItemFocused();
}