// Commander X16 Emulator
// Copyright (c) 2020 Frank van den Hoef
// All rights reserved. License: 2-clause BSD

#include "audio.h"
#include "vera/vera_pcm.h"
#include "vera/vera_psg.h"
#include "ym2151/ym2151.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLERATE (25000000 / 512)

static SDL_AudioDeviceID Audio_dev = 0;
static int               Vera_clks = 0;
static int               Cpu_clks  = 0;
static int16_t *         Buffer    = nullptr;

static int16_t Psg_buffer[2 * SAMPLES_PER_BUFFER];
static int16_t Pcm_buffer[2 * SAMPLES_PER_BUFFER];
static int16_t Ym_buffer[2 * SAMPLES_PER_BUFFER];

audio_lock_scope::audio_lock_scope()
{
	SDL_LockAudio();
}

audio_lock_scope::~audio_lock_scope()
{
	SDL_UnlockAudio();
}

static void audio_callback(void *, Uint8 *stream, int len)
{
	const int expected = 2 * SAMPLES_PER_BUFFER * sizeof(int16_t);
	if (len != expected) {
		printf("Audio buffer size mismatch! (expected: %d, got: %d)\n", expected, len);
		return;
	}

	memcpy(stream, Buffer, len);

	psg_render(Psg_buffer, SAMPLES_PER_BUFFER);
	pcm_render(Pcm_buffer, SAMPLES_PER_BUFFER);
	YM_stream_update((uint16_t *)Ym_buffer, SAMPLES_PER_BUFFER);

	// Mix PSG, PCM and YM output
	for (int i = 0; i < 2 * SAMPLES_PER_BUFFER; i++) {
		Buffer[i] = ((int)Psg_buffer[i] + (int)Pcm_buffer[i] + (int)Ym_buffer[i]) / 3;
	}
}

void audio_init(const char *dev_name, int /*num_audio_buffers*/)
{
	if (Audio_dev > 0) {
		audio_close();
	}

	// Allocate audio buffers
	Buffer = new int16_t[2 * SAMPLES_PER_BUFFER];
	memset(Buffer, 0, 2 * SAMPLES_PER_BUFFER * sizeof(int16_t));

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
		fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
		if (dev_name != NULL) {
			audio_usage();
		}
		exit(-1);
	}

	// Init YM2151 emulation. 4 MHz clock
	YM_Create(3579545);
	YM_init(obtained.freq, 60);

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

	if (Buffer != nullptr) {
		delete[] Buffer;
	}
}

void audio_render(int cpu_clocks)
{
	// TODO: Maybe do some pre-rendering into a true backbuffer?
}

void audio_usage(void)
{
	// SDL_GetAudioDeviceName doesn't work if audio isn't initialized.
	// Since argument parsing happens before initializing SDL, ensure the
	// audio subsystem is initialized before printing audio device names.
	SDL_InitSubSystem(SDL_INIT_AUDIO);

	// List all available sound devices
	printf("The following sound output devices are available:\n");
	const int sounds = SDL_GetNumAudioDevices(0);
	for (int i = 0; i < sounds; ++i) {
		printf("\t%s\n", SDL_GetAudioDeviceName(i, 0));
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
