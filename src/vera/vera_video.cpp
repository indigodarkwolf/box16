// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2020 Frank van den Hoef
// Copyright (c) 2021-2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include "vera_video.h"

#include "vera_pcm.h"
#include "vera_psg.h"
#include "vera_spi.h"

#include <limits.h>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#	include "emscripten.h"
#endif

#define ADDR_VRAM_START 0x00000
#define ADDR_VRAM_END 0x20000
#define ADDR_PSG_START 0x1F9C0
#define ADDR_PSG_END 0x1FA00
#define ADDR_PALETTE_START 0x1FA00
#define ADDR_PALETTE_END 0x1FC00
#define ADDR_SPRDATA_START 0x1FC00
#define ADDR_SPRDATA_END 0x20000

#define NUM_SPRITES 128

// both VGA and NTSC
#define SCAN_HEIGHT 525
#define PIXEL_FREQ 25.0f

// VGA
#define VGA_SCAN_WIDTH 800
#define VGA_X_OFFSET 0
#define VGA_Y_OFFSET 0

// NTSC: 262.5 lines per frame, lower field first
#define NTSC_HALF_SCAN_WIDTH 794
#define NTSC_X_OFFSET 270
#define NTSC_Y_OFFSET_LOW 42
#define NTSC_Y_OFFSET_HIGH 568
#define TITLE_SAFE_X 0.067
#define TITLE_SAFE_Y 0.05

#define SCREEN_RAM_OFFSET 0x00000

// When rendering a layer line, we can amortize some of the cost by calculating multiple pixels at a time.
#define LAYER_PIXELS_PER_ITERATION 8

static bool is_fullscreen = false;

static uint8_t video_ram[0x20000];
static uint8_t palette[256 * 2];
static uint8_t sprite_data[128][8];

// I/O registers
static uint32_t io_addr[2];
static uint8_t  io_rddata[2];
static uint8_t  io_inc[2];
static uint8_t  io_addrsel;
static uint8_t  io_dcsel;

static uint8_t ien;
static uint8_t isr;

static uint16_t irq_line;

static uint8_t reg_layer[2][7];
static uint8_t reg_composer[8];

static uint8_t layer_line[2][SCREEN_WIDTH];
static uint8_t sprite_line_col[SCREEN_WIDTH];
static uint8_t sprite_line_z[SCREEN_WIDTH];
static uint8_t sprite_line_mask[SCREEN_WIDTH];
static uint8_t sprite_line_collisions;
static bool    layer_line_enable[2];
static bool    sprite_line_enable;

static float    vga_scan_pos_x;
static uint16_t vga_scan_pos_y;
static float    ntsc_half_cnt;
static uint16_t ntsc_scan_pos_y;

static int frame_count = 0;
static int cheat_mask  = 0;

static bool log_video           = false;
static bool shadow_safety_frame = false;

static uint8_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT * 4];

static const uint16_t default_palette[] = {
	0x000, 0xfff, 0x800, 0xafe, 0xc4c, 0x0c5, 0x00a, 0xee7, 0xd85, 0x640, 0xf77, 0x333, 0x777, 0xaf6, 0x08f, 0xbbb, 0x000, 0x111, 0x222, 0x333, 0x444, 0x555, 0x666, 0x777, 0x888, 0x999, 0xaaa, 0xbbb, 0xccc, 0xddd, 0xeee, 0xfff, 0x211, 0x433, 0x644, 0x866, 0xa88, 0xc99, 0xfbb, 0x211, 0x422, 0x633, 0x844, 0xa55, 0xc66, 0xf77, 0x200, 0x411, 0x611, 0x822, 0xa22, 0xc33, 0xf33, 0x200, 0x400, 0x600, 0x800, 0xa00, 0xc00, 0xf00, 0x221, 0x443, 0x664, 0x886, 0xaa8, 0xcc9, 0xfeb, 0x211, 0x432, 0x653, 0x874, 0xa95, 0xcb6, 0xfd7, 0x210, 0x431, 0x651, 0x862, 0xa82, 0xca3, 0xfc3, 0x210, 0x430, 0x640, 0x860, 0xa80, 0xc90, 0xfb0, 0x121, 0x343, 0x564, 0x786, 0x9a8, 0xbc9, 0xdfb, 0x121, 0x342, 0x463, 0x684, 0x8a5, 0x9c6, 0xbf7, 0x120, 0x241, 0x461, 0x582, 0x6a2, 0x8c3, 0x9f3, 0x120, 0x240, 0x360, 0x480, 0x5a0, 0x6c0, 0x7f0, 0x121, 0x343, 0x465, 0x686, 0x8a8, 0x9ca, 0xbfc, 0x121, 0x242, 0x364, 0x485, 0x5a6, 0x6c8, 0x7f9, 0x020, 0x141, 0x162, 0x283, 0x2a4, 0x3c5, 0x3f6, 0x020, 0x041, 0x061, 0x082, 0x0a2, 0x0c3, 0x0f3, 0x122, 0x344, 0x466, 0x688, 0x8aa, 0x9cc, 0xbff, 0x122, 0x244, 0x366, 0x488, 0x5aa, 0x6cc, 0x7ff, 0x022, 0x144, 0x166, 0x288, 0x2aa, 0x3cc, 0x3ff, 0x022, 0x044, 0x066, 0x088, 0x0aa, 0x0cc, 0x0ff, 0x112, 0x334, 0x456, 0x668, 0x88a, 0x9ac, 0xbcf, 0x112, 0x224, 0x346, 0x458, 0x56a, 0x68c, 0x79f, 0x002, 0x114, 0x126, 0x238, 0x24a, 0x35c, 0x36f, 0x002, 0x014, 0x016, 0x028, 0x02a, 0x03c, 0x03f, 0x112, 0x334, 0x546, 0x768, 0x98a, 0xb9c, 0xdbf, 0x112, 0x324, 0x436, 0x648, 0x85a, 0x96c, 0xb7f, 0x102, 0x214, 0x416, 0x528, 0x62a, 0x83c, 0x93f, 0x102, 0x204, 0x306, 0x408, 0x50a, 0x60c, 0x70f, 0x212, 0x434, 0x646, 0x868, 0xa8a, 0xc9c, 0xfbe, 0x211, 0x423, 0x635, 0x847, 0xa59, 0xc6b, 0xf7d, 0x201, 0x413, 0x615, 0x826, 0xa28, 0xc3a, 0xf3c, 0x201, 0x403, 0x604, 0x806, 0xa08, 0xc09, 0xf0b
};

static void refresh_palette();

void vera_video_reset()
{
	// init I/O registers
	memset(io_addr, 0, sizeof(io_addr));
	memset(io_inc, 0, sizeof(io_inc));
	io_addrsel   = 0;
	io_dcsel     = 0;
	io_rddata[0] = 0;
	io_rddata[1] = 0;

	ien      = 0;
	isr      = 0;
	irq_line = 0;

	// init Layer registers
	memset(reg_layer, 0, sizeof(reg_layer));

	// init composer registers
	memset(reg_composer, 0, sizeof(reg_composer));
	reg_composer[1] = 128; // hscale = 1.0
	reg_composer[2] = 128; // vscale = 1.0
	reg_composer[5] = 640 >> 2;
	reg_composer[7] = 480 >> 1;

	// init sprite data
	memset(sprite_data, 0, sizeof(sprite_data));

	// copy palette
	memcpy(palette, default_palette, sizeof(palette));
	for (int i = 0; i < 256; i++) {
		palette[i * 2 + 0] = default_palette[i] & 0xff;
		palette[i * 2 + 1] = default_palette[i] >> 8;
	}

	refresh_palette();

	// fill video RAM with random data
	for (int i = 0; i < 128 * 1024; i++) {
		video_ram[i] = rand();
	}

	sprite_line_collisions = 0;

	vga_scan_pos_x  = 0;
	vga_scan_pos_y  = 0;
	ntsc_half_cnt   = 0;
	ntsc_scan_pos_y = 0;

	psg_reset();
	pcm_reset();
}

struct vera_video_layer_properties layer_properties[2];

static int calc_layer_eff_x(const struct vera_video_layer_properties *props, const int x)
{
	return (x + props->hscroll) & (props->layerw_max);
}

static int calc_layer_eff_y(const struct vera_video_layer_properties *props, const int y)
{
	return (y + props->vscroll) & (props->layerh_max);
}

static uint32_t calc_layer_map_offset_base2(const struct vera_video_layer_properties *props, const int eff_x)
{
	// Slightly faster on some platforms because we know that tilew and tileh are powers of 2.
	return ((eff_x >> props->tilew_log2) & props->mapw_max) << 1;
}

//TODO: Unused in all current cases. Delete? Or leave commented as a reminder?
//static uint32_t
//calc_layer_map_addr(struct video_layer_properties *props, int eff_x, int eff_y)
//{
//	return props->map_base + ((eff_y / props->tileh) * props->mapw + (eff_x / props->tilew)) * 2;
//}
static void refresh_layer_properties(const uint8_t layer)
{
	struct vera_video_layer_properties *props = &layer_properties[layer];

	uint16_t prev_layerw_max = props->layerw_max;
	uint16_t prev_hscroll    = props->hscroll;

	props->color_depth    = reg_layer[layer][0] & 0x3;
	props->map_base       = reg_layer[layer][1] << 9;
	props->tile_base      = (reg_layer[layer][2] & 0xFC) << 9;
	props->bitmap_mode    = (reg_layer[layer][0] & 0x4) != 0;
	props->text_mode      = (props->color_depth == 0) && !props->bitmap_mode;
	props->text_mode_256c = (reg_layer[layer][0] & 8) != 0;
	props->tile_mode      = !props->bitmap_mode && !props->text_mode;

	if (!props->bitmap_mode) {
		props->hscroll = reg_layer[layer][3] | (reg_layer[layer][4] & 0xf) << 8;
		props->vscroll = reg_layer[layer][5] | (reg_layer[layer][6] & 0xf) << 8;
	} else {
		props->hscroll = 0;
		props->vscroll = 0;
	}

	uint16_t mapw = 0;
	uint16_t maph = 0;
	props->tilew  = 0;
	props->tileh  = 0;

	if (props->tile_mode || props->text_mode) {
		props->mapw_log2 = 5 + ((reg_layer[layer][0] >> 4) & 3);
		props->maph_log2 = 5 + ((reg_layer[layer][0] >> 6) & 3);
		mapw             = 1 << props->mapw_log2;
		maph             = 1 << props->maph_log2;

		// Scale the tiles or text characters according to TILEW and TILEH.
		props->tilew_log2 = 3 + (reg_layer[layer][2] & 1);
		props->tileh_log2 = 3 + ((reg_layer[layer][2] >> 1) & 1);
		props->tilew      = 1 << props->tilew_log2;
		props->tileh      = 1 << props->tileh_log2;
	} else if (props->bitmap_mode) {
		// bitmap mode is basically tiled mode with a single huge tile
		props->tilew = (reg_layer[layer][2] & 1) ? 640 : 320;
		props->tileh = SCREEN_HEIGHT;
	}

	// We know mapw, maph, tilew, and tileh are powers of two in all cases except bitmap modes, and any products of that set will be powers of two,
	// so there's no need to modulo against them if we have bitmasks we can bitwise-and against.

	props->mapw_max   = mapw - 1;
	props->maph_max   = maph - 1;
	props->tilew_max  = props->tilew - 1;
	props->tileh_max  = props->tileh - 1;
	props->layerw_max = (mapw * props->tilew) - 1;
	props->layerh_max = (maph * props->tileh) - 1;

	props->bits_per_pixel = 1 << props->color_depth;
	props->tile_size_log2 = props->tilew_log2 + props->tileh_log2 + props->color_depth - 3;

	props->first_color_pos  = 8 - props->bits_per_pixel;
	props->color_mask       = (1 << props->bits_per_pixel) - 1;
	props->color_fields_max = (8 >> props->color_depth) - 1;
}

vera_video_sprite_properties sprite_properties[128];

static void refresh_sprite_properties(const uint16_t sprite)
{
	struct vera_video_sprite_properties *props = &sprite_properties[sprite];

	props->sprite_zdepth         = (sprite_data[sprite][6] >> 2) & 3;
	props->sprite_collision_mask = sprite_data[sprite][6] & 0xf0;

	props->sprite_x           = sprite_data[sprite][2] | (sprite_data[sprite][3] & 3) << 8;
	props->sprite_y           = sprite_data[sprite][4] | (sprite_data[sprite][5] & 3) << 8;
	props->sprite_width_log2  = (((sprite_data[sprite][7] >> 4) & 3) + 3);
	props->sprite_height_log2 = ((sprite_data[sprite][7] >> 6) + 3);
	props->sprite_width       = 1 << props->sprite_width_log2;
	props->sprite_height      = 1 << props->sprite_height_log2;

	// fix up negative coordinates
	if (props->sprite_x >= 0x400 - props->sprite_width) {
		props->sprite_x -= 0x400;
	}
	if (props->sprite_y >= 0x400 - props->sprite_height) {
		props->sprite_y -= 0x400;
	}

	props->hflip = sprite_data[sprite][6] & 1;
	props->vflip = (sprite_data[sprite][6] >> 1) & 1;

	props->color_mode     = (sprite_data[sprite][1] >> 7) & 1;
	props->sprite_address = sprite_data[sprite][0] << 5 | (sprite_data[sprite][1] & 0xf) << 13;

	props->palette_offset = (sprite_data[sprite][7] & 0x0f) << 4;
}

struct video_palette_props {
	uint32_t entries[256];
	bool     dirty;
};

struct video_palette_props video_palette;

static void refresh_palette()
{
	const uint8_t out_mode       = reg_composer[0] & 3;
	const bool    chroma_disable = (reg_composer[0] >> 2) & 1;
	for (int i = 0; i < 256; ++i) {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		if (out_mode == 0) {
			// video generation off
			// -> show blue screen
			r = 0;
			g = 0;
			b = 255;
		} else {
			uint16_t entry = palette[i * 2] | palette[i * 2 + 1] << 8;
			r              = ((entry >> 8) & 0xf) << 4 | ((entry >> 8) & 0xf);
			g              = ((entry >> 4) & 0xf) << 4 | ((entry >> 4) & 0xf);
			b              = (entry & 0xf) << 4 | (entry & 0xf);
			if (chroma_disable) {
				r = g = b = (r + b + g) / 3;
			}
		}

		video_palette.entries[i] = 0xff000000 | (uint32_t)(r << 16) | ((uint32_t)g << 8) | ((uint32_t)b);
	}
	video_palette.dirty = false;
}

static void expand_1bpp_data(uint8_t *dst, const uint8_t *src, int dst_size)
{
	dst += 7;
	while (dst_size >= 8) {
		uint8_t s = *src;

		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;
		--dst;
		s >>= 1;
		*dst = s & 0x1;

		dst += 15;
		++src;
		dst_size -= 8;
	}
}

static void expand_2bpp_data(uint8_t *dst, const uint8_t *src, int dst_size)
{
	dst += 3;
	while (dst_size >= 4) {
		uint8_t s = *src;

		*dst = s & 0x3;
		--dst;
		s >>= 2;
		*dst = s & 0x3;
		--dst;
		s >>= 2;
		*dst = s & 0x3;
		--dst;
		s >>= 2;
		*dst = s & 0x3;

		dst += 7;
		++src;
		dst_size -= 4;
	}
}

static void expand_4bpp_data(uint8_t *dst, const uint8_t *src, int dst_size)
{
	while (dst_size >= 2) {
		*dst = (*src) >> 4;
		++dst;
		*dst = (*src) & 0xf;
		++dst;

		++src;
		dst_size -= 2;
	}
}

static void render_sprite_line(const uint16_t y)
{
	memset(sprite_line_col, 0, SCREEN_WIDTH);
	memset(sprite_line_z, 0, SCREEN_WIDTH);
	memset(sprite_line_mask, 0, SCREEN_WIDTH);

	uint16_t sprite_budget = 800 + 1;
	for (int i = 0; i < NUM_SPRITES; i++) {
		// one clock per lookup
		sprite_budget--;
		if (sprite_budget == 0)
			break;
		const vera_video_sprite_properties *props = &sprite_properties[i];

		if (props->sprite_zdepth == 0) {
			continue;
		}

		// check whether this line falls within the sprite
		if (y < props->sprite_y || y >= props->sprite_y + props->sprite_height) {
			continue;
		}

		const uint16_t eff_sy = props->vflip ? ((props->sprite_height - 1) - (y - props->sprite_y)) : (y - props->sprite_y);

		const uint8_t *bitmap_data = video_ram + props->sprite_address + (eff_sy << (props->sprite_width_log2 - (1 - props->color_mode)));

		const uint16_t width = std::min((uint32_t)props->sprite_width, (uint32_t)64);
		uint8_t        unpacked_sprite_line[64];
		if (props->color_mode == 0) {
			// 4bpp
			expand_4bpp_data(unpacked_sprite_line, bitmap_data, width);
		} else {
			// 8bpp
			memcpy(unpacked_sprite_line, bitmap_data, width);
		}

		const int32_t scale          = reg_composer[1];
		const int16_t scaled_x_start = ((int32_t)props->sprite_x << 7) / scale;
		const int16_t scaled_x_end   = scaled_x_start + (((int32_t)width << 7) / scale);
		const bool hflip = props->hflip;
		for (int16_t sx = scaled_x_start; sx < scaled_x_end; sx += 1) {
			if ((uint16_t)sx >= SCREEN_WIDTH) {
				continue;
			}

			const uint16_t x = ((sx - scaled_x_start) * scale) >> 7;

			// one clock per fetched 32 bits
			if (!(x & 3)) {
				sprite_budget--;
				if (sprite_budget == 0)
					break;
			}

			// one clock per rendered pixel
			sprite_budget--;
			if (sprite_budget == 0)
				break;

			const uint8_t col_index = unpacked_sprite_line[hflip ? width - x - 1: x];

			// palette offset
			if (col_index > 0) {
				sprite_line_collisions |= sprite_line_mask[sx] & props->sprite_collision_mask;
				sprite_line_mask[sx] |= props->sprite_collision_mask;

				if (props->sprite_zdepth > sprite_line_z[sx]) {
					sprite_line_col[sx] = col_index + props->palette_offset;
					sprite_line_z[sx]   = props->sprite_zdepth;
				}
			}
		}
	}
}

static void render_layer_line_text(uint8_t layer, uint16_t y)
{
	const struct vera_video_layer_properties *props = &layer_properties[layer];

	const uint8_t max_pixels_per_byte = 7; // (8 >> props->color_depth) - 1; // Don't need this calculation, because props->color_depth will always be 0.
	const int     eff_y               = calc_layer_eff_y(props, y);
	const int     yy                  = eff_y & props->tileh_max;

	// additional bytes to reach the correct line of the tile
	const uint32_t y_add = (yy << props->tilew_log2) >> 3;

	uint8_t tile_bytes[512]; // max 256 tiles, 2 bytes each.
	vera_video_space_read_range(tile_bytes, props->map_base + ((eff_y >> props->tileh_log2) << (props->mapw_log2 + 1)), 2 << props->mapw_log2);

	uint32_t tile_start;

	uint8_t fg_color;
	uint8_t bg_color;
	uint8_t s;
	uint8_t color_shift;

	{
		const int eff_x = calc_layer_eff_x(props, 0);
		const int xx    = eff_x & props->tilew_max;

		// extract all information from the map
		const uint32_t map_addr = calc_layer_map_offset_base2(props, eff_x);

		const uint8_t tile_index = tile_bytes[map_addr];
		const uint8_t byte1      = tile_bytes[map_addr + 1];

		if (!props->text_mode_256c) {
			fg_color = byte1 & 15;
			bg_color = byte1 >> 4;
		} else {
			fg_color = byte1;
			bg_color = 0;
		}

		// offset within tilemap of the current tile
		tile_start = tile_index << props->tile_size_log2;

		// additional bytes to reach the correct column of the tile
		const uint16_t x_add       = xx >> 3;
		const uint32_t tile_offset = tile_start + y_add + x_add;

		s = vera_video_space_read(props->tile_base + tile_offset);
	}

	// Render tile line.
	const uint32_t scale      = reg_composer[1];
	uint32_t       scaled_x   = 0;
	int            last_eff_x = calc_layer_eff_x(props, 0);

	for (int i = 0; i < SCREEN_WIDTH; i++) {
		const uint16_t x = scaled_x >> 7;

		// Scrolling
		const int eff_x = calc_layer_eff_x(props, x);
		const int xx    = eff_x & props->tilew_max;

		if ((eff_x ^ last_eff_x) & ~0x7) {
			if ((eff_x ^ last_eff_x) & ~props->tilew_max) {
				// extract all information from the map
				const uint32_t map_addr = calc_layer_map_offset_base2(props, eff_x);

				const uint8_t tile_index = tile_bytes[map_addr];
				const uint8_t byte1      = tile_bytes[map_addr + 1];

				if (!props->text_mode_256c) {
					fg_color = byte1 & 15;
					bg_color = byte1 >> 4;
				} else {
					fg_color = byte1;
					bg_color = 0;
				}

				// offset within tilemap of the current tile
				tile_start = tile_index << props->tile_size_log2;
			}

			// additional bytes to reach the correct column of the tile
			const uint16_t x_add       = xx >> 3;
			const uint32_t tile_offset = tile_start + y_add + x_add;

			s = vera_video_space_read(props->tile_base + tile_offset);
		}

		// convert tile byte to indexed color
		color_shift             = max_pixels_per_byte - (eff_x & 0x7);
		const uint8_t col_index = (s >> color_shift) & 1;
		layer_line[layer][i]    = col_index ? fg_color : bg_color;

		scaled_x += scale;
		last_eff_x = eff_x;
	}
}

static void render_layer_line_tile(uint8_t layer, uint16_t y)
{
	struct vera_video_layer_properties *props = &layer_properties[layer];

	const uint8_t  max_pixels_per_byte = (8 >> props->color_depth) - 1;
	const int      eff_y               = calc_layer_eff_y(props, y);
	const uint8_t  yy                  = eff_y & props->tileh_max;
	const uint8_t  yy_flip             = yy ^ props->tileh_max;
	const uint32_t y_add               = (yy << (props->tilew_log2 + props->color_depth - 3));
	const uint32_t y_add_flip          = (yy_flip << (props->tilew_log2 + props->color_depth - 3));

	uint8_t tile_bytes[512]; // max 256 tiles, 2 bytes each.
	vera_video_space_read_range(tile_bytes, props->map_base + ((eff_y >> props->tileh_log2) << (props->mapw_log2 + 1)), 2 << props->mapw_log2);

	uint8_t  palette_offset;
	bool     vflip;
	bool     hflip;
	uint32_t tile_start;
	uint8_t  s;
	uint8_t  color_shift;

	{
		const int eff_x = calc_layer_eff_x(props, 0);

		// extract all information from the map
		const uint32_t map_addr = calc_layer_map_offset_base2(props, eff_x);

		const uint8_t byte0 = tile_bytes[map_addr];
		const uint8_t byte1 = tile_bytes[map_addr + 1];

		// Tile Flipping
		vflip = (byte1 >> 3) & 1;
		hflip = (byte1 >> 2) & 1;

		palette_offset = byte1 & 0xf0;

		// offset within tilemap of the current tile
		const uint16_t tile_index = byte0 | ((byte1 & 3) << 8);
		tile_start                = tile_index << props->tile_size_log2;

		int xx = eff_x & props->tilew_max;
		if (hflip) {
			xx = xx ^ (props->tilew_max);
		}

		// additional bytes to reach the correct column of the tile
		uint16_t x_add       = (xx << props->color_depth) >> 3;
		uint32_t tile_offset = tile_start + (vflip ? y_add_flip : y_add) + x_add;

		s = vera_video_space_read(props->tile_base + tile_offset);
	}

	// Render tile line.
	const uint32_t scale      = reg_composer[1];
	uint32_t       scaled_x   = 0;
	int            last_eff_x = calc_layer_eff_x(props, 0);

	for (int i = 0; i < SCREEN_WIDTH; i++) {
		const uint16_t x     = scaled_x >> 7;
		const int      eff_x = calc_layer_eff_x(props, x);

		if ((eff_x ^ last_eff_x) & ~max_pixels_per_byte) {
			if ((eff_x ^ last_eff_x) & ~props->tilew_max) {
				// extract all information from the map
				const uint32_t map_addr = calc_layer_map_offset_base2(props, eff_x);

				const uint8_t byte0 = tile_bytes[map_addr];
				const uint8_t byte1 = tile_bytes[map_addr + 1];

				// Tile Flipping
				vflip = (byte1 >> 3) & 1;
				hflip = (byte1 >> 2) & 1;

				palette_offset = byte1 & 0xf0;

				// offset within tilemap of the current tile
				const uint16_t tile_index = byte0 | ((byte1 & 3) << 8);
				tile_start                = tile_index << props->tile_size_log2;
			}

			int xx = eff_x & props->tilew_max;
			if (hflip) {
				xx = xx ^ (props->tilew_max);
			}

			// additional bytes to reach the correct column of the tile
			const uint16_t x_add       = (xx << props->color_depth) >> 3;
			const uint32_t tile_offset = tile_start + (vflip ? y_add_flip : y_add) + x_add;

			s = vera_video_space_read(props->tile_base + tile_offset);
		}

		if (hflip) {
			color_shift = (eff_x & max_pixels_per_byte) << props->color_depth;
		} else {
			color_shift = props->first_color_pos - ((eff_x & max_pixels_per_byte) << props->color_depth);
		}
		// convert tile byte to indexed color
		uint8_t col_index = (s >> color_shift) & props->color_mask;

		// Apply Palette Offset
		if (palette_offset && col_index > 0 && col_index < 16) {
			col_index += palette_offset;
		}
		layer_line[layer][i] = col_index;

		scaled_x += scale;
		last_eff_x = eff_x;
	}
}

static void render_layer_line_bitmap(uint8_t layer, uint16_t y)
{
	struct vera_video_layer_properties *props = &layer_properties[layer];

	int yy = y % props->tileh;
	// additional bytes to reach the correct line of the tile
	uint32_t y_add = (yy * props->tilew * props->bits_per_pixel) >> 3;

	// Render tile line.
	const uint32_t scale    = reg_composer[1];
	uint32_t       scaled_x = 0;
	for (int i = 0; i < SCREEN_WIDTH; i++) {
		const uint16_t x  = scaled_x >> 7;
		int            xx = x % props->tilew;

		// extract all information from the map
		uint8_t palette_offset = reg_layer[layer][4] & 0xf;

		// additional bytes to reach the correct column of the tile
		uint16_t x_add       = (xx * props->bits_per_pixel) >> 3;
		uint32_t tile_offset = y_add + x_add;
		uint8_t  s           = vera_video_space_read(props->tile_base + tile_offset);

		// convert tile byte to indexed color
		uint8_t col_index = (s >> (props->first_color_pos - ((xx & props->color_fields_max) << props->color_depth))) & props->color_mask;

		// Apply Palette Offset
		if (palette_offset && col_index > 0 && col_index < 16) {
			col_index += palette_offset << 4;
		}
		layer_line[layer][i] = col_index;

		scaled_x += scale;
	}
}

static uint8_t calculate_line_col_index(uint8_t spr_zindex, uint8_t spr_col_index, uint8_t l1_col_index, uint8_t l2_col_index)
{
	uint8_t col_index = 0;
	switch (spr_zindex) {
		case 3:
			col_index = spr_col_index ? spr_col_index : (l2_col_index ? l2_col_index : l1_col_index);
			break;
		case 2:
			col_index = l2_col_index ? l2_col_index : (spr_col_index ? spr_col_index : l1_col_index);
			break;
		case 1:
			col_index = l2_col_index ? l2_col_index : (l1_col_index ? l1_col_index : spr_col_index);
			break;
		case 0:
			col_index = l2_col_index ? l2_col_index : l1_col_index;
			break;
	}
	return col_index;
}

static void render_line(uint16_t y)
{
	if (y >= SCREEN_HEIGHT) {
		return;
	}

	uint8_t out_mode = reg_composer[0] & 3;

	uint8_t  border_color = reg_composer[3];
	uint16_t hstart       = reg_composer[4] << 2;
	uint16_t hstop        = reg_composer[5] << 2;
	uint16_t vstart       = reg_composer[6] << 1;
	uint16_t vstop        = reg_composer[7] << 1;

	int eff_y = (reg_composer[2] * (y - vstart)) >> 7;

	uint8_t dc_video = reg_composer[0];

	const bool layer0_was_enabled = layer_line_enable[0];
	const bool layer1_was_enabled = layer_line_enable[1];
	const bool sprite_was_enabled = sprite_line_enable;

	layer_line_enable[0] = dc_video & 0x10;
	layer_line_enable[1] = dc_video & 0x20;
	sprite_line_enable   = dc_video & 0x40;

	if (sprite_line_enable) {
		render_sprite_line(eff_y);
	} else if (sprite_was_enabled) {
		memset(sprite_line_z, 0, SCREEN_WIDTH);
		memset(sprite_line_col, 0, SCREEN_WIDTH);
	}

	if (vera_video_is_cheat_frame()) {
		// sprites were needed for the collision IRQ, but we can skip
		// everything else if we're cheating and not actually updating.
		return;
	}

	if (layer_line_enable[0]) {
		if (layer_properties[0].text_mode) {
			render_layer_line_text(0, eff_y);
		} else if (layer_properties[0].bitmap_mode) {
			render_layer_line_bitmap(0, eff_y);
		} else {
			render_layer_line_tile(0, eff_y);
		}
	} else if (layer0_was_enabled) {
		memset(layer_line[0], 0, SCREEN_WIDTH);
	}

	if (layer_line_enable[1]) {
		if (layer_properties[1].text_mode) {
			render_layer_line_text(1, eff_y);
		} else if (layer_properties[1].bitmap_mode) {
			render_layer_line_bitmap(1, eff_y);
		} else {
			render_layer_line_tile(1, eff_y);
		}
	} else if (layer1_was_enabled) {
		memset(layer_line[1], 0, SCREEN_WIDTH);
	}

	uint8_t col_line[SCREEN_WIDTH];

	if (video_palette.dirty) {
		refresh_palette();
	}

	// If video output is enabled, calculate color indices for line.
	if (out_mode != 0) {
		// Add border after if required.
		if (y < vstart || y > vstop) {
			uint32_t border_fill = border_color;
			border_fill          = border_fill | (border_fill << 8);
			border_fill          = border_fill | (border_fill << 16);
			memset(col_line, border_fill, SCREEN_WIDTH);
		} else {
			hstart = hstart < 640 ? hstart : 640;
			hstop  = hstop < 640 ? hstop : 640;

			for (uint16_t x = 0; x < hstart; ++x) {
				col_line[x] = border_color;
			}
			uint16_t hwidth = hstop - hstart;
			for (uint16_t x = 0; x < hwidth; ++x) {
				col_line[hstart + x] = calculate_line_col_index(sprite_line_z[x], sprite_line_col[x], layer_line[0][x], layer_line[1][x]);
			}
			for (uint16_t x = hstop; x < SCREEN_WIDTH; ++x) {
				col_line[x] = border_color;
			}
		}
	}

	// Look up all color indices.
	uint32_t *const framebuffer4_begin = ((uint32_t *)framebuffer) + (y * SCREEN_WIDTH);
	{
		uint32_t *framebuffer4 = framebuffer4_begin;
		for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
			*framebuffer4++ = video_palette.entries[col_line[x]];
		}
	}

	// NTSC overscan
	if (out_mode == 2 || shadow_safety_frame) {
		uint32_t *framebuffer4 = framebuffer4_begin;
		for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
			if (x < SCREEN_WIDTH * TITLE_SAFE_X ||
			    x > SCREEN_WIDTH * (1 - TITLE_SAFE_X) ||
			    y < SCREEN_HEIGHT * TITLE_SAFE_Y ||
			    y > SCREEN_HEIGHT * (1 - TITLE_SAFE_Y)) {

				// Divide RGB elements by 4.
				*framebuffer4 &= 0x00fcfcfc;
				*framebuffer4 >>= 2;
			}
			framebuffer4++;
		}
	}
}

static void update_isr_and_coll(uint16_t y, uint16_t compare)
{
	if (y == SCREEN_HEIGHT) {
		if (ien & 4) {
			if (sprite_line_collisions != 0) {
				isr |= 4;
			}
			isr = (isr & 0xf) | sprite_line_collisions;
		}
		sprite_line_collisions = 0;
		if (ien & 1) { // VSYNC IRQ
			isr |= 1;
		}
	}
	if ((ien & 2) && (y < SCREEN_HEIGHT) && (y == compare)) { // LINE IRQ
		isr |= 2;
	}
}

bool vera_video_step(float mhz, float steps)
{
	uint16_t y         = 0;
	bool     ntsc_mode = reg_composer[0] & 2;
	bool     new_frame = false;
	vga_scan_pos_x += PIXEL_FREQ * steps / mhz;
	if (vga_scan_pos_x > VGA_SCAN_WIDTH) {
		vga_scan_pos_x -= VGA_SCAN_WIDTH;
		if (!ntsc_mode) {
			render_line(vga_scan_pos_y - VGA_Y_OFFSET);
		}
		vga_scan_pos_y++;
		if (vga_scan_pos_y == SCAN_HEIGHT) {
			vga_scan_pos_y = 0;
			if (!ntsc_mode) {
				new_frame = true;
				frame_count++;
			}
		}
		if (!ntsc_mode) {
			update_isr_and_coll(vga_scan_pos_y - VGA_Y_OFFSET, irq_line);
		}
	}
	ntsc_half_cnt += PIXEL_FREQ * steps / mhz;
	if (ntsc_half_cnt > NTSC_HALF_SCAN_WIDTH) {
		ntsc_half_cnt -= NTSC_HALF_SCAN_WIDTH;
		if (ntsc_mode) {
			if (ntsc_scan_pos_y < SCAN_HEIGHT) {
				y = ntsc_scan_pos_y - NTSC_Y_OFFSET_LOW;
				if ((y & 1) == 0) {
					render_line(y);
				}
			} else {
				y = ntsc_scan_pos_y - NTSC_Y_OFFSET_HIGH;
				if ((y & 1) == 0) {
					render_line(y | 1);
				}
			}
		}
		ntsc_scan_pos_y++;
		if (ntsc_scan_pos_y == SCAN_HEIGHT) {
			reg_composer[0] |= 0x80;
			if (ntsc_mode) {
				new_frame = true;
				frame_count++;
			}
		}
		if (ntsc_scan_pos_y == SCAN_HEIGHT * 2) {
			reg_composer[0] &= ~0x80;
			ntsc_scan_pos_y = 0;
			if (ntsc_mode) {
				new_frame = true;
				frame_count++;
			}
		}
		if (ntsc_mode) {
			// this is correct enough for even screen heights
			if (ntsc_scan_pos_y < SCAN_HEIGHT) {
				update_isr_and_coll(ntsc_scan_pos_y - NTSC_Y_OFFSET_LOW, irq_line & ~1);
			} else {
				update_isr_and_coll(ntsc_scan_pos_y - NTSC_Y_OFFSET_HIGH, irq_line & ~1);
			}
		}
	}

	return new_frame;
}

void vera_video_force_redraw_screen()
{
	const uint8_t old_sprite_line_collisions = sprite_line_collisions;

	for (int y = 0; y < SCREEN_HEIGHT; ++y) {
		render_line(y);
	}

	sprite_line_collisions = old_sprite_line_collisions;
}

bool vera_video_get_irq_out()
{
	uint8_t tmp_isr = isr | (pcm_is_fifo_almost_empty() ? 8 : 0);
	return (tmp_isr & ien) != 0;
}

//
// saves the video memory and register content into a file
//

void vera_video_save(SDL_RWops *f)
{
	SDL_RWwrite(f, &video_ram[0], sizeof(uint8_t), sizeof(video_ram));
	SDL_RWwrite(f, &reg_composer[0], sizeof(uint8_t), sizeof(reg_composer));
	SDL_RWwrite(f, &palette[0], sizeof(uint8_t), sizeof(palette));
	SDL_RWwrite(f, &reg_layer[0][0], sizeof(uint8_t), sizeof(reg_layer));
	SDL_RWwrite(f, &sprite_data[0], sizeof(uint8_t), sizeof(sprite_data));
}

static const int increments[32] = {
	0,
	0,
	1,
	-1,
	2,
	-2,
	4,
	-4,
	8,
	-8,
	16,
	-16,
	32,
	-32,
	64,
	-64,
	128,
	-128,
	256,
	-256,
	512,
	-512,
	40,
	-40,
	80,
	-80,
	160,
	-160,
	320,
	-320,
	640,
	-640,
};

static uint32_t get_and_inc_address(uint8_t sel)
{
	uint32_t address = io_addr[sel];
	io_addr[sel] += increments[io_inc[sel]];
	return address;
}

//
// Vera: Internal Video Address Space
//

uint8_t vera_video_space_read(uint32_t address)
{
	return video_ram[address & 0x1FFFF];
}

void vera_video_space_read_range(uint8_t *dest, uint32_t address, uint32_t size)
{
	address &= 0x1FFFF;
	if (address >= ADDR_VRAM_START && (address + size) <= ADDR_VRAM_END) {
		memcpy(dest, &video_ram[address], size);
	} else {
		const uint32_t tail_size = ADDR_VRAM_END - address;
		memcpy(dest, &video_ram[address], tail_size);
		const uint32_t head_size = ((address + size) & 0x1FFFF);
		memcpy(dest + tail_size, video_ram, head_size);
	}
}

void vera_video_space_write(uint32_t address, uint8_t value)
{
	video_ram[address & 0x1FFFF] = value;

	if (address >= ADDR_PSG_START && address < ADDR_PSG_END) {
		psg_writereg(address & 0x3f, value);
	} else if (address >= ADDR_PALETTE_START && address < ADDR_PALETTE_END) {
		palette[address & 0x1ff] = value;
		video_palette.dirty      = true;
	} else if (address >= ADDR_SPRDATA_START && address < ADDR_SPRDATA_END) {
		sprite_data[(address >> 3) & 0x7f][address & 0x7] = value;
		refresh_sprite_properties((address >> 3) & 0x7f);
	}
}

//
// Vera: 6502 I/O Interface
//
// if debugOn, read without any side effects (registers & memory unchanged)

uint8_t vera_debug_video_read(uint8_t reg)
{
	const bool     ntsc_mode = reg_composer[0] & 2;
	const uint16_t scanline  = std::min(511, ntsc_mode ? ntsc_scan_pos_y % SCAN_HEIGHT : vga_scan_pos_y);

	switch (reg & 0x1F) {
		case 0x00: return io_addr[io_addrsel] & 0xff;
		case 0x01: return (io_addr[io_addrsel] >> 8) & 0xff;
		case 0x02: return (io_addr[io_addrsel] >> 16) | (io_inc[io_addrsel] << 3);

		case 0x03:
		case 0x04: return io_rddata[reg - 3];

		case 0x05: return (io_dcsel << 1) | io_addrsel;
		case 0x06: return ((irq_line & 0x100) >> 1) | ((scanline & 0x100) >> 2) | (ien & 0xF);
		case 0x07: return isr | (pcm_is_fifo_almost_empty() ? 8 : 0);
		case 0x08: return scanline & 0xFF;

		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C: return reg_composer[reg - 0x09 + (io_dcsel ? 4 : 0)];

		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13: return reg_layer[0][reg - 0x0D];

		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A: return reg_layer[1][reg - 0x14];

		case 0x1B: return pcm_read_ctrl();
		case 0x1C: return pcm_read_rate();
		case 0x1D: return 0;

		case 0x1E:
		case 0x1F: return debug_vera_spi_read(reg & 1);
	}
	return 0;
}

uint8_t vera_video_read(uint8_t reg)
{
	const bool     ntsc_mode = reg_composer[0] & 2;
	const uint16_t scanline  = std::min(511, ntsc_mode ? ntsc_scan_pos_y % SCAN_HEIGHT : vga_scan_pos_y);

	switch (reg & 0x1F) {
		case 0x00: return io_addr[io_addrsel] & 0xff;
		case 0x01: return (io_addr[io_addrsel] >> 8) & 0xff;
		case 0x02: return (io_addr[io_addrsel] >> 16) | (io_inc[io_addrsel] << 3);

		case 0x03:
		case 0x04: {
			uint32_t address = get_and_inc_address(reg - 3);

			uint8_t value      = io_rddata[reg - 3];
			io_rddata[reg - 3] = vera_video_space_read(io_addr[reg - 3]);

			if (log_video) {
				printf("READ  video_space[$%X] = $%02X\n", address, value);
			}
			return value;
		}
		case 0x05: return (io_dcsel << 1) | io_addrsel;
		case 0x06: return ((irq_line & 0x100) >> 1) | ((scanline & 0x100) >> 2) | (ien & 0xF);
		case 0x07: return isr | (pcm_is_fifo_almost_empty() ? 8 : 0);
		case 0x08: return scanline & 0xFF;

		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C: return reg_composer[reg - 0x09 + (io_dcsel ? 4 : 0)];

		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13: return reg_layer[0][reg - 0x0D];

		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A: return reg_layer[1][reg - 0x14];

		case 0x1B: return pcm_read_ctrl();
		case 0x1C: return pcm_read_rate();
		case 0x1D: return 0;

		case 0x1E:
		case 0x1F: return vera_spi_read(reg & 1);
	}
	return 0;
}

void vera_video_write(uint8_t reg, uint8_t value)
{
	// if (reg > 4) {
	// 	printf("ioregisters[0x%02X] = 0x%02X\n", reg, value);
	// }
	//	printf("ioregisters[%d] = $%02X\n", reg, value);
	switch (reg & 0x1F) {
		case 0x00:
			io_addr[io_addrsel]   = (io_addr[io_addrsel] & 0x1ff00) | value;
			io_rddata[io_addrsel] = vera_video_space_read(io_addr[io_addrsel]);
			break;
		case 0x01:
			io_addr[io_addrsel]   = (io_addr[io_addrsel] & 0x100ff) | (value << 8);
			io_rddata[io_addrsel] = vera_video_space_read(io_addr[io_addrsel]);
			break;
		case 0x02:
			io_addr[io_addrsel]   = (io_addr[io_addrsel] & 0x0ffff) | ((value & 0x1) << 16);
			io_inc[io_addrsel]    = value >> 3;
			io_rddata[io_addrsel] = vera_video_space_read(io_addr[io_addrsel]);
			break;
		case 0x03:
		case 0x04: {
			uint32_t address = get_and_inc_address(reg - 3);
			if (log_video) {
				printf("WRITE video_space[$%X] = $%02X\n", address, value);
			}
			vera_video_space_write(address, value);

			io_rddata[reg - 3] = vera_video_space_read(io_addr[reg - 3]);
			break;
		}
		case 0x05:
			if (value & 0x80) {
				vera_video_reset();
			}
			io_dcsel   = (value >> 1) & 1;
			io_addrsel = value & 1;
			break;
		case 0x06:
			irq_line = (irq_line & 0xFF) | ((value >> 7) << 8);
			ien      = value & 0xF;
			break;
		case 0x07:
			isr &= value ^ 0xff;
			break;
		case 0x08:
			irq_line = (irq_line & 0x100) | value;
			break;

		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C: {
			int i = reg - 0x09 + (io_dcsel ? 4 : 0);
			if (i == 0) {
				// interlace field bit is read-only
				reg_composer[0]     = (reg_composer[0] & ~0x7f) | (value & 0x7f);
				video_palette.dirty = true;
			} else {
				reg_composer[i] = value;
			}
			break;
		}

		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			reg_layer[0][reg - 0x0D] = value;
			refresh_layer_properties(0);
			break;

		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A:
			reg_layer[1][reg - 0x14] = value;
			refresh_layer_properties(1);
			break;

		case 0x1B: pcm_write_ctrl(value); break;
		case 0x1C: pcm_write_rate(value); break;
		case 0x1D: pcm_write_fifo(value); break;

		case 0x1E:
		case 0x1F:
			vera_spi_write(reg & 1, value);
			break;
	}
}

bool vera_video_is_tilemap_address(uint32_t addr)
{
	for (int l = 0; l < 2; ++l) {
		struct vera_video_layer_properties *props = &layer_properties[l];
		if (addr < props->map_base) {
			continue;
		}
		if (addr >= props->map_base + (2 << (props->mapw_log2 + props->maph_log2))) {
			continue;
		}

		return true;
	}
	return false;
}

bool vera_video_is_tiledata_address(uint32_t addr)
{
	for (int l = 0; l < 2; ++l) {
		struct vera_video_layer_properties *props = &layer_properties[l];
		if (addr < props->tile_base) {
			continue;
		}
		int tile_size = props->tilew * props->tileh * props->bits_per_pixel / 8;
		if (addr >= props->tile_base + tile_size * (props->bits_per_pixel == 1 ? 256 : 1024)) {
			continue;
		}

		return true;
	}
	return false;
}

bool vera_video_is_special_address(uint32_t addr)
{
	return addr >= 0x1F9C0;
}

const uint8_t *vera_video_get_framebuffer()
{
	return framebuffer;
}

void vera_video_get_increment_values(const int **in, int *length)
{
	if (in != nullptr && length != nullptr) {
		*in     = increments;
		*length = 32;
	}
}

const int vera_video_get_data_auto_increment(int channel)
{
	return increments[io_inc[channel & 1]];
}

void vera_video_set_data_auto_increment(int channel, uint8_t value)
{
	io_inc[channel & 1] = value;
}

const uint32_t vera_video_get_data_addr(int channel)
{
	return io_addr[channel & 1];
}

void vera_video_set_data_addr(int channel, uint32_t value)
{
	io_addr[channel & 1] = value;
}

const uint8_t vera_video_get_dc_video()
{
	return reg_composer[0];
}

const uint8_t vera_video_get_dc_hscale()
{
	return reg_composer[1];
}

const uint8_t vera_video_get_dc_vscale()
{
	return reg_composer[2];
}

const uint8_t vera_video_get_dc_border()
{
	return reg_composer[3];
}

const uint8_t vera_video_get_dc_hstart()
{
	return reg_composer[4];
}

const uint8_t vera_video_get_dc_hstop()
{
	return reg_composer[5];
}

const uint8_t vera_video_get_dc_vstart()
{
	return reg_composer[6];
}

const uint8_t vera_video_get_dc_vstop()
{
	return reg_composer[7];
}

void vera_video_set_dc_video(uint8_t value)
{
	reg_composer[0]     = value;
	if ((value & 0x3) == 1) {
		reg_composer[0] &= 0x7f;
	}
	video_palette.dirty = true;
}

void vera_video_set_dc_hscale(uint8_t value)
{
	reg_composer[1] = value;
}

void vera_video_set_dc_vscale(uint8_t value)
{
	reg_composer[2] = value;
}

void vera_video_set_dc_border(uint8_t value)
{
	reg_composer[3] = value;
}

void vera_video_set_dc_hstart(uint8_t value)
{
	reg_composer[4] = value;
}

void vera_video_set_dc_hstop(uint8_t value)
{
	reg_composer[5] = value;
}

void vera_video_set_dc_vstart(uint8_t value)
{
	reg_composer[6] = value;
}

void vera_video_set_dc_vstop(uint8_t value)
{
	reg_composer[7] = value;
}

void vera_video_set_cheat_mask(int mask)
{
	cheat_mask = mask;
}

int vera_video_get_cheat_mask()
{
	return cheat_mask;
}

bool vera_video_is_cheat_frame()
{
	return frame_count & cheat_mask;
}

void vera_video_set_log_video(bool enable)
{
	log_video = enable;
}

bool vera_video_get_log_video()
{
	return log_video;
}

void vera_video_get_expanded_vram(uint32_t address, int bpp, uint8_t *dest, uint32_t dest_size)
{
	switch (bpp) {
		case 1:
			expand_1bpp_data(dest, video_ram + address, dest_size);
			break;
		case 2:
			expand_2bpp_data(dest, video_ram + address, dest_size);
			break;
		case 4:
			expand_4bpp_data(dest, video_ram + address, dest_size);
			break;
		case 8:
			vera_video_space_read_range(dest, address, dest_size);
			break;
		default:
			break;
	}
}

const uint32_t *vera_video_get_palette_argb32()
{
	return video_palette.entries;
}

const uint16_t *vera_video_get_palette_argb16()
{
	return reinterpret_cast<const uint16_t *>(palette);
}

void vera_video_set_palette(int index, uint32_t argb32)
{
	argb32 &= 0xf0f0f000;
	argb32 |= (argb32 >> 4);
	argb32 &= 0x0ff00f00;
	const uint16_t argb16 = ((argb32 >> 8) & 0x00f) | ((argb32 >> 16) & 0xff0) | 0xf000;

	vera_video_set_palette(index, argb16);
}

void vera_video_set_palette(int index, uint16_t argb16)
{
	uint16_t *const p16 = reinterpret_cast<uint16_t *>(palette);
	p16[index & 0xff]   = argb16;
	video_palette.dirty = true;
}

const vera_video_layer_properties *vera_video_get_layer_properties(int layer)
{
	if (layer >= 0 && layer < 2) {
		return &layer_properties[layer];
	} else {
		return nullptr;
	}
}

const uint8_t *vera_video_get_layer_data(int layer)
{
	if (layer >= 0 && layer < 2) {
		return reg_layer[layer];
	} else {
		return nullptr;
	}
}

const vera_video_sprite_properties *vera_video_get_sprite_properties(int sprite)
{
	if (sprite >= 0 && sprite < 128) {
		return &sprite_properties[sprite];
	} else {
		return nullptr;
	}
}

const uint8_t *vera_video_get_sprite_data(int sprite)
{
	if (sprite >= 0 && sprite < 128) {
		return sprite_data[sprite];
	} else {
		return nullptr;
	}
}

void vera_video_enable_safety_frame(bool enable)
{
	shadow_safety_frame = enable;
}

bool vera_video_safety_frame_is_enabled()
{
	return shadow_safety_frame;
}

float vera_video_get_scan_pos_x()
{
	return (reg_composer[0] & 2) ? floorf((ntsc_half_cnt + (ntsc_scan_pos_y & 1) * NTSC_HALF_SCAN_WIDTH) / 2) : vga_scan_pos_x;
}

uint16_t vera_video_get_scan_pos_y()
{
	if (reg_composer[0] & 2) {
		uint16_t y = ntsc_scan_pos_y & ~1;
		if (y >= SCAN_HEIGHT) {
			y -= SCAN_HEIGHT;
		}
		return y;
	}
	return vga_scan_pos_y;
}

vera_video_rect vera_video_get_scan_visible()
{
	if (reg_composer[0] & 2) {
		return vera_video_rect{
			NTSC_X_OFFSET / 2, NTSC_X_OFFSET / 2 + SCREEN_WIDTH,
			NTSC_Y_OFFSET_LOW, NTSC_Y_OFFSET_LOW + SCREEN_HEIGHT
		};
	} else {
		return vera_video_rect{
			VGA_X_OFFSET, VGA_X_OFFSET + SCREEN_WIDTH,
			VGA_Y_OFFSET, VGA_Y_OFFSET + SCREEN_HEIGHT
		};
	}
}