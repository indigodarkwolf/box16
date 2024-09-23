#include "disasm_overlay.h"

#include "cpu/mnemonics.h"
#include "debugger.h"
#include "disasm.h"
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
	op_mode mode = mnemonics_mode[opcode];
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

/* ---------------------
*
* imgui_debugger_disasm
*
--------------------- */

uint8_t imgui_debugger_disasm::get_current_bank(uint16_t address)
{
	if (address >= 0xc000) {
		return rom_bank;
	} else if (address >= 0xa000) {
		return ram_bank;
	} else {
		return 0;
	}
}

bool imgui_debugger_disasm::get_hex_flag()
{
	return show_hex;
}

int imgui_debugger_disasm::get_memory_window()
{
	return memory_window;
}

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
	ImGui::BeginChild("disasm", ImVec2(397.0f, ImGui::GetContentRegionAvail().y));
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
			if (reset_input) {
				fmt::format_to_n(input_fields.disasm_address, std::size(input_fields.disasm_address), "{:04X}", dump_start);
			}
			if (ImGui::InputHexLabel("Disasm Address", input_fields.disasm_address)) {
				dump_start   = parse(input_fields.disasm_address);
				reset_scroll = true;
			}
		}
		ImGui::SameLine();

		{
			if (reset_input) {
				fmt::format_to_n(input_fields.ram_bank, std::size(input_fields.ram_bank), "{:04X}", ram_bank);
			}
			if (ImGui::InputHexLabel("  RAM Bank", input_fields.ram_bank)) {
				ram_bank = parse(input_fields.ram_bank);
			}
		}
		ImGui::SameLine();

		{
			if (reset_input) {
				fmt::format_to_n(input_fields.rom_bank, std::size(input_fields.rom_bank), "{:04X}", rom_bank);
			}
			if (ImGui::InputHexLabel("  ROM Bank", input_fields.rom_bank)) {
				rom_bank = parse(input_fields.rom_bank);
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

		ImGui::BeginChild("memory dump", ImVec2(382.0, ImGui::GetContentRegionAvail().y), false);
		{
			float line_height = ImGui::CalcTextSize("0xFFFF").y;

			ImGuiListClipper clipper;
			clipper.Begin(0x10000, line_height);

			while (clipper.Step()) {
				uint32_t addr  = clipper.DisplayStart;
				uint32_t lines = clipper.DisplayEnd - clipper.DisplayStart;

				if (reset_scroll) {
					// if (dump_address > 0x1FED0) {
					//	dump_address = 0x1FED0;
					// }
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
						const symbol_list_type &symbols = symbols_find(i, get_current_bank(i));
						for (auto &sym : symbols) {
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
							if (ImGui::FitSelectable(fmt::format("{:04X}", i), false, 0)) {
								set_dump_start(i);
							}
							ImGui::SameLine();
							ImGui::Text(" ");
							ImGui::SameLine();

							if (ImGui::SelectableFormat(sym, false, 0, ImVec2(0, line_height))) {
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
						if (ImGui::FitSelectable(fmt::format("{:04X}", addr), false, 0)) {
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

void imgui_debugger_disasm::imgui_disasm_line(uint16_t pc, uint8_t bank)
{
	const uint8_t opcode   = debug_read6502(pc, bank);
	char const   *mnemonic = mnemonics[opcode];

	const bool is_branch = disasm_is_branch(opcode);

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

	const op_mode mode = mnemonics_mode[opcode];
	switch (mode) {
		case op_mode::MODE_ZPREL: {
			uint8_t  zp     = debug_read6502(pc + 1, bank);
			uint16_t target = pc + 3 + (int8_t)debug_read6502(pc + 2, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line(8, zp, bank, false);

			ImGui::SameLine();
			ImGui::TextUnformatted(", ");
			ImGui::SameLine();

			ImGui::disasm_line(16, target, bank, is_branch);
		} break;

		case op_mode::MODE_IMP:
			ImGui::TextUnformatted(mnemonic);
			break;

		case op_mode::MODE_IMM: {
			uint16_t value = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			if (show_hex) {
				ImGui::TextFormat("#${:02X}", value);
			} else {
				ImGui::TextFormat("#{:d}", value);
			}
		} break;

		case op_mode::MODE_ZP: {
			uint8_t value = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line(16, value, bank, is_branch);
		} break;

		case op_mode::MODE_REL: {
			uint16_t target = pc + 2 + (int8_t)debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line(16, target, bank, is_branch);
		} break;

		case op_mode::MODE_ZPX: {
			uint8_t value = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(8, value, bank, is_branch, "{},x");
		} break;

		case op_mode::MODE_ZPY: {
			uint8_t value = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("%{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(8, value, bank, is_branch, "{},y");
		} break;

		case op_mode::MODE_ABSO: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line(16, target, bank, is_branch);
		} break;

		case op_mode::MODE_ABSX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(16, target, bank, is_branch, "{},x");
		} break;

		case op_mode::MODE_ABSY: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(16, target, bank, is_branch, "{},y");
		} break;

		case op_mode::MODE_AINX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(16, target, bank, is_branch, "({},x)");
		} break;

		case op_mode::MODE_INDY: {
			uint8_t target = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(8, target, bank, is_branch, "({}),y");
		} break;

		case op_mode::MODE_INDX: {
			uint8_t target = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(8, target, bank, is_branch, "({},x)");
		} break;

		case op_mode::MODE_IND: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(16, target, bank, is_branch, "({})");
		} break;

		case op_mode::MODE_IND0: {
			uint8_t target = debug_read6502(pc + 1, bank);

			ImGui::TextFormat("{} ", mnemonic);
			ImGui::SameLine();

			ImGui::disasm_line_wrap(8, target, bank, is_branch, "({})");
		} break;

		case op_mode::MODE_A:
			ImGui::TextFormat("{} a", mnemonic);
			break;
	}

	ImGui::PopStyleVar();
}

namespace ImGui
{
	void disasm_line(size_t bits, uint16_t target, uint8_t bank, bool branch_target)
	{
		auto inner = [=]() -> const std::string {
			if (const std::string &symbol = disasm_get_label(target, bank); !symbol.empty()) {
				return symbol;
			} else if (disasm.get_hex_flag()) {
				const size_t nybbles = (bits + 0b11) >> 2;
				return fmt::format("${:0{}x}", target, nybbles);
			} else {
				return fmt::format("{:d}", target);
			}
		};

		if (ImGui::FitSelectable(inner(), false, 0)) {
			if (branch_target) {
				disasm.set_dump_start(target);
			} else if (disasm.get_memory_window() == 1) {
				Show_memory_dump_1 = true;
				memory_dump_1.set_dump_start(target);
			} else {
				Show_memory_dump_2 = true;
				memory_dump_2.set_dump_start(target);
			}
		}
	}

	void disasm_line_wrap(size_t bits, uint16_t target, uint8_t bank, bool branch_target, const std::string &wrapper_format)
	{
		auto inner = [=]() -> const std::string {
			if (const std::string &symbol = disasm_get_label(target, bank); !symbol.empty()) {
				return symbol;
			} else if (disasm.get_hex_flag()) {
				const size_t nybbles = (bits + 0b11) >> 2;
				return fmt::format("${:0{}x}", target, nybbles);
			} else {
				return fmt::format("{:d}", target);
			}
		};

		std::string wrapped = fmt::format(fmt::runtime(wrapper_format), inner());

		if (ImGui::FitSelectable(wrapped, false, 0)) {
			if (branch_target) {
				disasm.set_dump_start(target);
			} else if (disasm.get_memory_window() == 1) {
				Show_memory_dump_1 = true;
				memory_dump_1.set_dump_start(target);
			} else {
				Show_memory_dump_2 = true;
				memory_dump_2.set_dump_start(target);
			}
		}
	}
} // namespace ImGui
