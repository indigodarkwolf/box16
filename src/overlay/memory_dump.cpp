#include "memory_dump.h"

#include "imgui/imgui.h"

class memory_dump
{
public:
	void draw(float height);

private:
	void draw_header(const ImVec2 &size);
	void draw_editor(const ImVec2 &size);
	void draw_footer(const ImVec2 &size);

	struct memory_dump_settings
	{
		uint16_t cells_per_row;
		uint16_t bytes_per_cell;
	};
};

void memory_dump::draw(float height)
{
	const float width = ImGui::GetContentRegionAvail().x;

	float header_height = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
	float footer_height = ImGui::GetTextLineHeightWithSpacing() * 2.0f;

	if (height > header_height) {
		draw_header(ImVec2(width, header_height));
	}

	if (height - header_height > footer_height) {
		draw_editor(ImVec2(width, height - header_height - footer_height));
		draw_footer(ImVec2(width, footer_height));
	} else {
		draw_editor(ImVec2(width, height - header_height));
	}
}

void memory_dump::draw_header(const ImVec2 &size)
{
	// Start address
	// RAM bank
	// ROM bank
}

void memory_dump::draw_editor(const ImVec2 &size)
{
	
}

void memory_dump::draw_footer(const ImVec2 &size)
{

}
