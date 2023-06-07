#include "util.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#	define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// ColorEdit supports RGB and HSV inputs. In case of RGB input resulting color may have undefined hue and/or saturation.
// Since widget displays both RGB and HSV values we must preserve hue and saturation to prevent these values resetting.
static void ColorEditRestoreHS(const float *col, float *H, float *S, float *V)
{
	// This check is optional. Suppose we have two color widgets side by side, both widgets display different colors, but both colors have hue and/or saturation undefined.
	// With color check: hue/saturation is preserved in one widget. Editing color in one widget would reset hue/saturation in another one.
	// Without color check: common hue/saturation would be displayed in all widgets that have hue/saturation undefined.
	// g.ColorEditSavedColor is stored as ImU32 RGB value: this essentially gives us color equality check with reduced precision.
	// Tiny external color changes would not be detected and this check would still pass. This is OK, since we only restore hue/saturation _only_ if they are undefined,
	// therefore this change flipping hue/saturation from undefined to a very tiny value would still be represented in color picker.
	ImGuiContext &g = *GImGui;
	if (g.ColorEditSavedColor != ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0)))
		return;

	// When S == 0, H is undefined.
	// When H == 1 it wraps around to 0.
	if (*S == 0.0f || (*H == 0.0f && g.ColorEditSavedHue == 1))
		*H = g.ColorEditSavedHue;

	// When V == 0, S is undefined.
	if (*V == 0.0f)
		*S = g.ColorEditSavedSat;
}

// Helper for ColorPicker4()
static void RenderArrowsForVerticalBar(ImDrawList *draw_list, ImVec2 pos, ImVec2 half_sz, float bar_w, float alpha)
{
	ImU32 alpha8 = IM_F32_TO_INT8_SAT(alpha);
	ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + half_sz.x + 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Right, IM_COL32(0, 0, 0, alpha8));
	ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + half_sz.x, pos.y), half_sz, ImGuiDir_Right, IM_COL32(255, 255, 255, alpha8));
	ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + bar_w - half_sz.x - 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Left, IM_COL32(0, 0, 0, alpha8));
	ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + bar_w - half_sz.x, pos.y), half_sz, ImGuiDir_Left, IM_COL32(255, 255, 255, alpha8));
}

namespace ImGui
{
	// Note: only access 3 floats if ImGuiColorEditFlags_NoAlpha flag is set.
	void VERAColorTooltip(const char *text, const float *col, ImGuiColorEditFlags flags)
	{
		ImGuiContext &g = *GImGui;

		BeginTooltipEx(ImGuiTooltipFlags_OverridePreviousTooltip, ImGuiWindowFlags_None);
		const char *text_end = text ? FindRenderedTextEnd(text, NULL) : text;
		if (text_end > text) {
			TextEx(text, text_end);
			Separator();
		}

		ImVec2 sz(g.FontSize * 3 + g.Style.FramePadding.y * 2, g.FontSize * 3 + g.Style.FramePadding.y * 2);
		ImVec4 cf(col[0], col[1], col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
		int    cr = IM_F32_TO_INT8_SAT(col[0]) >> 4, cg = IM_F32_TO_INT8_SAT(col[1]) >> 4, cb = IM_F32_TO_INT8_SAT(col[2]) >> 4, ca = (flags & ImGuiColorEditFlags_NoAlpha) ? 255 : IM_F32_TO_INT8_SAT(col[3]) >> 4;
		VERAColorButton("##preview", cf, (flags & (ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf)) | ImGuiColorEditFlags_NoTooltip, sz);
		SameLine();
		if ((flags & ImGuiColorEditFlags_InputRGB) || !(flags & ImGuiColorEditFlags_InputMask_)) {
			if (flags & ImGuiColorEditFlags_NoAlpha)
				Text("#%01X%01X%01X\nR: %d, G: %d, B: %d\n(%.3f, %.3f, %.3f)", cr, cg, cb, cr, cg, cb, col[0], col[1], col[2]);
			else
				Text("#%01X%01X%01X%01X\nR:%d, G:%d, B:%d, A:%d\n(%.3f, %.3f, %.3f, %.3f)", cr, cg, cb, ca, cr, cg, cb, ca, col[0], col[1], col[2], col[3]);
		} else if (flags & ImGuiColorEditFlags_InputHSV) {
			if (flags & ImGuiColorEditFlags_NoAlpha)
				Text("H: %.3f, S: %.3f, V: %.3f", col[0], col[1], col[2]);
			else
				Text("H: %.3f, S: %.3f, V: %.3f, A: %.3f", col[0], col[1], col[2], col[3]);
		}
		EndTooltip();
	}

	// A little color square. Return true when clicked.
	// FIXME: May want to display/ignore the alpha component in the color display? Yet show it in the tooltip.
	// 'desc_id' is not called 'label' because we don't display it next to the button, but only in the tooltip.
	// Note that 'col' may be encoded in HSV if ImGuiColorEditFlags_InputHSV is set.
	bool VERAColorButton(const char *desc_id, const ImVec4 &col, ImGuiColorEditFlags flags, const ImVec2 &size_arg)
	{
		ImGuiWindow *window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext &g            = *GImGui;
		const ImGuiID id           = window->GetID(desc_id);
		const float   default_size = GetFrameHeight();
		const ImVec2  size(size_arg.x == 0.0f ? default_size : size_arg.x, size_arg.y == 0.0f ? default_size : size_arg.y);
		const ImRect  bb(window->DC.CursorPos, window->DC.CursorPos + size);
		ItemSize(bb, (size.y >= default_size) ? g.Style.FramePadding.y : 0.0f);
		if (!ItemAdd(bb, id))
			return false;

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);

		if (flags & ImGuiColorEditFlags_NoAlpha)
			flags &= ~(ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf);

		ImVec4 col_rgb = col;
		if (flags & ImGuiColorEditFlags_InputHSV)
			ColorConvertHSVtoRGB(col_rgb.x, col_rgb.y, col_rgb.z, col_rgb.x, col_rgb.y, col_rgb.z);

		ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);
		float  grid_step = ImMin(size.x, size.y) / 2.99f;
		float  rounding  = ImMin(g.Style.FrameRounding, grid_step * 0.5f);
		ImRect bb_inner  = bb;
		float  off       = 0.0f;
		if ((flags & ImGuiColorEditFlags_NoBorder) == 0) {
			off = -0.75f; // The border (using Col_FrameBg) tends to look off when color is near-opaque and rounding is enabled. This offset seemed like a good middle ground to reduce those artifacts.
			bb_inner.Expand(off);
		}
		if ((flags & ImGuiColorEditFlags_AlphaPreviewHalf) && col_rgb.w < 1.0f) {
			float mid_x = IM_ROUND((bb_inner.Min.x + bb_inner.Max.x) * 0.5f);
			RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(bb_inner.Min.x + grid_step, bb_inner.Min.y), bb_inner.Max, GetColorU32(col_rgb), grid_step, ImVec2(-grid_step + off, off), rounding, ImDrawFlags_RoundCornersRight);
			window->DrawList->AddRectFilled(bb_inner.Min, ImVec2(mid_x, bb_inner.Max.y), GetColorU32(col_rgb_without_alpha), rounding, ImDrawFlags_RoundCornersLeft);
		} else {
			// Because GetColorU32() multiplies by the global style Alpha and we don't want to display a checkerboard if the source code had no alpha
			ImVec4 col_source = (flags & ImGuiColorEditFlags_AlphaPreview) ? col_rgb : col_rgb_without_alpha;
			if (col_source.w < 1.0f)
				RenderColorRectWithAlphaCheckerboard(window->DrawList, bb_inner.Min, bb_inner.Max, GetColorU32(col_source), grid_step, ImVec2(off, off), rounding);
			else
				window->DrawList->AddRectFilled(bb_inner.Min, bb_inner.Max, GetColorU32(col_source), rounding);
		}
		RenderNavHighlight(bb, id);
		if ((flags & ImGuiColorEditFlags_NoBorder) == 0) {
			if (g.Style.FrameBorderSize > 0.0f)
				RenderFrameBorder(bb.Min, bb.Max, rounding);
			else
				window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), rounding); // Color button are often in need of some sort of border
		}

		// Drag and Drop Source
		// NB: The ActiveId test is merely an optional micro-optimization, BeginDragDropSource() does the same test.
		if (g.ActiveId == id && !(flags & ImGuiColorEditFlags_NoDragDrop) && BeginDragDropSource()) {
			if (flags & ImGuiColorEditFlags_NoAlpha)
				SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, &col_rgb, sizeof(float) * 3, ImGuiCond_Once);
			else
				SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, &col_rgb, sizeof(float) * 4, ImGuiCond_Once);
			VERAColorButton(desc_id, col, flags);
			SameLine();
			TextEx("Color");
			EndDragDropSource();
		}

		// Tooltip
		if (!(flags & ImGuiColorEditFlags_NoTooltip) && hovered)
			VERAColorTooltip(desc_id, &col.x, flags & (ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf));

		return pressed;
	}

	bool VERAColorPicker3(const char *label, float col[3], ImGuiColorEditFlags flags)
	{
		float col4[4] = { col[0], col[1], col[2], 1.0f };
		if (!VERAColorPicker4(label, col4, flags | ImGuiColorEditFlags_NoAlpha))
			return false;
		col[0] = col4[0];
		col[1] = col4[1];
		col[2] = col4[2];
		return true;
	}

	// Note: ColorPicker4() only accesses 3 floats if ImGuiColorEditFlags_NoAlpha flag is set.
	// (In C++ the 'float col[4]' notation for a function argument is equivalent to 'float* col', we only specify a size to facilitate understanding of the code.)
	// FIXME: we adjust the big color square height based on item width, which may cause a flickering feedback loop (if automatic height makes a vertical scrollbar appears, affecting automatic width..)
	// FIXME: this is trying to be aware of style.Alpha but not fully correct. Also, the color wheel will have overlapping glitches with (style.Alpha < 1.0)
	bool VERAColorPicker4(const char *label, float col[4], ImGuiColorEditFlags flags, const float *ref_col)
	{
		ImGuiContext &g      = *GImGui;
		ImGuiWindow  *window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImDrawList *draw_list = window->DrawList;
		ImGuiStyle &style     = g.Style;
		ImGuiIO    &io        = g.IO;

		const float width = CalcItemWidth();
		g.NextItemData.ClearFlags();

		PushID(label);
		BeginGroup();

		if (!(flags & ImGuiColorEditFlags_NoSidePreview))
			flags |= ImGuiColorEditFlags_NoSmallPreview;

		// Context menu: display and store options.
		if (!(flags & ImGuiColorEditFlags_NoOptions))
			ColorPickerOptionsPopup(col, flags);

		// Read stored options
		if (!(flags & ImGuiColorEditFlags_PickerMask_))
			flags |= ((g.ColorEditOptions & ImGuiColorEditFlags_PickerMask_) ? g.ColorEditOptions : ImGuiColorEditFlags_DefaultOptions_) & ImGuiColorEditFlags_PickerMask_;
		if (!(flags & ImGuiColorEditFlags_InputMask_))
			flags |= ((g.ColorEditOptions & ImGuiColorEditFlags_InputMask_) ? g.ColorEditOptions : ImGuiColorEditFlags_DefaultOptions_) & ImGuiColorEditFlags_InputMask_;
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags_PickerMask_)); // Check that only 1 is selected
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags_InputMask_));  // Check that only 1 is selected
		if (!(flags & ImGuiColorEditFlags_NoOptions))
			flags |= (g.ColorEditOptions & ImGuiColorEditFlags_AlphaBar);

		// Setup
		int    components             = (flags & ImGuiColorEditFlags_NoAlpha) ? 3 : 4;
		bool   alpha_bar              = (flags & ImGuiColorEditFlags_AlphaBar) && !(flags & ImGuiColorEditFlags_NoAlpha);
		ImVec2 picker_pos             = window->DC.CursorPos;
		float  square_sz              = GetFrameHeight();
		float  bars_width             = square_sz;                                                                                    // Arbitrary smallish width of Hue/Alpha picking bars
		float  sv_picker_size         = ImMax(bars_width * 1, width - (alpha_bar ? 2 : 1) * (bars_width + style.ItemInnerSpacing.x)); // Saturation/Value picking box
		float  bar0_pos_x             = picker_pos.x + sv_picker_size + style.ItemInnerSpacing.x;
		float  bar1_pos_x             = bar0_pos_x + bars_width + style.ItemInnerSpacing.x;
		float  bars_triangles_half_sz = IM_FLOOR(bars_width * 0.20f);

		float backup_initial_col[4];
		memcpy(backup_initial_col, col, components * sizeof(float));

		float  wheel_thickness = sv_picker_size * 0.08f;
		float  wheel_r_outer   = sv_picker_size * 0.50f;
		float  wheel_r_inner   = wheel_r_outer - wheel_thickness;
		ImVec2 wheel_center(picker_pos.x + (sv_picker_size + bars_width) * 0.5f, picker_pos.y + sv_picker_size * 0.5f);

		// Note: the triangle is displayed rotated with triangle_pa pointing to Hue, but most coordinates stays unrotated for logic.
		float  triangle_r  = wheel_r_inner - (int)(sv_picker_size * 0.027f);
		ImVec2 triangle_pa = ImVec2(triangle_r, 0.0f);                            // Hue point.
		ImVec2 triangle_pb = ImVec2(triangle_r * -0.5f, triangle_r * -0.866025f); // Black point.
		ImVec2 triangle_pc = ImVec2(triangle_r * -0.5f, triangle_r * +0.866025f); // White point.

		float H = col[0], S = col[1], V = col[2];
		float R = col[0], G = col[1], B = col[2];
		if (flags & ImGuiColorEditFlags_InputRGB) {
			// Hue is lost when converting from greyscale rgb (saturation=0). Restore it.
			ColorConvertRGBtoHSV(R, G, B, H, S, V);
			ColorEditRestoreHS(col, &H, &S, &V);
		} else if (flags & ImGuiColorEditFlags_InputHSV) {
			ColorConvertHSVtoRGB(H, S, V, R, G, B);
		}

		bool value_changed = false, value_changed_h = false, value_changed_sv = false;

		PushItemFlag(ImGuiItemFlags_NoNav, true);
		if (flags & ImGuiColorEditFlags_PickerHueWheel) {
			// Hue wheel + SV triangle logic
			InvisibleButton("hsv", ImVec2(sv_picker_size + style.ItemInnerSpacing.x + bars_width, sv_picker_size));
			if (IsItemActive()) {
				ImVec2 initial_off   = g.IO.MouseClickedPos[0] - wheel_center;
				ImVec2 current_off   = g.IO.MousePos - wheel_center;
				float  initial_dist2 = ImLengthSqr(initial_off);
				if (initial_dist2 >= (wheel_r_inner - 1) * (wheel_r_inner - 1) && initial_dist2 <= (wheel_r_outer + 1) * (wheel_r_outer + 1)) {
					// Interactive with Hue wheel
					H = ImAtan2(current_off.y, current_off.x) / IM_PI * 0.5f;
					if (H < 0.0f)
						H += 1.0f;
					value_changed = value_changed_h = true;
				}
				float cos_hue_angle = ImCos(-H * 2.0f * IM_PI);
				float sin_hue_angle = ImSin(-H * 2.0f * IM_PI);
				if (ImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, ImRotate(initial_off, cos_hue_angle, sin_hue_angle))) {
					// Interacting with SV triangle
					ImVec2 current_off_unrotated = ImRotate(current_off, cos_hue_angle, sin_hue_angle);
					if (!ImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated))
						current_off_unrotated = ImTriangleClosestPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated);
					float uu, vv, ww;
					ImTriangleBarycentricCoords(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated, uu, vv, ww);
					V             = ImClamp(1.0f - vv, 0.0001f, 1.0f);
					S             = ImClamp(uu / V, 0.0001f, 1.0f);
					value_changed = value_changed_sv = true;
				}
			}
			if (!(flags & ImGuiColorEditFlags_NoOptions))
				OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
		} else if (flags & ImGuiColorEditFlags_PickerHueBar) {
			// SV rectangle logic
			InvisibleButton("sv", ImVec2(sv_picker_size, sv_picker_size));
			if (IsItemActive()) {
				S = ImSaturate((io.MousePos.x - picker_pos.x) / (sv_picker_size - 1));
				V = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));

				// Greatly reduces hue jitter and reset to 0 when hue == 255 and color is rapidly modified using SV square.
				if (g.ColorEditSavedColor == ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0)))
					H = g.ColorEditSavedHue;
				value_changed = value_changed_sv = true;
			}
			if (!(flags & ImGuiColorEditFlags_NoOptions))
				OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);

			// Hue bar logic
			SetCursorScreenPos(ImVec2(bar0_pos_x, picker_pos.y));
			InvisibleButton("hue", ImVec2(bars_width, sv_picker_size));
			if (IsItemActive()) {
				H             = ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
				value_changed = value_changed_h = true;
			}
		}

		// Alpha bar logic
		if (alpha_bar) {
			SetCursorScreenPos(ImVec2(bar1_pos_x, picker_pos.y));
			InvisibleButton("alpha", ImVec2(bars_width, sv_picker_size));
			if (IsItemActive()) {
				col[3]        = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
				value_changed = true;
			}
		}
		PopItemFlag(); // ImGuiItemFlags_NoNav

		if (!(flags & ImGuiColorEditFlags_NoSidePreview)) {
			SameLine(0, style.ItemInnerSpacing.x);
			BeginGroup();
		}

		if (!(flags & ImGuiColorEditFlags_NoLabel)) {
			const char *label_display_end = FindRenderedTextEnd(label);
			if (label != label_display_end) {
				if ((flags & ImGuiColorEditFlags_NoSidePreview))
					SameLine(0, style.ItemInnerSpacing.x);
				TextEx(label, label_display_end);
			}
		}

		if (!(flags & ImGuiColorEditFlags_NoSidePreview)) {
			PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
			ImVec4 col_v4(col[0], col[1], col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
			if ((flags & ImGuiColorEditFlags_NoLabel))
				Text("Current");

			ImGuiColorEditFlags sub_flags_to_forward = ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
			VERAColorButton("##current", col_v4, (flags & sub_flags_to_forward), ImVec2(square_sz * 3, square_sz * 2));
			if (ref_col != NULL) {
				Text("Original");
				ImVec4 ref_col_v4(ref_col[0], ref_col[1], ref_col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : ref_col[3]);
				if (VERAColorButton("##original", ref_col_v4, (flags & sub_flags_to_forward), ImVec2(square_sz * 3, square_sz * 2))) {
					memcpy(col, ref_col, components * sizeof(float));
					value_changed = true;
				}
			}
			PopItemFlag();
			EndGroup();
		}

		// Convert back color to RGB
		if (value_changed_h || value_changed_sv) {
			if (flags & ImGuiColorEditFlags_InputRGB) {
				ColorConvertHSVtoRGB(H, S, V, col[0], col[1], col[2]);
				g.ColorEditSavedHue   = H;
				g.ColorEditSavedSat   = S;
				g.ColorEditSavedColor = ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0));
			} else if (flags & ImGuiColorEditFlags_InputHSV) {
				col[0] = H;
				col[1] = S;
				col[2] = V;
			}
		}

		// R,G,B and H,S,V slider color editor
		bool value_changed_fix_hue_wrap = false;
		if ((flags & ImGuiColorEditFlags_NoInputs) == 0) {
			PushItemWidth((alpha_bar ? bar1_pos_x : bar0_pos_x) + bars_width - picker_pos.x);
			ImGuiColorEditFlags sub_flags_to_forward = ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf;
			ImGuiColorEditFlags sub_flags            = (flags & sub_flags_to_forward) | ImGuiColorEditFlags_NoPicker;
			if (flags & ImGuiColorEditFlags_DisplayRGB || (flags & ImGuiColorEditFlags_DisplayMask_) == 0)
				if (VERAColorEdit4("##rgb", col, sub_flags | ImGuiColorEditFlags_DisplayRGB)) {
					// FIXME: Hackily differentiating using the DragInt (ActiveId != 0 && !ActiveIdAllowOverlap) vs. using the InputText or DropTarget.
					// For the later we don't want to run the hue-wrap canceling code. If you are well versed in HSV picker please provide your input! (See #2050)
					value_changed_fix_hue_wrap = (g.ActiveId != 0 && !g.ActiveIdAllowOverlap);
					value_changed              = true;
				}
			if (flags & ImGuiColorEditFlags_DisplayHSV || (flags & ImGuiColorEditFlags_DisplayMask_) == 0)
				value_changed |= VERAColorEdit4("##hsv", col, sub_flags | ImGuiColorEditFlags_DisplayHSV);
			if (flags & ImGuiColorEditFlags_DisplayHex || (flags & ImGuiColorEditFlags_DisplayMask_) == 0)
				value_changed |= VERAColorEdit4("##hex", col, sub_flags | ImGuiColorEditFlags_DisplayHex);
			PopItemWidth();
		}

		// Try to cancel hue wrap (after ColorEdit4 call), if any
		if (value_changed_fix_hue_wrap && (flags & ImGuiColorEditFlags_InputRGB)) {
			float new_H, new_S, new_V;
			ColorConvertRGBtoHSV(col[0], col[1], col[2], new_H, new_S, new_V);
			if (new_H <= 0 && H > 0) {
				if (new_V <= 0 && V != new_V)
					ColorConvertHSVtoRGB(H, S, new_V <= 0 ? V * 0.5f : new_V, col[0], col[1], col[2]);
				else if (new_S <= 0)
					ColorConvertHSVtoRGB(H, new_S <= 0 ? S * 0.5f : new_S, new_V, col[0], col[1], col[2]);
			}
		}

		if (value_changed) {
			if (flags & ImGuiColorEditFlags_InputRGB) {
				R = col[0];
				G = col[1];
				B = col[2];
				ColorConvertRGBtoHSV(R, G, B, H, S, V);
				ColorEditRestoreHS(col, &H, &S, &V); // Fix local Hue as display below will use it immediately.
			} else if (flags & ImGuiColorEditFlags_InputHSV) {
				H = col[0];
				S = col[1];
				V = col[2];
				ColorConvertHSVtoRGB(H, S, V, R, G, B);
			}
		}

		const int   style_alpha8    = IM_F32_TO_INT8_SAT(style.Alpha);
		const ImU32 col_black       = IM_COL32(0, 0, 0, style_alpha8);
		const ImU32 col_white       = IM_COL32(255, 255, 255, style_alpha8);
		const ImU32 col_midgrey     = IM_COL32(128, 128, 128, style_alpha8);
		const ImU32 col_hues[6 + 1] = { IM_COL32(255, 0, 0, style_alpha8), IM_COL32(255, 255, 0, style_alpha8), IM_COL32(0, 255, 0, style_alpha8), IM_COL32(0, 255, 255, style_alpha8), IM_COL32(0, 0, 255, style_alpha8), IM_COL32(255, 0, 255, style_alpha8), IM_COL32(255, 0, 0, style_alpha8) };

		ImVec4 hue_color_f(1, 1, 1, style.Alpha);
		ColorConvertHSVtoRGB(H, 1, 1, hue_color_f.x, hue_color_f.y, hue_color_f.z);
		ImU32 hue_color32                 = ColorConvertFloat4ToU32(hue_color_f);
		ImU32 user_col32_striped_of_alpha = ColorConvertFloat4ToU32(ImVec4(R, G, B, style.Alpha)); // Important: this is still including the main rendering/style alpha!!

		ImVec2 sv_cursor_pos;

		if (flags & ImGuiColorEditFlags_PickerHueWheel) {
			// Render Hue Wheel
			const float aeps            = 0.5f / wheel_r_outer; // Half a pixel arc length in radians (2pi cancels out).
			const int   segment_per_arc = ImMax(32, (int)wheel_r_outer / 12);
			for (int n = 0; n < 6; n++) {
				const float a0             = (n) / 6.0f * 2.0f * IM_PI - aeps;
				const float a1             = (n + 1.0f) / 6.0f * 2.0f * IM_PI + aeps;
				const int   vert_start_idx = draw_list->VtxBuffer.Size;
				draw_list->PathArcTo(wheel_center, (wheel_r_inner + wheel_r_outer) * 0.5f, a0, a1, segment_per_arc);
				draw_list->PathStroke(col_white, 0, wheel_thickness);
				const int vert_end_idx = draw_list->VtxBuffer.Size;

				// Paint colors over existing vertices
				ImVec2 gradient_p0(wheel_center.x + ImCos(a0) * wheel_r_inner, wheel_center.y + ImSin(a0) * wheel_r_inner);
				ImVec2 gradient_p1(wheel_center.x + ImCos(a1) * wheel_r_inner, wheel_center.y + ImSin(a1) * wheel_r_inner);
				ShadeVertsLinearColorGradientKeepAlpha(draw_list, vert_start_idx, vert_end_idx, gradient_p0, gradient_p1, col_hues[n], col_hues[n + 1]);
			}

			// Render Cursor + preview on Hue Wheel
			float  cos_hue_angle = ImCos(H * 2.0f * IM_PI);
			float  sin_hue_angle = ImSin(H * 2.0f * IM_PI);
			ImVec2 hue_cursor_pos(wheel_center.x + cos_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f, wheel_center.y + sin_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f);
			float  hue_cursor_rad      = value_changed_h ? wheel_thickness * 0.65f : wheel_thickness * 0.55f;
			int    hue_cursor_segments = ImClamp((int)(hue_cursor_rad / 1.4f), 9, 32);
			draw_list->AddCircleFilled(hue_cursor_pos, hue_cursor_rad, hue_color32, hue_cursor_segments);
			draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad + 1, col_midgrey, hue_cursor_segments);
			draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad, col_white, hue_cursor_segments);

			// Render SV triangle (rotated according to hue)
			ImVec2 tra      = wheel_center + ImRotate(triangle_pa, cos_hue_angle, sin_hue_angle);
			ImVec2 trb      = wheel_center + ImRotate(triangle_pb, cos_hue_angle, sin_hue_angle);
			ImVec2 trc      = wheel_center + ImRotate(triangle_pc, cos_hue_angle, sin_hue_angle);
			ImVec2 uv_white = GetFontTexUvWhitePixel();
			draw_list->PrimReserve(6, 6);
			draw_list->PrimVtx(tra, uv_white, hue_color32);
			draw_list->PrimVtx(trb, uv_white, hue_color32);
			draw_list->PrimVtx(trc, uv_white, col_white);
			draw_list->PrimVtx(tra, uv_white, 0);
			draw_list->PrimVtx(trb, uv_white, col_black);
			draw_list->PrimVtx(trc, uv_white, 0);
			draw_list->AddTriangle(tra, trb, trc, col_midgrey, 1.5f);
			sv_cursor_pos = ImLerp(ImLerp(trc, tra, ImSaturate(S)), trb, ImSaturate(1 - V));
		} else if (flags & ImGuiColorEditFlags_PickerHueBar) {
			// Render SV Square
			draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + ImVec2(sv_picker_size, sv_picker_size), col_white, hue_color32, hue_color32, col_white);
			draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + ImVec2(sv_picker_size, sv_picker_size), 0, 0, col_black, col_black);
			RenderFrameBorder(picker_pos, picker_pos + ImVec2(sv_picker_size, sv_picker_size), 0.0f);
			sv_cursor_pos.x = ImClamp(IM_ROUND(picker_pos.x + ImSaturate(S) * sv_picker_size), picker_pos.x + 2, picker_pos.x + sv_picker_size - 2); // Sneakily prevent the circle to stick out too much
			sv_cursor_pos.y = ImClamp(IM_ROUND(picker_pos.y + ImSaturate(1 - V) * sv_picker_size), picker_pos.y + 2, picker_pos.y + sv_picker_size - 2);

			// Render Hue Bar
			for (int i = 0; i < 6; ++i)
				draw_list->AddRectFilledMultiColor(ImVec2(bar0_pos_x, picker_pos.y + i * (sv_picker_size / 6)), ImVec2(bar0_pos_x + bars_width, picker_pos.y + (i + 1) * (sv_picker_size / 6)), col_hues[i], col_hues[i], col_hues[i + 1], col_hues[i + 1]);
			float bar0_line_y = IM_ROUND(picker_pos.y + H * sv_picker_size);
			RenderFrameBorder(ImVec2(bar0_pos_x, picker_pos.y), ImVec2(bar0_pos_x + bars_width, picker_pos.y + sv_picker_size), 0.0f);
			RenderArrowsForVerticalBar(draw_list, ImVec2(bar0_pos_x - 1, bar0_line_y), ImVec2(bars_triangles_half_sz + 1, bars_triangles_half_sz), bars_width + 2.0f, style.Alpha);
		}

		// Render cursor/preview circle (clamp S/V within 0..1 range because floating points colors may lead HSV values to be out of range)
		float sv_cursor_rad = value_changed_sv ? 10.0f : 6.0f;
		draw_list->AddCircleFilled(sv_cursor_pos, sv_cursor_rad, user_col32_striped_of_alpha, 12);
		draw_list->AddCircle(sv_cursor_pos, sv_cursor_rad + 1, col_midgrey, 12);
		draw_list->AddCircle(sv_cursor_pos, sv_cursor_rad, col_white, 12);

		// Render alpha bar
		if (alpha_bar) {
			float  alpha = ImSaturate(col[3]);
			ImRect bar1_bb(bar1_pos_x, picker_pos.y, bar1_pos_x + bars_width, picker_pos.y + sv_picker_size);
			RenderColorRectWithAlphaCheckerboard(draw_list, bar1_bb.Min, bar1_bb.Max, 0, bar1_bb.GetWidth() / 2.0f, ImVec2(0.0f, 0.0f));
			draw_list->AddRectFilledMultiColor(bar1_bb.Min, bar1_bb.Max, user_col32_striped_of_alpha, user_col32_striped_of_alpha, user_col32_striped_of_alpha & ~IM_COL32_A_MASK, user_col32_striped_of_alpha & ~IM_COL32_A_MASK);
			float bar1_line_y = IM_ROUND(picker_pos.y + (1.0f - alpha) * sv_picker_size);
			RenderFrameBorder(bar1_bb.Min, bar1_bb.Max, 0.0f);
			RenderArrowsForVerticalBar(draw_list, ImVec2(bar1_pos_x - 1, bar1_line_y), ImVec2(bars_triangles_half_sz + 1, bars_triangles_half_sz), bars_width + 2.0f, style.Alpha);
		}

		EndGroup();

		if (value_changed && memcmp(backup_initial_col, col, components * sizeof(float)) == 0)
			value_changed = false;
		if (value_changed)
			MarkItemEdited(g.LastItemData.ID);

		PopID();

		return value_changed;
	}

	bool VERAColorEdit3(const char *label, float col[3], ImGuiColorEditFlags flags)
	{
		return VERAColorEdit4(label, col, flags | ImGuiColorEditFlags_NoAlpha);
	}

	// Edit colors components (each component in 0.0f..1.0f range).
	// See enum ImGuiColorEditFlags_ for available options. e.g. Only access 3 floats if ImGuiColorEditFlags_NoAlpha flag is set.
	// With typical options: Left-click on color square to open color picker. Right-click to open option menu. CTRL-Click over input fields to edit them and TAB to go to next item.
	bool VERAColorEdit4(const char *label, float col[4], ImGuiColorEditFlags flags)
	{
		ImGuiWindow *window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext     &g                 = *GImGui;
		const ImGuiStyle &style             = g.Style;
		const float       square_sz         = GetFrameHeight();
		const float       w_full            = CalcItemWidth();
		const float       w_button          = (flags & ImGuiColorEditFlags_NoSmallPreview) ? 0.0f : (square_sz + style.ItemInnerSpacing.x);
		const float       w_inputs          = w_full - w_button;
		const char       *label_display_end = FindRenderedTextEnd(label);
		g.NextItemData.ClearFlags();

		BeginGroup();
		PushID(label);

		// If we're not showing any slider there's no point in doing any HSV conversions
		const ImGuiColorEditFlags flags_untouched = flags;
		if (flags & ImGuiColorEditFlags_NoInputs)
			flags = (flags & (~ImGuiColorEditFlags_DisplayMask_)) | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoOptions;

		// Context menu: display and modify options (before defaults are applied)
		if (!(flags & ImGuiColorEditFlags_NoOptions))
			ColorEditOptionsPopup(col, flags);

		// Read stored options
		if (!(flags & ImGuiColorEditFlags_DisplayMask_))
			flags |= (g.ColorEditOptions & ImGuiColorEditFlags_DisplayMask_);
		if (!(flags & ImGuiColorEditFlags_DataTypeMask_))
			flags |= (g.ColorEditOptions & ImGuiColorEditFlags_DataTypeMask_);
		if (!(flags & ImGuiColorEditFlags_PickerMask_))
			flags |= (g.ColorEditOptions & ImGuiColorEditFlags_PickerMask_);
		if (!(flags & ImGuiColorEditFlags_InputMask_))
			flags |= (g.ColorEditOptions & ImGuiColorEditFlags_InputMask_);
		flags |= (g.ColorEditOptions & ~(ImGuiColorEditFlags_DisplayMask_ | ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ | ImGuiColorEditFlags_InputMask_));
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags_DisplayMask_)); // Check that only 1 is selected
		IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags_InputMask_));   // Check that only 1 is selected

		const bool alpha      = (flags & ImGuiColorEditFlags_NoAlpha) == 0;
		const bool hdr        = (flags & ImGuiColorEditFlags_HDR) != 0;
		const int  components = alpha ? 4 : 3;

		// Convert to the formats we need
		float f[4] = { col[0], col[1], col[2], alpha ? col[3] : 1.0f };
		if ((flags & ImGuiColorEditFlags_InputHSV) && (flags & ImGuiColorEditFlags_DisplayRGB))
			ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
		else if ((flags & ImGuiColorEditFlags_InputRGB) && (flags & ImGuiColorEditFlags_DisplayHSV)) {
			// Hue is lost when converting from greyscale rgb (saturation=0). Restore it.
			ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);
			ColorEditRestoreHS(col, &f[0], &f[1], &f[2]);
		}
		int i[4] = { IM_F32_TO_INT8_UNBOUND(f[0]) >> 4, IM_F32_TO_INT8_UNBOUND(f[1]) >> 4, IM_F32_TO_INT8_UNBOUND(f[2]) >> 4, IM_F32_TO_INT8_UNBOUND(f[3]) >> 4 };

		bool value_changed          = false;
		bool value_changed_as_float = false;

		const ImVec2 pos             = window->DC.CursorPos;
		const float  inputs_offset_x = (style.ColorButtonPosition == ImGuiDir_Left) ? w_button : 0.0f;
		window->DC.CursorPos.x       = pos.x + inputs_offset_x;

		if ((flags & (ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV)) != 0 && (flags & ImGuiColorEditFlags_NoInputs) == 0) {
			// RGB/HSV 0..255 Sliders
			const float w_item_one  = ImMax(1.0f, IM_FLOOR((w_inputs - (style.ItemInnerSpacing.x) * (components - 1)) / (float)components));
			const float w_item_last = ImMax(1.0f, IM_FLOOR(w_inputs - (w_item_one + style.ItemInnerSpacing.x) * (components - 1)));

			const bool         hide_prefix         = (w_item_one <= CalcTextSize((flags & ImGuiColorEditFlags_Float) ? "M:0.000" : "M:000").x);
			static const char *ids[4]              = { "##X", "##Y", "##Z", "##W" };
			static const char *fmt_table_int[3][4] = {
				{ "%2d", "%2d", "%2d", "%2d" },         // Short display
				{ "R:%2d", "G:%2d", "B:%2d", "A:%2d" }, // Long display for RGBA
				{ "H:%2d", "S:%2d", "V:%2d", "A:%2d" }  // Long display for HSVA
			};
			static const char *fmt_table_float[3][4] = {
				{ "%0.3f", "%0.3f", "%0.3f", "%0.3f" },         // Short display
				{ "R:%0.3f", "G:%0.3f", "B:%0.3f", "A:%0.3f" }, // Long display for RGBA
				{ "H:%0.3f", "S:%0.3f", "V:%0.3f", "A:%0.3f" }  // Long display for HSVA
			};
			const int fmt_idx = hide_prefix ? 0 : (flags & ImGuiColorEditFlags_DisplayHSV) ? 2
			                                                                               : 1;

			for (int n = 0; n < components; n++) {
				if (n > 0)
					SameLine(0, style.ItemInnerSpacing.x);
				SetNextItemWidth((n + 1 < components) ? w_item_one : w_item_last);

				// FIXME: When ImGuiColorEditFlags_HDR flag is passed HS values snap in weird ways when SV values go below 0.
				if (flags & ImGuiColorEditFlags_Float) {
					value_changed |= DragFloat(ids[n], &f[n], 1.0f / 15.0f, 0.0f, hdr ? 0.0f : 1.0f, fmt_table_float[fmt_idx][n]);
					value_changed_as_float |= value_changed;
				} else {
					value_changed |= DragInt(ids[n], &i[n], 1.0f, 0, hdr ? 0 : 15, fmt_table_int[fmt_idx][n]);
				}
				if (!(flags & ImGuiColorEditFlags_NoOptions))
					OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
			}
		} else if ((flags & ImGuiColorEditFlags_DisplayHex) != 0 && (flags & ImGuiColorEditFlags_NoInputs) == 0) {
			// RGB Hexadecimal Input
			char buf[64];
			if (alpha)
				ImFormatString(buf, IM_ARRAYSIZE(buf), "#%01X%01X%01X%01X", ImClamp(i[0], 0, 15), ImClamp(i[1], 0, 15), ImClamp(i[2], 0, 15), ImClamp(i[3], 0, 15));
			else
				ImFormatString(buf, IM_ARRAYSIZE(buf), "#%01X%01X%01X", ImClamp(i[0], 0, 15), ImClamp(i[1], 0, 15), ImClamp(i[2], 0, 15));
			SetNextItemWidth(w_inputs);
			if (InputText("##Text", buf, IM_ARRAYSIZE(buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
				value_changed = true;
				char *p       = buf;
				while (*p == '#' || ImCharIsBlankA(*p))
					p++;
				i[0] = i[1] = i[2] = 0;
				i[3]               = 0xFF; // alpha default to 255 is not parsed by scanf (e.g. inputting #FFFFFF omitting alpha)
				int r;
				if (alpha)
					r = sscanf(p, "%01X%01X%01X%01X", (unsigned int *)&i[0], (unsigned int *)&i[1], (unsigned int *)&i[2], (unsigned int *)&i[3]); // Treat at unsigned (%X is unsigned)
				else
					r = sscanf(p, "%01X%01X%01X", (unsigned int *)&i[0], (unsigned int *)&i[1], (unsigned int *)&i[2]);
				IM_UNUSED(r); // Fixes C6031: Return value ignored: 'sscanf'.
			}
			if (!(flags & ImGuiColorEditFlags_NoOptions))
				OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
		}

		ImGuiWindow *picker_active_window = NULL;
		if (!(flags & ImGuiColorEditFlags_NoSmallPreview)) {
			const float button_offset_x = ((flags & ImGuiColorEditFlags_NoInputs) || (style.ColorButtonPosition == ImGuiDir_Left)) ? 0.0f : w_inputs + style.ItemInnerSpacing.x;
			window->DC.CursorPos        = ImVec2(pos.x + button_offset_x, pos.y);

			const ImVec4 col_v4(col[0], col[1], col[2], alpha ? col[3] : 1.0f);
			if (VERAColorButton("##ColorButton", col_v4, flags)) {
				if (!(flags & ImGuiColorEditFlags_NoPicker)) {
					// Store current color and open a picker
					g.ColorPickerRef = col_v4;
					OpenPopup("picker");
					SetNextWindowPos(g.LastItemData.Rect.GetBL() + ImVec2(0.0f, style.ItemSpacing.y));
				}
			}
			if (!(flags & ImGuiColorEditFlags_NoOptions))
				OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);

			if (BeginPopup("picker")) {
				picker_active_window = g.CurrentWindow;
				if (label != label_display_end) {
					TextEx(label, label_display_end);
					Spacing();
				}
				ImGuiColorEditFlags picker_flags_to_forward = ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ | ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaBar;
				ImGuiColorEditFlags picker_flags            = (flags_untouched & picker_flags_to_forward) | ImGuiColorEditFlags_DisplayMask_ | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf;
				SetNextItemWidth(square_sz * 12.0f); // Use 256 + bar sizes?
				value_changed |= VERAColorPicker4("##picker", col, picker_flags, &g.ColorPickerRef.x);
				EndPopup();
			}
		}

		if (label != label_display_end && !(flags & ImGuiColorEditFlags_NoLabel)) {
			SameLine(0.0f, style.ItemInnerSpacing.x);
			TextEx(label, label_display_end);
		}

		// Convert back
		if (value_changed && picker_active_window == NULL) {
			if (!value_changed_as_float)
				for (int n = 0; n < 4; n++) {
					i[n] |= (i[n] << 4);
					f[n] = i[n] / 255.0f;
				}
			if ((flags & ImGuiColorEditFlags_DisplayHSV) && (flags & ImGuiColorEditFlags_InputRGB)) {
				g.ColorEditSavedHue = f[0];
				g.ColorEditSavedSat = f[1];
				ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
				g.ColorEditSavedColor = ColorConvertFloat4ToU32(ImVec4(f[0], f[1], f[2], 0));
			}
			if ((flags & ImGuiColorEditFlags_DisplayRGB) && (flags & ImGuiColorEditFlags_InputHSV))
				ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);

			col[0] = f[0];
			col[1] = f[1];
			col[2] = f[2];
			if (alpha)
				col[3] = f[3];
		}

		PopID();
		EndGroup();

		// Drag and Drop Target
		// NB: The flag test is merely an optional micro-optimization, BeginDragDropTarget() does the same test.
		if ((g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredRect) && !(flags & ImGuiColorEditFlags_NoDragDrop) && BeginDragDropTarget()) {
			bool accepted_drag_drop = false;
			if (const ImGuiPayload *payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
				memcpy((float *)col, payload->Data, sizeof(float) * 3); // Preserve alpha if any //-V512
				value_changed = accepted_drag_drop = true;
			}
			if (const ImGuiPayload *payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F)) {
				memcpy((float *)col, payload->Data, sizeof(float) * components);
				value_changed = accepted_drag_drop = true;
			}

			// Drag-drop payloads are always RGB
			if (accepted_drag_drop && (flags & ImGuiColorEditFlags_InputHSV))
				ColorConvertRGBtoHSV(col[0], col[1], col[2], col[0], col[1], col[2]);
			EndDragDropTarget();
		}

		// When picker is being actively used, use its active id so IsItemActive() will function on ColorEdit4().
		if (picker_active_window && g.ActiveId != 0 && g.ActiveIdWindow == picker_active_window)
			g.LastItemData.ID = g.ActiveId;

		if (value_changed)
			MarkItemEdited(g.LastItemData.ID);

		return value_changed;
	}

} // namespace ImGui