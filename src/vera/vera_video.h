// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef VERA_VIDEO_H
#define VERA_VIDEO_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// visible area we're drawing
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

void vera_video_reset(void);
bool vera_video_step(float mhz, float cycles);
bool vera_video_get_irq_out(void);
void vera_video_save(SDL_RWops *f);

uint8_t vera_debug_video_read(uint8_t reg);
uint8_t vera_video_read(uint8_t reg);
void    vera_video_write(uint8_t reg, uint8_t value);

uint8_t via1_read(uint8_t reg);
void    via1_write(uint8_t reg, uint8_t value);

// For debugging purposes only:
uint8_t vera_video_space_read(uint32_t address);
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

#endif
