// Commander X16 Emulator
// Copyright (c) 2020 Frank van den Hoef
// All rights reserved. License: 2-clause BSD

#pragma once

#include <stdint.h>

#define PSG_NUM_CHANNELS (16)

enum waveform {
	WF_PULSE = 0,
	WF_SAWTOOTH,
	WF_TRIANGLE,
	WF_NOISE,
};

struct psg_channel {
	uint16_t freq;
	uint8_t  volume;
	bool     left, right;
	uint8_t  pw;
	uint8_t  waveform;

	unsigned phase;
	uint8_t  noiseval;
};

void psg_reset(void);
void psg_writereg(uint8_t reg, uint8_t val);
void psg_render(int16_t *buf, unsigned int num_samples);

const psg_channel *psg_get_channel(unsigned int channel);
psg_channel *      psg_get_channel_debug(unsigned int channel);

void psg_set_channel_frequency(unsigned int channel, uint16_t freq);
void psg_set_channel_left(unsigned int channel, bool left);
void psg_set_channel_right(unsigned int channel, bool right);
void psg_set_channel_volume(unsigned int channel, uint8_t volume);
void psg_set_channel_waveform(unsigned int channel, uint8_t waveform);
void psg_set_channel_pulse_width(unsigned int channel, uint8_t pw);
