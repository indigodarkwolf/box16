#include "disasm.h"

#include "cpu/mnemonics.h"
#include "debugger.h"
#include "display.h"
#include "glue.h"
#include "imgui/imgui.h"
#include "overlay.h"
#include "ram_dump.h"
#include "symbols.h"
#include "util.h"

imgui_debugger_disasm disasm;

/* ---------------------
* 
* Helpers              
* 
--------------------- */

static int disasm_len(uint16_t pc, uint8_t bank)
{
	uint8_t opcode = debug_read6502(pc, bank);

	// BRK is a two-byte instruction.
	if (opcode == 0x00) {
		return 2;
	}
	op_mode mode   = mnemonics_mode[opcode];
	switch (mode) {
		case op_mode::MODE_A:
		case op_mode::MODE_IMP:
			return 1;
		case op_mode::MODE_IMM:
		case op_mode::MODE_ZP:
		case op_mode::MODE_REL:
		case op_mode::MODE_ZPX:
		case op_mode::MODE_ZPY:
		case op_mode::MODE_INDY:
		case op_mode::MODE_INDX:
		case op_mode::MODE_IND0:
			return 2;
		case op_mode::MODE_ZPREL:
		case op_mode::MODE_ABSO:
		case op_mode::MODE_ABSX:
		case op_mode::MODE_ABSY:
		case op_mode::MODE_AINX:
		case op_mode::MODE_IND:
			return 3;
	}
	return 1;
}

static char const *get_disasm_label(uint16_t address)
{
	static char label[256];

	const symbol_list_type &symbols = symbols_find(address);
	if (symbols.size() > 0) {
		strncpy(label, symbols.front().c_str(), 256);
		label[255] = '\0';
		return label;
	}

	for (uint16_t i = 1; i < 3; ++i) {
		const symbol_list_type &symbols = symbols_find(address - i);
		if (symbols.size() > 0) {
			snprintf(label, 256, "%s+%d", symbols.front().c_str(), i);
			label[255] = '\0';
			return label;
		}
	}

	return nullptr;
}

/* ---------------------
* 
* imgui_debugger_disasm              
* 
--------------------- */

void imgui_debugger_disasm::set_dump_start(uint16_t addr)
{
	dump_start   = addr;
	reset_input  = true;
	reset_scroll = true;
	following_pc = false;
}

void imgui_debugger_disasm::set_ram_bank(uint8_t bank)
{
	ram_bank = bank;
}

void imgui_debugger_disasm::set_rom_bank(uint8_t bank)
{
	rom_bank = bank;
}

void imgui_debugger_disasm::follow_pc()
{
	following_pc     = true;
	follow_countdown = 3;
}

void imgui_debugger_disasm::draw()
{
	ImGui::BeginChild("disasm", ImVec2(397.0f, 518.0f));
	{
		bool paused = debugger_is_paused();

		if (following_pc) {
			follow_countdown = (follow_countdown > 0) ? follow_countdown - 1 : 0;
			if (!paused) {
				follow_countdown = 3;
			} else if (follow_countdown > 0) {
				set_dump_start(state6502.pc - waiting);
				ram_bank = memory_get_ram_bank();
				rom_bank = memory_get_rom_bank();
			}
		}

		if (ImGui::TileButton(ICON_RETURN_TO_PC, paused)) {
			follow_pc();
		}
		ImGui::SameLine();

		{
			static char hex[5] = { "0000" };
			if (reset_input) {
				sprintf(hex, "%04X", dump_start);
			}
			if (ImGui::InputHexLabel("Disasm Address", hex)) {
				dump_start   = parse<16>(hex);
				reset_scroll = true;
			}
		}
		ImGui::SameLine();

		{
			static char ram_bank_hex[3] = "00";
			if (reset_input) {
				sprintf(ram_bank_hex, "%02X", ram_bank);
			}
			if (ImGui::InputHexLabel("  RAM Bank", ram_bank_hex)) {
				ram_bank = parse<8>(ram_bank_hex);
			}
		}
		ImGui::SameLine();

		{
			static char rom_bank_hex[3] = "00";
			if (reset_input) {
				sprintf(rom_bank_hex, "%02X", rom_bank);
			}
			if (ImGui::InputHexLabel("  ROM Bank", rom_bank_hex)) {
				rom_bank = parse<8>(rom_bank_hex);
			}
		}

		reset_input = false;

		ImGui::Checkbox("Show Hex", &show_hex);
		ImGui::SameLine();
		if (ImGui::RadioButton("Memory 1", memory_window == 1)) {
			memory_window = 1;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Memory 2", memory_window == 2)) {
			memory_window = 2;
		}

		ImGui::Separator();

		ImGui::BeginChild("memory dump", ImVec2(382.0, 458.0f));
		{
			float line_height = ImGui::CalcTextSize("0xFFFF").y;

			ImGuiListClipper clipper;
			clipper.Begin(0x10000, line_height);

			while (clipper.Step()) {
				uint32_t addr  = clipper.DisplayStart;
				uint32_t lines = clipper.DisplayEnd - clipper.DisplayStart;

				if (reset_scroll) {
					//if (dump_address > 0x1FED0) {
					//	dump_address = 0x1FED0;
					//}
				} else if (clipper.DisplayEnd - clipper.DisplayStart >= 28) {
					if (addr != dump_start) {
						dump_start   = addr;
						reset_input  = true;
						reset_scroll = true;
					}
				}
				for (uint32_t y = 0; y < lines; ++y) {
					ImGui::PushID(y);
					int len = disasm_len(addr, addr < 0xc000 ? ram_bank : rom_bank);

					bool found_symbols = false;

					if (state6502.pc - waiting == addr) {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
					}

					for (uint32_t i = addr; i < addr + len; ++i) {
						const symbol_list_type &symbols = symbols_find(i, memory_get_current_bank(i));
						for (auto &sym : symbols) {
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
							char addr_text[5];
							sprintf(addr_text, "%04X", i);
							if (ImGui::Selectable(addr_text, false, 0, ImGui::CalcTextSize(addr_text))) {
								set_dump_start(i);
							}
							ImGui::SameLine();
							ImGui::Text(" ");
							ImGui::SameLine();

							if (ImGui::Selectable(sym.c_str(), false, 0, ImVec2(0, line_height))) {
								set_dump_start(i);
							}
							ImGui::PopStyleVar();

							found_symbols = true;
							++y;
							if (y >= lines) {
								break;
							}
						}
					}

					if (y >= lines) {
						if (state6502.pc - waiting == addr) {
							ImGui::PopStyleColor();
						}
						ImGui::PopID();
						break;
					}

					if (!found_symbols) {
						char addr_text[5];
						sprintf(addr_text, "%04X", addr);
						if (ImGui::Selectable(addr_text, false, 0, ImGui::CalcTextSize(addr_text))) {
							set_dump_start(addr);
						}
						ImGui::SameLine();
						ImGui::Dummy(ImVec2(8.0f, 16.0f));
					} else {
						ImGui::Dummy(ImVec2(44.0f, 16.0f));
					}
					ImGui::SameLine();

					imgui_disasm_line(addr, addr < 0xc000 ? ram_bank : rom_bank);

					if (state6502.pc - waiting == addr) {
						ImGui::PopStyleColor();
					}
					ImGui::PopID();

					addr += len;

					if (addr >= 0x10000) {
						break;
					}
				}
			}
			clipper.End();

			if (reset_scroll) {
				ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + line_height * (dump_start), 0.0f);
				reset_scroll = false;
			}
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
}

void imgui_debugger_disasm::imgui_label(uint16_t target, bool branch_target, const char *hex_format)
{
	const char *symbol = get_disasm_label(target);

	char inner[256];
	if (symbol != nullptr) {
		snprintf(inner, 256, "%s", symbol);
	} else if (show_hex) {
		snprintf(inner, 256, hex_format, target);
	} else {
		snprintf(inner, 256, "%d", (int)target);
	}
	inner[255] = '\0';

	if (ImGui::Selectable(inner, false, 0, ImGui::CalcTextSize(inner))) {
		if (branch_target) {
			set_dump_start(target);
		} else if (memory_window == 1) {
			Show_memory_dump_1 = true;
			memory_dump_1.set_dump_start(target);
		} else {
			Show_memory_dump_2 = true;
			memory_dump_2.set_dump_start(target);
		}
	}
}

void imgui_debugger_disasm::imgui_label_wrap(uint16_t target, bool branch_target, const char *hex_format, const char *wrapper_format)
{
	const char *symbol = get_disasm_label(target);

	char inner[256];
	if (symbol != nullptr) {
		snprintf(inner, 256, "%s", symbol);
	} else if (show_hex) {
		snprintf(inner, 256, hex_format, target);
	} else {
		snprintf(inner, 256, "%d", (int)target);
	}
	inner[255] = '\0';

	char wrapped[256];
	snprintf(wrapped, 256, wrapper_format, inner);
	wrapped[255] = '\0';

	if (ImGui::Selectable(wrapped, false, 0, ImGui::CalcTextSize(wrapped))) {
		if (branch_target) {
			set_dump_start(target);
		} else if (memory_window == 1) {
			Show_memory_dump_1 = true;
			memory_dump_1.set_dump_start(target);
		} else {
			Show_memory_dump_2 = true;
			memory_dump_2.set_dump_start(target);
		}
	}
}

void imgui_debugger_disasm::imgui_disasm_line(uint16_t pc, /*char *line, unsigned int max_line,*/ uint8_t bank)
{
	uint8_t     opcode   = debug_read6502(pc, bank);
	char const *mnemonic = mnemonics[opcode];
	op_mode     mode     = mnemonics_mode[opcode];

	//		Test for branches. These are BRA ($80) and
	//		$10,$30,$50,$70,$90,$B0,$D0,$F0.
	//
	bool is_branch = (opcode == 0x80) || ((opcode & 0x1F) == 0x10) || (opcode == 0x20);

	//		Ditto bbr and bbs, the "zero-page, relative" ops.
	//		$0F,$1F,$2F,$3F,$4F,$5F,$6F,$7F,$8F,$9F,$AF,$BF,$CF,$DF,$EF,$FF
	//
	bool is_zprel = (opcode & 0x0F) == 0x0F;

	is_branch = is_branch || is_zprel;

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	const ImVec2 cursor = ImGui::GetCursorPos();
	ImGui::Tile(ICON_ADD_BREAKPOINT_DISABLED);
	const bool has_breakpoint     = debugger_has_breakpoint(pc, memory_get_current_bank(pc));
	const bool breakpoint_enabled = debugger_breakpoint_is_active(pc, memory_get_current_bank(pc));
	if (breakpoint_enabled) {
		if (ImGui::IsItemClicked()) {
			debugger_deactivate_breakpoint(pc, memory_get_current_bank(pc));
		} else {
			ImGui::SetCursorPos(cursor);
			ImGui::Tile(ICON_CHECKED);
		}
	} else if (has_breakpoint) {
		if (ImGui::IsItemClicked()) {
			debugger_remove_breakpoint(pc, memory_get_current_bank(pc));
		} else {
			ImGui::SetCursorPos(cursor);
			ImGui::Tile(ICON_UNCHECKED);
		}
	} else {
		if (ImGui::IsItemClicked()) {
			debugger_add_breakpoint(pc, memory_get_current_bank(pc));
		} else if (ImGui::IsItemHovered()) {
			ImGui::SetCursorPos(cursor);
			ImGui::Tile(ICON_ADD_BREAKPOINT);
		}
	}
	ImGui::SameLine();
	ImGui::Dummy(ImVec2(4.0f, 16.0f));
	ImGui::SameLine();

    switch (mode)
	{
		case op_mode::MODE_ZPREL: {
			uint8_t  zp     = debug_read6502(pc + 1, bank);
			uint16_t target = pc + 3 + (int8_t)debug_read6502(pc + 2, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label(zp, is_branch, "$%02X");

			ImGui::SameLine();
			ImGui::Text(", ");
			ImGui::SameLine();

			imgui_label(target, is_branch, "$%04X");
		} break;

		case op_mode::MODE_IMP:
			ImGui::Text("%s", mnemonic);
			break;

		case op_mode::MODE_IMM: {
			uint16_t value = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			if (show_hex) {
				ImGui::Text("#$%02X", value);
			} else {
				ImGui::Text("#%d", (int)value);
			}
		} break;

		case op_mode::MODE_ZP: {
			uint8_t value = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			if (show_hex) {
				ImGui::Text("$%02X", value);
			} else {
				ImGui::Text("%d", (int)value);
			}
		} break;

		case op_mode::MODE_REL: {
			uint16_t target = pc + 2 + (int8_t)debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label(target, is_branch, "$%04X");
		} break;

		case op_mode::MODE_ZPX: {
			uint8_t value = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(value, is_branch, "$%02X", "%s,x");
		} break;

		case op_mode::MODE_ZPY: {
			uint8_t value = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(value, is_branch, "$%02X", "%s,y");
		} break;

		case op_mode::MODE_ABSO: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label(target, is_branch, "$%04X");
		} break;

		case op_mode::MODE_ABSX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%04X", "%s,x");
		} break;

		case op_mode::MODE_ABSY: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%04X", "%s,y");
		} break;

		case op_mode::MODE_AINX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%04X", "(%s,x)");
		} break;

		case op_mode::MODE_INDY: {
			uint8_t target = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%02X", "(%s),y");
		} break;

		case op_mode::MODE_INDX: {
			uint8_t target = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%02X", "(%s,x)");
		} break;

		case op_mode::MODE_IND: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%04X", "(%s)");
		} break;

		case op_mode::MODE_IND0: {
			uint8_t target = debug_read6502(pc + 1, bank);

			ImGui::Text("%s ", mnemonic);
			ImGui::SameLine();

			imgui_label_wrap(target, is_branch, "$%02X", "(%s)");
		} break;

		case op_mode::MODE_A:
			ImGui::Text("%s a", mnemonic);
			break;
	}

	ImGui::PopStyleVar();
}
