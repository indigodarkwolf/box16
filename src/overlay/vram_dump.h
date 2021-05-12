#pragma once
#if !defined(VRAM_DUMP_H)
#	define VRAM_DUMP_H

#include "memory_dump.h"

#include "vera/vera_video.h"

class imgui_vram_dump : public imgui_memory_dump<imgui_vram_dump, 0x20000, uint32_t, 20>
{
public:
	void draw();

	void write_impl(uint32_t address, uint8_t value);
	uint8_t read_impl(uint32_t address);

private:
	using parent = imgui_memory_dump<imgui_vram_dump, 0x20000, uint32_t, 20>;
};

#endif
