#include "options_menu.h"

#include "display.h"
#include "imgui/imgui.h"
#include "nfd.h"
#include "options.h"
#include "ym2151/ym2151.h"

void draw_options_menu()
{
	if (ImGui::Button("Save to box16.ini")) {
		save_options(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load from box16.ini")) {
		load_options();
	}

	auto file_option = [](char const *ext, char(&path)[PATH_MAX], char const *name, char const *tip) {
		bool result = false;
		ImGui::PushID(name);
		ImGui::BeginGroup();
		if (ImGui::Button("...")) {
			char *open_path = nullptr;
			if (NFD_OpenDialog(ext, nullptr, &open_path) == NFD_OKAY && open_path != nullptr) {
				strcpy(path, open_path);
				result = true;
			}
		}
		ImGui::SameLine();
		ImGui::InputText(name, path, PATH_MAX);
		ImGui::EndGroup();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tip);
		}
		ImGui::PopID();
		return result;
	};

	auto bool_option = [](bool &option, char const *name, char const *tip) {
		bool result = ImGui::Checkbox(name, &option);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tip);
		}
		return result;
	};

	//===============================
	// System Paths
	//
	//-------------------------------

	ImGui::TextDisabled("System Paths");
	ImGui::Separator();
	ImGui::BeginGroup();
	{
		if (ImGui::Button("...")) {
			char *open_path = nullptr;
			if (NFD_PickFolder(Options.hyper_path, &open_path) == NFD_OKAY && open_path != nullptr) {
				strcpy(Options.hyper_path, open_path);
			} else if (NFD_PickFolder("", &open_path) == NFD_OKAY && open_path != nullptr) {
				strcpy(Options.hyper_path, open_path);
			}
		}
		ImGui::SameLine();
		ImGui::InputText("Hypercall Path", Options.hyper_path, PATH_MAX);
	}
	ImGui::EndGroup();
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("When attempting to LOAD or SAVE files without an SD card inserted, this is the root directory.\nCommand line: -hypercall_path <path>");
	}

	file_option("bin", Options.rom_path, "ROM path", "Location of the emulator ROM file.\nCommand line: -rom <path>");
	file_option("bin;nvram", Options.nvram_path, "NVRAM path", "Location of NVRAM image file, if any.\nCommand line: -nvram <path>");
	file_option("bin;img;sdcard", Options.sdcard_path, "SD Card path", "Location of SD card image file, if any.\nCommand line: -sdcard <path>");

	ImGui::NewLine();

	//===============================
	// Boot Options
	//
	//-------------------------------

	ImGui::TextDisabled("Boot Options");
	ImGui::Separator();
	file_option("prg", Options.prg_path, "PRG path", "PRG file to LOAD after boot, if any.\nCommand line: -prg <path>");
	file_option("bas", Options.bas_path, "BAS path", "Text BAS file to automatically type into the console after boot, if any.\nCommand line: -bas <path>");

	bool_option(Options.run_after_load, "Run after load", "If a PRG or BAS file is set to be loaded, run it immediately.\nCommand line: -run");
	bool_option(Options.run_geos, "Run GEOS", "Run GEOS after boot.\nCommand line: -geos");
	bool_option(Options.run_test, "Run tests", "Run tests after boot.\nCommand line: -test");
	ImGui::InputInt("Test ID", &Options.test_number);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Test ID to run, if any.");
	}

	static constexpr const char *keymaps[] = {
		"en-us",
		"en-gb",
		"de",
		"nordic",
		"it",
		"pl",
		"hu",
		"es",
		"fr",
		"de-ch",
		"fr-be",
		"pt-br",
	};

	if (ImGui::BeginCombo("Keymap", keymaps[Options.keymap])) {
		for (uint8_t i = 0; i < sizeof(keymaps) / sizeof(*keymaps); i++) {
			if (ImGui::Selectable(keymaps[i], Options.keymap == i)) {
				Options.keymap = i;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Keymap assumed by the kernal.\nCommand line: -keymap <map>");
	}

	ImGui::NewLine();

	//===============================
	// Logging and Exit Dumps
	//
	//-------------------------------

	ImGui::TextDisabled("Logging and Exit Dumps");
	ImGui::Separator();
	bool_option(Options.log_keyboard, "Log Keyboard", "Log keyboard activity.\nCommand line: -log k");
	bool_option(Options.log_speed, "Log Speed", "Log speed periodically.\nCommand line: -log s");
	bool_option(Options.log_video, "Log Video", "Log video memory activity.\nCommand line: -log v");

	bool_option(Options.dump_cpu, "Dump CPU", "Machine dumps should include CPU status.\nCommand line: -dump c");
	bool_option(Options.dump_ram, "Dump RAM", "Machine dumps should include low RAM.\nCommand line: -dump r");
	bool_option(Options.dump_bank, "Dump banks", "Machine dumps should include hi RAM banks.\nCommand line: -dump b");
	bool_option(Options.dump_vram, "Dump VRAM", "Machine dumps should include VRAM.\nCommand line: -dump v");

	static char const *echo_mode_labels[] = {
		"None",
		"Raw",
		"Cooked",
		"ISO",
	};
	if (ImGui::BeginCombo("Echo Mode", echo_mode_labels[Options.echo_mode])) {
		for (int i = 0; i < 4; ++i) {
			ImGui::Selectable(echo_mode_labels[i], Options.echo_mode == i);
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Format of console text to echoed to output.\nCommand line: -echo {raw|iso|cooked|none}");
	}

	ImGui::NewLine();

	//===============================
	// Machine Options
	//
	//-------------------------------

	ImGui::TextDisabled("Machine Options");
	ImGui::Separator();

	if (ImGui::InputPow2("Himem KBs", &Options.num_ram_banks, "%d")) {
		if (Options.num_ram_banks < 8) {
			Options.num_ram_banks = 8;
		}
		if (Options.num_ram_banks > 2048) {
			Options.num_ram_banks = 2048;
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("KBs of bankable Hi RAM (8-2048, in powers of 2)\nCommand line: -ram <qty>");
	}

	ImGui::Checkbox("Set RTC", &Options.set_system_time);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Set X16 system time to current time reported by your OS.\nCommand line: -rtc");
	}

	bool warp_speed = Options.warp_factor;
	if (ImGui::Checkbox("Warp Speed", &warp_speed)) {
		Options.warp_factor = warp_speed ? 1 : 0;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Toggle warp speed. (VERA will skip most frames, speed cap is removed.)\nCommand line: -warp");
	}

	ImGui::NewLine();

	//===============================
	// Misc. Options
	//
	//-------------------------------

	ImGui::TextDisabled("Misc. Options");
	ImGui::Separator();

	if (ImGui::InputInt("Window Scale", &Options.window_scale)) {
		if (Options.window_scale < 1) {
			Options.window_scale = 1;
		}
		if (Options.window_scale > 4) {
			Options.window_scale = 4;
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Set window scale (1x-4x) on emulator start.\nCommand line: -scale {1|2|3|4}");
	}

	static auto quality_name = [](scale_quality_t quality) {
		switch (quality) {
			case scale_quality_t::NEAREST: return "Nearest";
			case scale_quality_t::LINEAR: return "Linear";
			case scale_quality_t::BEST: return "Best";
			default: return "Nearest";
		}
	};

	if (ImGui::BeginCombo("Scale Quality", quality_name(Options.scale_quality))) {
		auto selection = [](scale_quality_t quality) {
			if (ImGui::Selectable(quality_name(quality), Options.scale_quality == quality)) {
				Options.scale_quality = quality;
			}
		};

		selection(scale_quality_t::NEAREST);
		selection(scale_quality_t::LINEAR);
		selection(scale_quality_t::BEST);

		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Set scaling quality:\nNearest: Scale by nearest pixel.\nLinear: Scale by linearly averaging between pixels.\nBest: Scale by anisotropic filtering.\nCommand line: -quality {nearest|linear|best}");
	}

	file_option("gif", Options.gif_path, "GIF path", "Location to save gifs\nCommand line: -gif <path>[,wait]");
	file_option("wav", Options.wav_path, "WAV path", "Location to save wavs\nCommand line: -wav <path>[,wait]");
	bool_option(Options.load_standard_symbols, "Load Standard Symbols", "Load all symbols files typically included with ROM distributions.\nCommand line: -stds");

	bool_option(Options.no_keybinds, "No Keybinds", "Disable all emulator keyboard bindings.\nDoes not affect F12 (emulator debug break) or key shortcuts when the ASM Monitor is open.\nCommand line: -nobinds");

	ImGui::NewLine();

	//===============================
	// Audio
	//
	//-------------------------------

	ImGui::TextDisabled("Audio");
	ImGui::Separator();

	ImGui::InputText("Audio Device Name", Options.audio_dev_name, PATH_MAX);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Name of default audio device to use.\nCommand line: -sound <device>");
	}

	if (ImGui::Checkbox("No Sound", &Options.no_sound)) {

	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Disable audio subsystems entirely.\nCommand line: -nosound");
	}

	ImGui::InputInt("Audio Buffers", &Options.audio_buffers);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Number of audio buffers.\n(Deprecated: No longer has any effect.)\nCommand line: -abufs <qty>");
	}

	if (bool_option(Options.ym_irq, "Enable YM2151 interrupts", "Enable interrupt generation from the YM2151 chip.\nCommand line: -ymirq")) {
		YM_set_irq_enabled(Options.ym_irq);
	}

	if (bool_option(Options.ym_strict, "Enable strict YM behaviors", "Enforce strict limitations in the YM2151. This is hardware accurate, but the official emulator is less strict.\nCommand line: -ymstrict")) {
		YM_set_strict_busy(Options.ym_strict);
	}
}
