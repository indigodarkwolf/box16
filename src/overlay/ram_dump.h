#pragma once
#if !defined(RAM_DUMP_H)
#	define RAM_DUMP_H

#include "memory_dump.h"

class imgui_ram_dump : public imgui_memory_dump<imgui_ram_dump, 0x10000, uint16_t>
{
public:
	void draw();

	void write_impl(uint16_t addr, uint8_t value);
	uint8_t read_impl(uint16_t addr);

private:
	uint8_t ram_bank = 0;
	uint8_t rom_bank = 0;
	using parent = imgui_memory_dump<imgui_ram_dump, 0x10000, uint16_t>;
};

extern imgui_ram_dump memory_dump_1;
extern imgui_ram_dump memory_dump_2;

#endif
