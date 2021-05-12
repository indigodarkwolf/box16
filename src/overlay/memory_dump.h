#pragma once
#if !defined(MEMORY_DUMP_H)
#	define MEMORY_DUMP_H

#	include "imgui/imgui.h"
#	include "util.h"

template <class derived_t, uint32_t MEM_SIZE, typename address_t, uint8_t ADDRESS_BITS = sizeof(address_t) * 8>
class imgui_memory_dump
{
public:
	void set_dump_start(uint16_t addr)
	{
		dump_address     = addr;
		selected_address = addr;
		reset_dump_hex   = true;
		reset_scroll     = true;
	}

protected:
	void draw()
	{
		ImGui::BeginGroup();
		{
			constexpr int line_height = 19; // Don't know how to get height of a text box programmatically, so this is just ground truth.

			ImGuiListClipper clipper;
			clipper.Begin(MEM_SIZE / 16, line_height);

			while (clipper.Step()) {
				uint16_t start_addr = clipper.DisplayStart * 16;
				if (reset_scroll) {
					if (dump_address > MEM_SIZE - (20 * 16)) {
						dump_address = 0xFEB0;
					}
				} else if (clipper.DisplayEnd - clipper.DisplayStart >= 20) {
					if (start_addr < (dump_address & 0xfff0)) {
						dump_address   = start_addr;
						reset_dump_hex = true;
						reset_scroll   = true;
					} else if (start_addr > (dump_address | 0x000f)) {
						dump_address   = start_addr;
						reset_dump_hex = true;
						reset_scroll   = true;
					}
				}
				for (int y = clipper.DisplayStart; y < clipper.DisplayEnd; ++y) {
					uint16_t       line_addr = start_addr;
					const uint16_t line_stop = start_addr + 16;

					ImGui::Text(hex_formats[(ADDRESS_BITS >> 2) + 1], start_addr);

					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));
					ImGui::PushItemWidth(width_uint8);

					for (; start_addr < line_stop; ++start_addr) {
						ImGui::SameLine();
						if (start_addr % 8 == 0) {
							ImGui::Dummy(ImVec2(width_uint8 * 0.5f, 0.0f));
							ImGui::SameLine();
						}
						uint8_t mem = read(start_addr);

						if (start_addr == selected_address) {
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
						}
						if (ImGui::InputHex<uint8_t>(start_addr, mem)) {
							write(start_addr, mem);
						}
						if (start_addr == selected_address) {
							ImGui::PopStyleColor();
						}
						if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
							if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
								set_dump_start(start_addr);
							} else {
								selected_address = start_addr;
							}
						}
					}

					ImGui::SameLine();
					ImGui::Dummy(ImVec2(width_uint8 * 0.5f, 0));
					ImGui::SameLine();

					char line[17];
					for (int x = 0; x < 16; ++x) {
						uint8_t c = read(line_addr);
						line[x]   = isprint(c) ? c : '.';
						++line_addr;
					}
					line[16] = 0;
					ImGui::Text("%s", line);

					ImGui::PopItemWidth();
					ImGui::PopStyleVar();
				}
			}
			clipper.End();

			if (reset_scroll) {
				ImGui::SetScrollY((float)(((dump_address & ~0xf) >> 4) * 19));
				reset_scroll = false;
			} else {
				// If someone clicks and drags on the scrollbar, this is the only way we can
				// re-align the view correctly.
				const int scroll_y = (const int)ImGui::GetScrollY();
				const int offset   = scroll_y % 19;
				ImGui::SetScrollY((float)(scroll_y - offset));
			}
		}
		ImGui::EndGroup();
	}

private:
	void write(uint32_t address, uint8_t value)
	{
		static_cast<derived_t *>(this)->write_impl(address, value);
	}

	uint8_t read(uint32_t address)
	{
		return static_cast<derived_t *>(this)->read_impl(address);
	}

protected:
	bool      reset_scroll     = false;
	bool      reset_dump_hex   = false;
	address_t dump_address     = 0;
	address_t selected_address = 0;
};

#endif
