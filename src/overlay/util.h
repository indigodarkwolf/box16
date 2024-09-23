#pragma once
#if !defined(UTIL_H)
#	define UTIL_H

#include "imgui/imgui.h"

#include "memory.h"

static uint32_t parse(const std::string &str)
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
	for (char c : str) {
		result = (result << 4) + ascii_to_hex[c];
	}

	return result;
}
consteval float hex_width(int nybbles)
{
	return 7.0f * (nybbles + 1) + 2.0f;
}

constexpr const ImGuiInputTextFlags hex_flags     = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CtrlEnterForNewLine;

namespace ImGui
{
	template <typename T, size_t BITS = sizeof(T) * 8>
	bool InputHexLabel(const std::string& name, T& value)
	{
		constexpr const size_t ARRAY_SIZE = BITS / 4 + 1;
		char                   data[ARRAY_SIZE];

		fmt::format_to_n(data, ARRAY_SIZE - 1, "{:0{}x}", value, ARRAY_SIZE - 1);
		data[0] = '\0';

		TextUnformatted(name.c_str());
		SameLine();

		PushID(name.c_str());
		PushItemWidth(hex_width(ARRAY_SIZE - 1));
		bool result = InputText("##input", data, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();

		if (result) {
			value = static_cast<T>(parse(data));
		}
		return result;
	}

	template <size_t ARRAY_SIZE>
	bool InputHexLabel(const std::string &name, char (&str)[ARRAY_SIZE])
	{
		TextUnformatted(name.c_str());
		SameLine();
		PushID(name.c_str());
		PushItemWidth(hex_width(ARRAY_SIZE - 1));
		bool result = InputText("##input", str, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();
		return result;
	}

	template <typename T, size_t BITS = sizeof(T) * 8>
	bool InputHex(int id, T &value)
	{
		constexpr const size_t ARRAY_SIZE = BITS / 4 + 1;

		char str[ARRAY_SIZE];
		fmt::format_to_n(str, ARRAY_SIZE - 1, "{:0{}x}", (value & ((1ULL << BITS) - 1)), ARRAY_SIZE - 1);
		str[ARRAY_SIZE - 1] = '\0';

		PushID(id);
		PushItemWidth(hex_width(ARRAY_SIZE - 1));
		bool result = InputText("##input", str, ARRAY_SIZE, hex_flags);
		PopItemWidth();
		PopID();

		if (result) {
			value = (T)parse(str);
		}
		return result;
	}

	template <size_t ARRAY_SIZE, typename INDEX_TYPE>
	bool InputCombo(int id, char const *(&elements)[ARRAY_SIZE], INDEX_TYPE &selected)
	{
		ImGui::PushID(id);
		ImGui::PushItemWidth(hex_width(7));
		bool result = false;
		if (ImGui::BeginCombo("##input", elements[selected])) {
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
		ImGui::TextUnformatted(name);
		ImGui::SameLine();

		ImGui::PushID(name);
		ImGui::PushItemWidth(hex_width(7));
		bool result = false;
		if (ImGui::BeginCombo("##input", elements[selected])) {
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

	template<typename ...T>
	void TextFormat(const std::string& format, T... args)
	{
		ImGui::TextUnformatted(fmt::format(fmt::runtime(format), args...).c_str());
	}

	bool BeginComboFormat(const std::string &label, const std::string &preview_value, ImGuiComboFlags flags = 0);

	bool SelectableFormat(const std::string &label, bool selected, ImGuiSelectableFlags flags = 0, const ImVec2 &size_arg = ImVec2(0, 0));
	bool FitSelectable(const std::string &label, bool selected, ImGuiSelectableFlags flags = 0);
} // namespace ImGui

static uint16_t get_mem16(uint16_t address, uint8_t bank)
{
	return ((uint16_t)debug_read6502(address, bank)) | ((uint16_t)debug_read6502(address + 1, bank) << 8);
}

constexpr const float width_uint8  = 23.0f;
constexpr const float width_uint16 = 37.0f;
constexpr const float width_uint24 = 51.0f;

#endif
