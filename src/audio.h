// Commander X16 Emulator
// Copyright (c) 2020 Frank van den Hoef
// Copyright (c) 2021-2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#pragma once

#include <SDL.h>

#define SAMPLERATE (25000000 / 512)
#ifdef __EMSCRIPTEN__
#	define SAMPLES_PER_BUFFER (1024)
#else
#	define SAMPLES_PER_BUFFER (256)
#endif

class audio_lock_scope
{
public:
	audio_lock_scope();
	~audio_lock_scope();
};

using audio_render_callback = void (*)(const int16_t *samples, const int num_samples);

void audio_init(const char *dev_name, int num_audio_buffers);
void audio_close(void);
void audio_render(int cpu_clocks);

void audio_usage(void);

void audio_get_psg_buffer(int16_t *dst);
void audio_get_pcm_buffer(int16_t *dst);
void audio_get_ym_buffer(int16_t *dst);

int audio_get_sample_rate();
void audio_set_render_callback(audio_render_callback cb);
