// Commander X16 Emulator
// Copyright (c) 2020 Frank van den Hoef
// All rights reserved. License: 2-clause BSD

#pragma once

#include <SDL.h>

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

void audio_init(const char *dev_name, int num_audio_buffers);
void audio_close(void);
void audio_render(int cpu_clocks);

void audio_usage(void);

void audio_get_psg_buffer(int16_t *dst);
void audio_get_pcm_buffer(int16_t *dst);
void audio_get_ym_buffer(int16_t *dst);
