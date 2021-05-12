#include "ram_dump.h"

#include "memory.h"

imgui_ram_dump memory_dump_1;
imgui_ram_dump memory_dump_2;

void imgui_ram_dump::draw()
{
	if (ImGui::InputHexLabel<uint16_t, 16>("RAM Address", this->dump_address)) {
		reset_scroll = true;
	} else if (reset_dump_hex) {
		reset_dump_hex = false;
	}
	ImGui::SameLine();

	ImGui::InputHexLabel<uint8_t, 8>("RAM Bank", ram_bank);
	ImGui::SameLine();

	ImGui::InputHexLabel<uint8_t, 8>("ROM Bank", rom_bank);

	ImGui::BeginChild("ram dump", ImVec2(618.0, 399.0f));
	{
		parent::draw();
	}
	ImGui::EndChild();
}

void imgui_ram_dump::write_impl(uint16_t addr, uint8_t value)
{
	if (addr >= 0xc000) {
		debug_write6502(addr, rom_bank, value);
	} else {
		debug_write6502(addr, ram_bank, value);
	}
}

uint8_t imgui_ram_dump::read_impl(uint16_t addr)
{
	return (addr >= 0xc000) ? debug_read6502(addr, rom_bank) : debug_read6502(addr, ram_bank);
}
