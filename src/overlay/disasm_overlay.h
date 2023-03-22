#pragma once
#if !defined(DISASM_OVERLAY_H)
#	define DISASM_OVERLAY_H

class imgui_debugger_disasm
{
private:
	uint16_t dump_start = 0;
	uint8_t  ram_bank   = 0;
	uint8_t  rom_bank   = 0;

	bool reset_input  = false;
	bool reset_scroll = false;
	bool following_pc = true;

	int follow_countdown = 3;

	bool show_hex      = true;
	int  memory_window = 1;

	uint8_t get_current_bank(uint16_t address);

public:
	bool get_hex_flag();
	int  get_memory_window();

	void set_dump_start(uint16_t addr);
	void set_ram_bank(uint8_t bank);
	void set_rom_bank(uint8_t bank);
	void follow_pc();

	void draw();

private:
	void imgui_disasm_line(uint16_t pc, uint8_t bank);
};

extern imgui_debugger_disasm disasm;

namespace ImGui
{
	void disasm_label(uint16_t target, uint8_t bank, bool branch_target, const char *hex_format);
	void disasm_label_wrap(uint16_t target, uint8_t bank, bool branch_target, const char *hex_format, const char *wrapper_format);
} // namespace ImGui

#endif // DISASM_OVERLAY_H
