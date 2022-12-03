#pragma once
#if !defined(UTIL_H)
#	define UTIL_H

#include "imgui/imgui.h"

#include "memory.h"

template <uint32_t BITS>
static uint32_t parse(char const *str)
{
	static constexpr uint8_t ascii_to_hex[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
		0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	uint32_t result = 0;
	uint32_t shift  = 0;
	while ((shift < BITS) && (*str != '\0')) {
		result = (result << 4) + ascii_to_hex[*str];
		++str;
		shift += 4;
	}
	return result;
}

constexpr const ImGuiInputTextFlags hex_flags     = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CtrlEnterForNewLine;
constexpr const float               hex_widths[]  = { 2.0f, 9.0f, 16.0f, 23.0f, 30.0f, 37.0f, 44.0f, 51.0f, 58.0f, 65.0f };
constexpr const char *              hex_formats[] = { "", "", "%01X", "%02X", "%03X", "%04X", "%05X", "%06X", "%07X", "%08X" };

namespace ImGui
{
	template <typename T, size_t BITS = sizeof(T) * 8>
	bool InputHexLabel(char const *name, T &value)
	{
		constexpr const size_t ARRAY_SIZE = BITS / 4 + 1;

		char str[ARRAY_SIZE];
		sprintf(str, hex_formats[ARRAY_SIZE], (int)value);
		Text("%s", name);
		SameLine();
		PushID(name);
		PushItemWidth(hex_widths[ARRAY_SIZE]);
		bool result = InputText("", str, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();
		if (result) {
			value = (T)parse<BITS>(str);
		}
		return result;
	}

	template <size_t ARRAY_SIZE>
	bool InputHexLabel(char const *name, char (&str)[ARRAY_SIZE])
	{
		Text("%s", name);
		SameLine();
		PushID(name);
		PushItemWidth(hex_widths[ARRAY_SIZE]);
		bool result = InputText("", str, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();
		return result;
	}

	template <typename T, size_t BITS = sizeof(T) * 8>
	bool InputHex(int id, T &value)
	{
		constexpr const size_t ARRAY_SIZE = BITS / 4 + 1;

		char str[ARRAY_SIZE];
		sprintf(str, hex_formats[ARRAY_SIZE], (int)(value & ((1ULL << BITS) - 1)));
		PushID(id);
		PushItemWidth(hex_widths[ARRAY_SIZE]);
		bool result = InputText("", str, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();
		if (result) {
			value = (T)parse<BITS>(str);
		}
		return result;
	}

	template <size_t ARRAY_SIZE>
	bool InputHex(int id, char (&str)[ARRAY_SIZE])
	{
		PushID(id);
		PushItemWidth(hex_widths[ARRAY_SIZE]);
		bool result = InputText("", str, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();
		return result;
	}

	template <size_t ARRAY_SIZE, typename INDEX_TYPE>
	bool InputCombo(int id, char const *(&elements)[ARRAY_SIZE], INDEX_TYPE &selected)
	{
		ImGui::PushID(id);
		ImGui::PushItemWidth(hex_widths[7]);
		bool result = false;
		if (ImGui::BeginCombo("", elements[selected])) {
			for (INDEX_TYPE i = 0; i < ARRAY_SIZE; ++i) {
				if (ImGui::Selectable(elements[i], (selected == i))) {
					selected = i;
					result   = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::PopID();
		return result;
	}

	template <size_t ARRAY_SIZE, typename INDEX_TYPE>
	bool InputCombo(char const *name, char const *(&elements)[ARRAY_SIZE], INDEX_TYPE &selected) 
	{
		ImGui::Text("%s", name);
		ImGui::SameLine();

		ImGui::PushID(name);
		ImGui::PushItemWidth(hex_widths[7]);
		bool result = false;
		if (ImGui::BeginCombo("", elements[selected])) {
			for (INDEX_TYPE i = 0; i < ARRAY_SIZE; ++i) {
				if (ImGui::Selectable(elements[i], (selected == i))) {
					selected = i;
					result   = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::PopID();
		return result;
	}

	void VERAColorTooltip(const char *text, const float *col, ImGuiColorEditFlags flags = 0);
	bool VERAColorButton(const char *desc_id, const ImVec4 &col, ImGuiColorEditFlags flags = 0, const ImVec2 &size = ImVec2(0, 0));
	bool VERAColorPicker3(const char *label, float col[3], ImGuiColorEditFlags flags = 0);
	bool VERAColorPicker4(const char *label, float col[4], ImGuiColorEditFlags flags = 0, const float *ref_col = NULL);
	bool VERAColorEdit3(const char *label, float col[3], ImGuiColorEditFlags flags = 0);
	bool VERAColorEdit4(const char *label, float col[4], ImGuiColorEditFlags flags = 0);
} // namespace ImGui

static uint16_t get_mem16(uint16_t address, uint8_t bank)
{
	return ((uint16_t)debug_read6502(address, bank)) | ((uint16_t)debug_read6502(address + 1, bank) << 8);
}

constexpr const float width_uint8  = 23.0f;
constexpr const float width_uint16 = 37.0f;
constexpr const float width_uint24 = 51.0f;

#endif
