// Commander X16 Emulator
// Copyright (c) 2020 Frank van den Hoef
// All rights reserved. License: 2-clause BSD

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct pcm_debug_info {
	uint8_t *fifo;
	unsigned curidx;
	unsigned cursiz;
	unsigned minsiz;
	unsigned maxsiz;
};

void           pcm_reset(void);
void           pcm_write_ctrl(uint8_t val);
uint8_t        pcm_read_ctrl(void);
void           pcm_write_rate(uint8_t val);
uint8_t        pcm_read_rate(void);
void           pcm_write_fifo(uint8_t val);
void           pcm_render(int16_t *buf, unsigned num_samples);
bool           pcm_is_fifo_almost_empty(void);
pcm_debug_info pcm_get_debug_info(void);
void           pcm_reset_debug_values(void);
