#include "cpu_visualization.h"

#include "imgui/imgui.h"

#include "glue.h"
#include "memory.h"
#include "vera/vera_video.h"

static bool                        Enabled = false;
static uint32_t                    Framebuffer[SCAN_WIDTH * SCAN_HEIGHT];
static uint32_t                    Last_p         = 0;
static cpu_visualization_coloring  Coloring_type  = cpu_visualization_coloring::ADDRESS;
static cpu_visualization_highlight Highlight_type = cpu_visualization_highlight::INVISIBLE;

float fmodf(float f, int m)
{
	const int ff = f;
	return (ff % m) + (f - (float)ff);
}

struct color_bgra {
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;
};

union color_u32 {
	color_bgra c;
	uint32_t   u;
};

static color_bgra hsv_to_rgb(float h, float s, float v)
{
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(h, s, v, b, g, r);

	return {
		255, static_cast<uint8_t>(r * 255.0f), static_cast<uint8_t>(g * 255.0f), static_cast<uint8_t>(b * 255.0f)
	};

	//const float c  = s * v;
	//const float hh = h / 60.0f;
	//const float x  = c * (1.0f - fabsf(fmodf(hh, 2) - 1.0f));
	//const float m  = v - c;
	//switch ((int)hh) {
	//	case 0:
	//		return { 255, static_cast<uint8_t>((m)*255.0f), static_cast<uint8_t>((x + m) * 255.0f), static_cast<uint8_t>((c + m) * 255.0f) };
	//	case 1:
	//		return { 255, static_cast<uint8_t>((m)*255.0f), static_cast<uint8_t>((c + m) * 255.0f), static_cast<uint8_t>((x + m) * 255.0f) };
	//	case 2:
	//		return { 255, static_cast<uint8_t>((x + m) * 255.0f), static_cast<uint8_t>((c + m) * 255.0f), static_cast<uint8_t>((m)*255.0f) };
	//	case 3:
	//		return { 255, static_cast<uint8_t>((c + m) * 255.0f), static_cast<uint8_t>((x + m) * 255.0f), static_cast<uint8_t>((m)*255.0f) };
	//	case 4:
	//		return { 255, static_cast<uint8_t>((c + m) * 255.0f), static_cast<uint8_t>((m)*255.0f), static_cast<uint8_t>((x + m) * 255.0f) };
	//	case 5:
	//		return { 255, static_cast<uint8_t>((x + m) * 255.0f), static_cast<uint8_t>((m)*255.0f), static_cast<uint8_t>((c + m) * 255.0f) };
	//	default:
	//		return { 0, 0, 0, 0 };
	//}
}

void cpu_visualization_enable(bool enable)
{
	Enabled = enable;
}

void cpu_visualization_step()
{
	if (!Enabled) {
		return;
	}

	const float sv = []() -> float {
		switch (Highlight_type) {
			case cpu_visualization_highlight::NONE:
				return 0.95f;

			case cpu_visualization_highlight::IRQ:
				return (status & 0x04) ? 0.95f : 0.50f;

			case cpu_visualization_highlight::VISIBLE: {
				const vera_video_rect visible = vera_video_get_scan_visible();
				const uint32_t        x       = vera_video_get_scan_pos_x();
				const uint32_t        y       = vera_video_get_scan_pos_y();
				return (x >= visible.hstart && x < visible.hstop && y >= visible.vstart && y < visible.vstop) ? 0.95f : 0.50f;
			}
			case cpu_visualization_highlight::INVISIBLE: {
				const vera_video_rect visible = vera_video_get_scan_visible();
				const uint32_t        x       = vera_video_get_scan_pos_x();
				const uint32_t        y       = vera_video_get_scan_pos_y();
				return (x >= visible.hstart && x < visible.hstop && y >= visible.vstart && y < visible.vstop) ? 0.50f : 0.95f;
			}
			default:
				return 0.95f;
		}
	}();

	const color_u32 vis_color = [sv]() -> color_u32 {
		switch (Coloring_type) {
			case cpu_visualization_coloring::ADDRESS:
				return { hsv_to_rgb((float)pc / 65536.0f, sv, sv) };
			case cpu_visualization_coloring::INSTRUCTION:
				return { hsv_to_rgb((float)debug_read6502(pc) / 256.0f, sv, sv) };
			case cpu_visualization_coloring::TEST: {
				static int count = 0;
				count            = (count + 1) % (256 << 4);
				return { hsv_to_rgb((float)count / (float)(256 << 4), sv, sv) };
			}
			default:
				return { 0, 0, 0, 0 };
		}
	}();

	uint32_t end_p = (uint32_t)vera_video_get_scan_pos_x() + (SCAN_WIDTH * vera_video_get_scan_pos_y());
	if (end_p < Last_p) {
		for (uint32_t p = Last_p; p < SCAN_WIDTH * SCAN_HEIGHT; ++p) {
			Framebuffer[p] = vis_color.u;
		}
		for (uint32_t p = 0; p < end_p; ++p) {
			Framebuffer[p] = vis_color.u;
		}
	} else {
		for (uint32_t p = Last_p; p < end_p; ++p) {
			Framebuffer[p] = vis_color.u;
		}
	}
	Last_p = end_p;
}

const uint32_t *cpu_visualization_get_framebuffer()
{
	return Framebuffer;
}

void cpu_visualization_set_coloring(cpu_visualization_coloring coloring)
{
	Coloring_type = coloring;
}

cpu_visualization_coloring cpu_visualization_get_coloring()
{
	return Coloring_type;
}

void cpu_visualization_set_highlight(cpu_visualization_highlight highlight)
{
	Highlight_type = highlight;
}

cpu_visualization_highlight cpu_visualization_get_highlight()
{
	return Highlight_type;
}
