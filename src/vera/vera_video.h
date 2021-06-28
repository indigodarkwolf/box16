// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef VERA_VIDEO_H
#define VERA_VIDEO_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// both VGA and NTSC signal timing
#define SCAN_WIDTH 800
#define SCAN_HEIGHT 525

// visible area we're drawing
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

struct vera_video_layer_properties {
	uint8_t  color_depth;
	uint32_t map_base;
	uint32_t tile_base;

	bool text_mode;
	bool text_mode_256c;
	bool tile_mode;
	bool bitmap_mode;

	uint16_t hscroll;
	uint16_t vscroll;

	uint8_t  mapw_log2;
	uint8_t  maph_log2;
	uint16_t tilew;
	uint16_t tileh;
	uint8_t  tilew_log2;
	uint8_t  tileh_log2;

	uint16_t mapw_max;
	uint16_t maph_max;
	uint16_t tilew_max;
	uint16_t tileh_max;
	uint16_t layerw_max;
	uint16_t layerh_max;

	uint8_t tile_size_log2;

	uint8_t bits_per_pixel;
	uint8_t first_color_pos;
	uint8_t color_mask;
	uint8_t color_fields_max;
};

struct vera_video_sprite_properties {
	int8_t  sprite_zdepth;
	uint8_t sprite_collision_mask;

	int16_t sprite_x;
	int16_t sprite_y;
	uint8_t sprite_width_log2;
	uint8_t sprite_height_log2;
	uint8_t sprite_width;
	uint8_t sprite_height;

	bool hflip;
	bool vflip;

	uint8_t  color_mode;
	uint32_t sprite_address;

	uint16_t palette_offset;
};

struct vera_video_rect {
	uint16_t hstart;
	uint16_t hstop;
	uint16_t vstart;
	uint16_t vstop;
};

void vera_video_reset(void);
bool vera_video_step(float mhz, float cycles);
void vera_video_force_redraw_screen();
bool vera_video_get_irq_out(void);
void vera_video_save(SDL_RWops *f);

uint8_t vera_debug_video_read(uint8_t reg);
uint8_t vera_video_read(uint8_t reg);
void    vera_video_write(uint8_t reg, uint8_t value);

uint8_t via1_read(uint8_t reg);
void    via1_write(uint8_t reg, uint8_t value);

// For debugging purposes only:
uint8_t vera_video_space_read(uint32_t address);
void    vera_video_space_read_range(uint8_t *dest, uint32_t address, uint32_t size);
void    vera_video_space_write(uint32_t address, uint8_t value);

bool vera_video_is_tilemap_address(uint32_t addr);
bool vera_video_is_tiledata_address(uint32_t addr);
bool vera_video_is_special_address(uint32_t addr);

const uint8_t *vera_video_get_framebuffer();

void vera_video_get_increment_values(const int **in, int *length);

const int vera_video_get_data_auto_increment(int channel);
void      vera_video_set_data_auto_increment(int channel, uint8_t value);

const uint32_t vera_video_get_data_addr(int channel);
void           vera_video_set_data_addr(int channel, uint32_t value);

const uint8_t vera_video_get_dc_video();
const uint8_t vera_video_get_dc_hscale();
const uint8_t vera_video_get_dc_vscale();
const uint8_t vera_video_get_dc_border();

const uint8_t vera_video_get_dc_hstart();
const uint8_t vera_video_get_dc_hstop();
const uint8_t vera_video_get_dc_vstart();
const uint8_t vera_video_get_dc_vstop();

void vera_video_set_dc_video(uint8_t value);
void vera_video_set_dc_hscale(uint8_t value);
void vera_video_set_dc_vscale(uint8_t value);
void vera_video_set_dc_border(uint8_t value);

void vera_video_set_dc_hstart(uint8_t value);
void vera_video_set_dc_hstop(uint8_t value);
void vera_video_set_dc_vstart(uint8_t value);
void vera_video_set_dc_vstop(uint8_t value);

void vera_video_set_cheat_mask(int mask);
int  vera_video_get_cheat_mask();
void vera_video_set_log_video(bool enable);
bool vera_video_get_log_video();

void vera_video_get_expanded_vram(uint32_t address, int bpp, uint8_t *dest, uint32_t dest_size);

const uint32_t *vera_video_get_palette_argb32();
const uint16_t *vera_video_get_palette_argb16();

const vera_video_layer_properties *vera_video_get_layer_properties(int layer);
const uint8_t *                    vera_video_get_layer_data(int layer);

const vera_video_sprite_properties *vera_video_get_sprite_properties(int sprite);
const uint8_t *                     vera_video_get_sprite_data(int sprite);

void vera_video_enable_safety_frame(bool enable);
bool vera_video_safety_frame_is_enabled();

float    vera_video_get_scan_pos_x();
uint16_t vera_video_get_scan_pos_y();

vera_video_rect vera_video_get_scan_visible();

#endif
