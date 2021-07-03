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
static int16_t **        Buffers;
static int               Read_index   = 0;
static int               Write_index  = 0;
static int               Buffer_count = 0;
static int               Num_buffers     = 0;

static void audio_callback(void *, Uint8 *stream, int len)
{
	int expected = 2 * SAMPLES_PER_BUFFER * sizeof(int16_t);
	if (len != expected) {
		printf("Audio buffer size mismatch! (expected: %d, got: %d)\n", expected, len);
		return;
	}

	if (Buffer_count == 0) {
		memset(stream, 0, len);
		return;
	}

	memcpy(stream, Buffers[Read_index++], len);
	if (Read_index == Num_buffers) {
		Read_index = 0;
	}
	Buffer_count--;
}

void audio_init(const char *dev_name, int num_audio_buffers)
{
	if (Audio_dev > 0) {
		audio_close();
	}

	// Set number of buffers
	Num_buffers = num_audio_buffers;
	if (Num_buffers < 3) {
		Num_buffers = 3;
	}
	if (Num_buffers > 1024) {
		Num_buffers = 1024;
	}

	// Allocate audio buffers
	Buffers = new int16_t *[Num_buffers * sizeof(*Buffers)];
	for (int i = 0; i < Num_buffers; i++) {
		Buffers[i] = new int16_t[2 * SAMPLES_PER_BUFFER * sizeof(int16_t)];
	}

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

	// Free audio buffers
	if (Buffers != NULL) {
		for (int i = 0; i < Num_buffers; i++) {
			if (Buffers[i] != NULL) {
				delete [] Buffers[i];
				Buffers[i] = NULL;
			}
		}
		delete [] Buffers;
		Buffers = NULL;
	}
}

static int16_t Psg_buffer[2 * SAMPLES_PER_BUFFER];
static int16_t Pcm_buffer[2 * SAMPLES_PER_BUFFER];
static int16_t Ym_buffer[2 * SAMPLES_PER_BUFFER];

void audio_render(int cpu_clocks)
{
	if (Audio_dev == 0) {
		return;
	}

	Cpu_clks += cpu_clocks;
	if (Cpu_clks > 8) {
		int c = Cpu_clks / 8;
		Cpu_clks -= c * 8;
		Vera_clks += c * 25;
	}

	while (Vera_clks >= 512 * SAMPLES_PER_BUFFER) {
		Vera_clks -= 512 * SAMPLES_PER_BUFFER;

		if (Audio_dev != 0) {
			psg_render(Psg_buffer, SAMPLES_PER_BUFFER);
			pcm_render(Pcm_buffer, SAMPLES_PER_BUFFER);
			YM_stream_update((uint16_t *)Ym_buffer, SAMPLES_PER_BUFFER);

			SDL_LockAudioDevice(Audio_dev);
			const bool buf_available = Buffer_count < Num_buffers;
			SDL_UnlockAudioDevice(Audio_dev);

			if (buf_available) {
				// Mix PSG, PCM and YM output
				int16_t *buf = Buffers[Write_index];
				for (int i = 0; i < 2 * SAMPLES_PER_BUFFER; i++) {
					buf[i] = ((int)Psg_buffer[i] + (int)Pcm_buffer[i] + (int)Ym_buffer[i]) / 3;
				}

				SDL_LockAudioDevice(Audio_dev);
				Write_index++;
				if (Write_index == Num_buffers) {
					Write_index = 0;
				}
				Buffer_count++;
				SDL_UnlockAudioDevice(Audio_dev);
			}
		}
	}
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

const int16_t *audio_get_psg_buffer()
{
	return Psg_buffer;
}

const int16_t *audio_get_pcm_buffer()
{
	return Pcm_buffer;
}

const int16_t *audio_get_ym_buffer()
{
	return Ym_buffer;
}
