#include "vram_dump.h"

#include "vera/vera_video.h"

void imgui_vram_dump::draw()
{
	if (ImGui::InputHexLabel<uint32_t, 20>("VRAM Address", this->dump_address)) {
		reset_scroll = true;
	} else if (reset_dump_hex) {
		reset_dump_hex = false;
	}

	ImGui::BeginChild("vram dump", ImVec2(637.0, 401.0f));
	{
		parent::draw();
	}
	ImGui::EndChild();
}

void imgui_vram_dump::write_impl(uint32_t address, uint8_t value)
{
	vera_video_space_write(address, value);
}

uint8_t imgui_vram_dump::read_impl(uint32_t address)
{
	return vera_video_space_read(address);
}
