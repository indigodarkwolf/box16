// Commander X16 Emulator
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include "audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ring_buffer.h"
#include "vera/vera_pcm.h"
#include "vera/vera_psg.h"
#include "ym2151/ym2151.h"

static SDL_AudioDeviceID Audio_dev            = 0;
static int               Obtained_sample_rate = 0;
static int               Clocks_per_sample    = 0;

static int16_t Ym_buffer[2 * SAMPLES_PER_BUFFER];
static int16_t Psg_buffer[2 * SAMPLES_PER_BUFFER];
static int16_t Pcm_buffer[2 * SAMPLES_PER_BUFFER];

struct audio_buffer {
	int16_t data[SAMPLES_PER_BUFFER * 2];
};

#define BACKBUFFER_COUNT (SAMPLERATE / (SAMPLES_PER_BUFFER * 5))
static ring_allocator<audio_buffer, BACKBUFFER_COUNT> Audio_backbuffer;

static constexpr size_t Low_buffer_threshold = 2;
static int              Clocks_rendered      = 0;

static volatile audio_render_callback Render_callback = nullptr;

audio_lock_scope::audio_lock_scope()
{
	SDL_LockAudio();
}

audio_lock_scope::~audio_lock_scope()
{
	SDL_UnlockAudio();
}

static void audio_callback_nop(const int16_t *, const int)
{
}

static void audio_render_buffer()
{
	YM_render(Ym_buffer, SAMPLES_PER_BUFFER, Obtained_sample_rate);
	psg_render(Psg_buffer, SAMPLES_PER_BUFFER);
	pcm_render(Pcm_buffer, SAMPLES_PER_BUFFER);

	int16_t buffer[2 * SAMPLES_PER_BUFFER];
	memcpy(buffer, Ym_buffer, sizeof(Ym_buffer));
	SDL_MixAudioFormat(reinterpret_cast<uint8_t *>(buffer), reinterpret_cast<uint8_t *>(Psg_buffer), AUDIO_S16, sizeof(Psg_buffer), SDL_MIX_MAXVOLUME);
	SDL_MixAudioFormat(reinterpret_cast<uint8_t *>(buffer), reinterpret_cast<uint8_t *>(Pcm_buffer), AUDIO_S16, sizeof(Pcm_buffer), SDL_MIX_MAXVOLUME);

	// Commit to the backbuffer
	{
		audio_lock_scope lock;
		audio_buffer *   backbuffer = Audio_backbuffer.allocate();
		memcpy(backbuffer->data, buffer, sizeof(buffer));
	}

	Render_callback(reinterpret_cast<int16_t *>(buffer), SAMPLES_PER_BUFFER);
}

static void audio_callback(void *, Uint8 *stream, int len)
{
	const int expected = 2 * SAMPLES_PER_BUFFER * sizeof(int16_t);
	if (len != expected) {
		fmt::print("ERROR: Audio buffer size mismatch! (expected: {}, got: {})\n", expected, len);
		return;
	}

	const audio_buffer *buffer = Audio_backbuffer.get_oldest();
	memcpy(stream, buffer->data, len);

	if (Audio_backbuffer.count() > 1) {
		Audio_backbuffer.free_oldest();
	}
}

void audio_init(const char *dev_name, int /*num_audio_buffers*/)
{
	if (Audio_dev > 0) {
		audio_close();
	}

	Render_callback = audio_callback_nop;

	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;

	// Setup SDL audio
	memset(&desired, 0, sizeof(desired));
	desired.freq     = SAMPLERATE;
	desired.format   = AUDIO_S16SYS;
	desired.samples  = SAMPLES_PER_BUFFER;
	desired.channels = 2;
	desired.callback = audio_callback;

	Audio_dev = SDL_OpenAudioDevice(dev_name, 0, &desired, &obtained, 0);
	if (Audio_dev <= 0) {
		fmt::print(stderr, "SDL_OpenAudioDevice failed: {}\n", SDL_GetError());
		if (dev_name != NULL) {
			audio_usage();
		}
	}

	Obtained_sample_rate = obtained.freq;
	Clocks_per_sample    = 8000000 / Obtained_sample_rate;

	fmt::print("INFO: Audio buffer is {} bytes\n", obtained.size);

	// Prime the buffer
	{
		auto backbuffer = Audio_backbuffer.allocate();
		memset(backbuffer->data, 0, sizeof(backbuffer->data));
	}

	// Start playback
	SDL_PauseAudioDevice(Audio_dev, 0);
}

void audio_close(void)
{
	if (Audio_dev == 0) {
		return;
	}

	SDL_CloseAudioDevice(Audio_dev);
	Audio_dev = 0;
}

void audio_render(int cpu_clocks)
{
	YM_prerender(cpu_clocks);

	if (Audio_dev == 0) {
		YM_clear_backbuffer();
		return;
	}

	Clocks_rendered += cpu_clocks;
	int samples_to_render = Clocks_rendered / Clocks_per_sample;
	while (samples_to_render >= SAMPLES_PER_BUFFER) {
		audio_render_buffer();
		samples_to_render -= SAMPLES_PER_BUFFER;
		Clocks_rendered -= Clocks_per_sample * SAMPLES_PER_BUFFER;
	}

	while (Audio_backbuffer.count() < Low_buffer_threshold) {
		audio_render_buffer();
	}
}

void audio_usage(void)
{
	// SDL_GetAudioDeviceName doesn't work if audio isn't initialized.
	// Since argument parsing happens before initializing SDL, ensure the
	// audio subsystem is initialized before printing audio device names.
	SDL_InitSubSystem(SDL_INIT_AUDIO);

	// List all available sound devices
	fmt::print("The following sound output devices are available:\n");
	const int sounds = SDL_GetNumAudioDevices(0);
	for (int i = 0; i < sounds; ++i) {
		fmt::print("\t{}\n", SDL_GetAudioDeviceName(i, 0));
	}

	SDL_Quit();
	exit(1);
}

void audio_get_psg_buffer(int16_t *dst)
{
	audio_lock_scope lock;
	memcpy(dst, Psg_buffer, 2 * SAMPLES_PER_BUFFER * sizeof(int16_t));
}

void audio_get_pcm_buffer(int16_t *dst)
{
	audio_lock_scope lock;
	memcpy(dst, Pcm_buffer, 2 * SAMPLES_PER_BUFFER * sizeof(int16_t));
}

void audio_get_ym_buffer(int16_t *dst)
{
	audio_lock_scope lock;
	memcpy(dst, Ym_buffer, 2 * SAMPLES_PER_BUFFER * sizeof(int16_t));
}

int audio_get_sample_rate()
{
	return Obtained_sample_rate;
}

void audio_set_render_callback(audio_render_callback cb)
{
	audio_lock_scope lock;
	Render_callback = cb;
}