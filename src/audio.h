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

void audio_init(const char *dev_name, int num_audio_buffers);
void audio_close(void);
void audio_render(int cpu_clocks);

void audio_usage(void);

const int16_t *audio_get_psg_buffer();
const int16_t *audio_get_pcm_buffer();
const int16_t *audio_get_ym_buffer();
