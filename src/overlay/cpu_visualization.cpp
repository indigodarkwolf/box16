#include "cpu_visualization.h"

#include "imgui/imgui.h"

#include "glue.h"
#include "memory.h"
#include "vera/vera_video.h"

static bool                        Enabled = false;
static uint32_t                    Framebuffer[SCAN_WIDTH * SCAN_HEIGHT];
static uint8_t                     Framebuffer_opcodes[SCAN_WIDTH * SCAN_HEIGHT];
static uint32_t                    Framebuffer_addrs[SCAN_WIDTH * SCAN_HEIGHT];
static uint32_t                    Last_p         = 0;
static cpu_visualization_coloring  Coloring_type  = cpu_visualization_coloring::ADDRESS;
static cpu_visualization_highlight Highlight_type = cpu_visualization_highlight::INVISIBLE;

struct color_abgr {
	uint8_t a;
	uint8_t b;
	uint8_t g;
	uint8_t r;
};

union color_u32 {
	color_abgr c;
	uint32_t   u;
};

// Original Michael Steil colors
static color_u32 Color_load  = { 255, 153, 162, 255 };
static color_u32 Color_trans = { 255, 187, 153, 255 };
static color_u32 Color_stack = { 255, 255, 153, 238 };
static color_u32 Color_shift = { 255, 240, 192, 168 };
static color_u32 Color_logic = { 255, 240, 216, 168 };
static color_u32 Color_arith = { 255, 180, 240, 168 };
static color_u32 Color_inc   = { 255, 168, 240, 204 };
static color_u32 Color_ctrl  = { 255, 102, 242, 255 };
static color_u32 Color_bra   = { 255, 102, 222, 255 };
static color_u32 Color_flags = { 255, 102, 201, 255 };

static color_u32 Color_nop   = { 255, 191, 191, 191 };
static color_u32 Color_wai   = { 255, 20, 20, 20 };

static color_u32 Op_color_table[256] = {
	/* 0 */ Color_ctrl, Color_logic, Color_nop, Color_nop, Color_logic, Color_logic, Color_shift, Color_logic, Color_stack, Color_logic, Color_shift, Color_nop, Color_logic, Color_logic, Color_shift, Color_ctrl, /* 0 */
	/* 1 */ Color_bra, Color_logic, Color_logic, Color_nop, Color_logic, Color_logic, Color_shift, Color_logic, Color_flags, Color_logic, Color_inc, Color_nop, Color_logic, Color_logic, Color_shift, Color_ctrl,  /* 1 */
	/* 2 */ Color_ctrl, Color_logic, Color_nop, Color_nop, Color_logic, Color_logic, Color_shift, Color_logic, Color_stack, Color_logic, Color_shift, Color_nop, Color_logic, Color_logic, Color_shift, Color_ctrl, /* 2 */
	/* 3 */ Color_bra, Color_logic, Color_logic, Color_nop, Color_logic, Color_logic, Color_shift, Color_logic, Color_flags, Color_logic, Color_inc, Color_nop, Color_logic, Color_logic, Color_shift, Color_ctrl,  /* 3 */
	/* 4 */ Color_ctrl, Color_logic, Color_nop, Color_nop, Color_nop, Color_logic, Color_shift, Color_logic, Color_stack, Color_logic, Color_shift, Color_nop, Color_ctrl, Color_logic, Color_shift, Color_ctrl,    /* 4 */
	/* 5 */ Color_bra, Color_logic, Color_logic, Color_nop, Color_nop, Color_logic, Color_shift, Color_logic, Color_flags, Color_logic, Color_stack, Color_nop, Color_nop, Color_logic, Color_shift, Color_ctrl,    /* 5 */
	/* 6 */ Color_ctrl, Color_arith, Color_nop, Color_nop, Color_load, Color_arith, Color_shift, Color_logic, Color_stack, Color_arith, Color_shift, Color_nop, Color_ctrl, Color_arith, Color_shift, Color_ctrl,   /* 6 */
	/* 7 */ Color_bra, Color_arith, Color_arith, Color_nop, Color_load, Color_arith, Color_shift, Color_logic, Color_flags, Color_arith, Color_stack, Color_nop, Color_ctrl, Color_arith, Color_shift, Color_ctrl,  /* 7 */
	/* 8 */ Color_ctrl, Color_load, Color_nop, Color_nop, Color_load, Color_load, Color_load, Color_logic, Color_inc, Color_logic, Color_trans, Color_nop, Color_load, Color_load, Color_load, Color_ctrl,          /* 8 */
	/* 9 */ Color_bra, Color_load, Color_load, Color_nop, Color_load, Color_load, Color_load, Color_logic, Color_trans, Color_load, Color_trans, Color_nop, Color_load, Color_load, Color_load, Color_ctrl,         /* 9 */
	/* A */ Color_load, Color_load, Color_load, Color_nop, Color_load, Color_load, Color_load, Color_logic, Color_trans, Color_load, Color_trans, Color_nop, Color_load, Color_load, Color_load, Color_ctrl,        /* A */
	/* B */ Color_bra, Color_load, Color_load, Color_nop, Color_load, Color_load, Color_load, Color_logic, Color_flags, Color_load, Color_trans, Color_nop, Color_load, Color_load, Color_load, Color_ctrl,         /* B */
	/* C */ Color_arith, Color_arith, Color_nop, Color_nop, Color_arith, Color_arith, Color_inc, Color_logic, Color_inc, Color_arith, Color_inc, Color_wai, Color_arith, Color_arith, Color_inc, Color_ctrl,        /* C */
	/* D */ Color_bra, Color_arith, Color_arith, Color_nop, Color_nop, Color_arith, Color_inc, Color_logic, Color_flags, Color_arith, Color_stack, Color_ctrl, Color_nop, Color_arith, Color_inc, Color_ctrl,       /* D */
	/* E */ Color_arith, Color_arith, Color_nop, Color_nop, Color_arith, Color_arith, Color_inc, Color_logic, Color_inc, Color_arith, Color_nop, Color_nop, Color_arith, Color_arith, Color_inc, Color_ctrl,        /* E */
	/* F */ Color_bra, Color_arith, Color_arith, Color_nop, Color_nop, Color_arith, Color_inc, Color_logic, Color_flags, Color_arith, Color_stack, Color_nop, Color_nop, Color_arith, Color_inc, Color_ctrl         /* F */
};

static color_abgr hsv_to_rgb(float h, float s, float v)
{
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);

	return {
		255, static_cast<uint8_t>(b * 255.0f), static_cast<uint8_t>(g * 255.0f), static_cast<uint8_t>(r * 255.0f)
	};
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

	static constexpr const float bright = 1.00f;
	static constexpr const float dim    = 0.65f;

	const float sv = []() -> float {
		switch (Highlight_type) {
			case cpu_visualization_highlight::NONE:
				return bright;

			case cpu_visualization_highlight::IRQ:
				return (state6502.status & 0x04) ? bright : dim;

			case cpu_visualization_highlight::VISIBLE: {
				const vera_video_rect visible = vera_video_get_scan_visible();
				const uint32_t        x       = (uint32_t)vera_video_get_scan_pos_x();
				const uint32_t        y       = vera_video_get_scan_pos_y();
				return (x >= visible.hstart && x < visible.hstop && y >= visible.vstart && y < visible.vstop) ? bright : dim;
			}
			case cpu_visualization_highlight::INVISIBLE: {
				const vera_video_rect visible = vera_video_get_scan_visible();
				const uint32_t        x       = (uint32_t)vera_video_get_scan_pos_x();
				const uint32_t        y       = vera_video_get_scan_pos_y();
				return (x >= visible.hstart && x < visible.hstop && y >= visible.vstart && y < visible.vstop) ? dim : bright;
			}
			default:
				return bright;
		}
	}();

	const color_u32 vis_color = [sv]() -> color_u32 {
		switch (Coloring_type) {
			case cpu_visualization_coloring::ADDRESS:
				return { hsv_to_rgb((float)state6502.pc / 65536.0f, sv, sv) };

			case cpu_visualization_coloring::INSTRUCTION: {
				uint8_t instruction = debug_read6502(state6502.pc - waiting);
				return Op_color_table[instruction];
				// if (instruction == 0xCB) {
				//	return { 0, 0, 0, 0 };
				// } else {
				//	return { hsv_to_rgb((float)instruction / 256.0f, sv, sv) };
				// }
			}
#if defined(_DEBUG)
			case cpu_visualization_coloring::TEST: {
				static int count = 0;
				count            = (count + 1) % (256 << 4);
				return { hsv_to_rgb((float)count / (float)(256 << 4), sv, sv) };
			}
#endif
			default:
				return { 0, 0, 0, 0 };
		}
	}();

	color_u32 end_color;
	end_color.u = ((vis_color.u & 0xf8f8f8f8) >> 3) | 0x000000ff;

	auto shade_color = [sv](color_u32 c) -> color_u32 {
		return { 255, static_cast<uint8_t>(c.c.b * sv), static_cast<uint8_t>(c.c.g * sv), static_cast<uint8_t>(c.c.r * sv) };
	};

	auto lerp_colors = [sv](color_u32 c0, color_u32 c1, uint32_t t0, uint32_t t1, uint32_t t) -> color_u32 {
		float f = 1.0f - (static_cast<float>(t - t0) / static_cast<float>(t1 - t0));
		return { 255, static_cast<uint8_t>((c1.c.b + (c0.c.b - c1.c.b) * f) * sv), static_cast<uint8_t>((c1.c.g + (c0.c.g - c1.c.g) * f) * sv), static_cast<uint8_t>((c1.c.r + (c0.c.r - c1.c.r) * f) * sv) };
	};

	uint32_t end_p = (uint32_t)vera_video_get_scan_pos_x() + (SCAN_WIDTH * vera_video_get_scan_pos_y());
	if (waiting) {
		const color_u32 final_color = shade_color(vis_color);
		if (end_p < Last_p) {
			for (uint32_t p = Last_p; p < SCAN_WIDTH * SCAN_HEIGHT; ++p) {
				Framebuffer[p] = final_color.u;
			}
			for (uint32_t p = 0; p < end_p; ++p) {
				Framebuffer[p] = final_color.u;
			}
		} else {
			for (uint32_t p = Last_p; p < end_p; ++p) {
				Framebuffer[p] = final_color.u;
			}
		}
	} else {
		if (end_p < Last_p) {
			uint32_t len = (SCAN_WIDTH * SCAN_HEIGHT - Last_p) + end_p;
			for (uint32_t p = Last_p; p < SCAN_WIDTH * SCAN_HEIGHT; ++p) {
				Framebuffer[p] = lerp_colors(vis_color, end_color, Last_p, Last_p + len, p).u;
			}
			for (uint32_t p = 0; p < end_p; ++p) {
				Framebuffer[p] = lerp_colors(vis_color, end_color, 0, end_p, p).u;
			}
		} else {
			for (uint32_t p = Last_p; p < end_p; ++p) {
				Framebuffer[p] = lerp_colors(vis_color, end_color, Last_p, end_p, p).u;
			}
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
