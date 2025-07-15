// Commander X16 Emulator
// Copyright (c) 2020 Frank van den Hoef
// All rights reserved. License: 2-clause BSD

#include "vera_psg.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"

static psg_channel Channels[PSG_NUM_CHANNELS];

static uint16_t noise_state;

static uint16_t volume_lut[64] = {
	  0,                                           4,   8,  12,
	 16,  17,  18,  20,  21,  22,  23,  25,  26,  28,  30,  31,
	 33,  35,  37,  40,  42,  45,  47,  50,  53,  56,  60,  63,
	 67,  71,  75,  80,  85,  90,  95, 101, 107, 113, 120, 127,
	135, 143, 151, 160, 170, 180, 191, 202, 214, 227, 241, 255,
	270, 286, 303, 321, 341, 361, 382, 405, 429, 455, 482, 511
};

void psg_reset(void)
{
	audio_lock_scope lock;
	memset(Channels, 0, sizeof(Channels));
	noise_state = 1;
}

void psg_writereg(uint8_t reg, uint8_t val)
{
	audio_lock_scope lock;
	reg &= 0x3f;

	int ch  = reg / 4;
	int idx = reg & 3;

	switch (idx) {
		case 0: Channels[ch].freq = (Channels[ch].freq & 0xFF00) | val; break;
		case 1: Channels[ch].freq = (Channels[ch].freq & 0x00FF) | (val << 8); break;
		case 2: {
			Channels[ch].right  = (val & 0x80) != 0;
			Channels[ch].left   = (val & 0x40) != 0;
			Channels[ch].volume = (val & 0x3F);
			break;
		}
		case 3: {
			Channels[ch].pw       = val & 0x3F;
			Channels[ch].waveform = val >> 6;
			break;
		}
	}
}

static void render(int16_t *left, int16_t *right)
{
	int l = 0;
	int r = 0;

	for (int i = 0; i < PSG_NUM_CHANNELS; i++) {
		noise_state = (noise_state << 1) | (((noise_state >> 1) ^ (noise_state >> 2) ^ (noise_state >> 4) ^ (noise_state >> 15)) & 1);
		struct psg_channel *ch = &Channels[i];

		uint32_t new_phase = (ch->left || ch->right) ? ((ch->phase + ch->freq) & 0x1FFFF) : 0;
		if ((ch->phase & 0x10000) && !(new_phase & 0x10000)) {
			ch->noiseval = (noise_state >> 1) & 0x3F;
		}
		ch->phase = new_phase;

		uint8_t v = 0;
		switch (ch->waveform) {
			case WF_PULSE: v = (ch->phase >> 10) > ch->pw ? 0 : 63; break;
			case WF_SAWTOOTH: v = (ch->phase >> 11) ^ ((ch->pw ^ 0x3f) & 0x3f); break;
			case WF_TRIANGLE: v = ((ch->phase & 0x10000) ? (~(ch->phase >> 10) & 0x3F) : ((ch->phase >> 10) & 0x3F)) ^ ((ch->pw ^ 0x3f) & 0x3f); break;
			case WF_NOISE: v = ch->noiseval; break;
		}
		int16_t sv = (v ^ 0x20);
		if (sv & 0x20) {
			sv |= 0xFFC0;
		}

		int16_t val = sv * volume_lut[ch->volume];

		if (ch->left) {
			l += val >> 3;
		}
		if (ch->right) {
			r += val >> 3;
		}
	}

	*left  = l;
	*right = r;
}

void psg_render(int16_t *buf, unsigned int num_samples)
{
	while (num_samples--) {
		render(&buf[0], &buf[1]);
		buf += 2;
	}
}

const psg_channel *psg_get_channel(unsigned int channel)
{
	audio_lock_scope lock;
	if (channel > PSG_NUM_CHANNELS) {
		return nullptr;
	}

	return &Channels[channel];
}

psg_channel *psg_get_channel_debug(unsigned int channel)
{
	audio_lock_scope lock;
	if (channel >= PSG_NUM_CHANNELS) {
		return nullptr;
	}

	return &Channels[channel];
}

void psg_set_channel_frequency(unsigned int channel, uint16_t freq)
{
	audio_lock_scope lock;
	if (channel < PSG_NUM_CHANNELS) {
		Channels[channel].freq = freq;
	}
}

void psg_set_channel_left(unsigned int channel, bool left)
{
	audio_lock_scope lock;
	if (channel < PSG_NUM_CHANNELS) {
		Channels[channel].left = left;
	}
}

void psg_set_channel_right(unsigned int channel, bool right)
{
	audio_lock_scope lock;
	if (channel < PSG_NUM_CHANNELS) {
		Channels[channel].right = right;
	}
}

void psg_set_channel_volume(unsigned int channel, uint8_t volume)
{
	audio_lock_scope lock;
	if (channel < PSG_NUM_CHANNELS) {
		Channels[channel].volume = volume & 0x3f;
	}
}

void psg_set_channel_waveform(unsigned int channel, uint8_t waveform)
{
	audio_lock_scope lock;
	if (channel < PSG_NUM_CHANNELS) {
		Channels[channel].waveform = waveform;
	}
}

void psg_set_channel_pulse_width(unsigned int channel, uint8_t pw)
{
	audio_lock_scope lock;
	if (channel < PSG_NUM_CHANNELS) {
		Channels[channel].pw = pw & 0x3f;
	}
}
