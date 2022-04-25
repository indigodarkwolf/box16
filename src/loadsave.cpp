// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#include "loadsave.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "glue.h"
#include "memory.h"
#include "options.h"
#include "rom_symbols.h"
#include "vera/vera_video.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static int create_directory_listing(uint8_t *data)
{
	uint8_t *data_start = data;
	int      file_size;

	// We inject this directly into RAM, so
	// this does not include the load address!

	// link
	*data++ = 1;
	*data++ = 1;
	// line number
	*data++ = 0;
	*data++ = 0;
	*data++ = 0x12; // REVERSE ON
	*data++ = '"';

	const std::string path_str = Options.hyper_path.generic_string();
	{
		int       i    = 0;
		const int stop = MIN((int)path_str.length(), 16);
		for (; i < stop; ++i) {
			*data++ = path_str[i];
		}
		for (; i < 16; ++i) {
			*data++ = ' ';
		}
	}
	*data++ = '"';
	*data++ = ' ';
	*data++ = '0';
	*data++ = '0';
	*data++ = ' ';
	*data++ = 'P';
	*data++ = 'C';
	*data++ = 0;

	if (!std::filesystem::exists(Options.hyper_path)) {
		return 0;
	}

	for (const auto &entry : std::filesystem::directory_iterator(Options.hyper_path)) {
		const std::string            filename = entry.path().filename().generic_string();
		size_t                       namlen   = filename.length();
		std::filesystem::file_status st       = entry.status();

		file_size = ((int)entry.file_size() + 255) / 256;
		if (file_size > 0xFFFF) {
			file_size = 0xFFFF;
		}

		// link
		*data++ = 1;
		*data++ = 1;

		*data++ = file_size & 0xFF;
		*data++ = file_size >> 8;
		if (file_size < 1000) {
			*data++ = ' ';
			if (file_size < 100) {
				*data++ = ' ';
				if (file_size < 10) {
					*data++ = ' ';
				}
			}
		}
		*data++ = '"';
		if (namlen > 16) {
			namlen = 16; // TODO hack
		}
		memcpy(data, filename.c_str(), namlen);
		data += namlen;
		*data++ = '"';
		for (size_t i = namlen; i < 16; i++) {
			*data++ = ' ';
		}
		*data++ = ' ';
		*data++ = 'P';
		*data++ = 'R';
		*data++ = 'G';
		*data++ = 0;
	}

	// link
	*data++ = 1;
	*data++ = 1;

	*data++ = 255; // "65535"
	*data++ = 255;

	const char *blocks_free = "BLOCKS FREE.";
	memcpy(data, blocks_free, strlen(blocks_free));
	data += strlen(blocks_free);
	*data++ = 0;

	// link
	*data++ = 0;
	*data++ = 0;

	return (int)(reinterpret_cast<uintptr_t>(data) - reinterpret_cast<uintptr_t>(data_start));
}

void LOAD()
{
	char const *kernal_filename = (char *)&RAM[RAM[FNADR] | RAM[FNADR + 1] << 8];
	uint16_t    override_start  = (x | (y << 8));

	if (kernal_filename[0] == '$') {
		uint16_t dir_len = create_directory_listing(RAM + override_start);
		uint16_t end     = override_start + dir_len;
		x                = end & 0xff;
		y                = end >> 8;
		status &= 0xfe;
		RAM[STATUS] = 0;
		a           = 0;
	} else {
		char      filename[PATH_MAX];
		const int len = MIN(RAM[FNLEN], PATH_MAX - 1);
		memcpy(filename, kernal_filename, len);
		filename[len] = 0;

		std::filesystem::path filepath = Options.hyper_path / filename;

		SDL_RWops *f = SDL_RWFromFile(filepath.generic_string().c_str(), "rb");
		if (!f) {
			a           = 4; // FNF
			RAM[STATUS] = a;
			status |= 1;
			return;
		}
		uint8_t start_lo = SDL_ReadU8(f);
		uint8_t start_hi = SDL_ReadU8(f);

		uint16_t start;
		if (!RAM[SA]) {
			start = override_start;
		} else {
			start = start_hi << 8 | start_lo;
		}

		uint16_t bytes_read = 0;
		if (a > 1) {
			// Video RAM
			vera_video_write(0, start & 0xff);
			vera_video_write(1, start >> 8);
			vera_video_write(2, ((a - 2) & 0xf) | 0x10);
			uint8_t buf[2048];
			while (1) {
				uint16_t n = (uint16_t)SDL_RWread(f, buf, 1, sizeof buf);
				if (n == 0)
					break;
				for (size_t i = 0; i < n; i++) {
					vera_video_write(3, buf[i]);
				}
				bytes_read += n;
			}
		} else if (start < 0x9f00) {
			// Fixed RAM
			bytes_read = (uint16_t)SDL_RWread(f, RAM + start, 1, 0x9f00 - start);
		} else if (start < 0xa000) {
			// IO addresses
		} else if (start < 0xc000) {
			// banked RAM
			while (1) {
				size_t len = 0xc000 - start;
				bytes_read = (uint16_t)SDL_RWread(f, RAM + (((memory_get_ram_bank() % (uint16_t)Options.num_ram_banks) << 13) & 0xffffff) + start, 1, len);
				if (bytes_read < len)
					break;

				// Wrap into the next bank
				start = 0xa000;
				memory_set_ram_bank(1 + memory_get_ram_bank());
			}
		} else {
			// ROM
		}

		SDL_RWclose(f);

		uint16_t end = start + bytes_read;
		x            = end & 0xff;
		y            = end >> 8;
		status &= 0xfe;
		RAM[STATUS] = 0;
		a           = 0;
	}
}

void SAVE()
{
	char const *kernal_filename = (char *)&RAM[RAM[FNADR] | RAM[FNADR + 1] << 8];

	char      filename[PATH_MAX];
	const int len = MIN(RAM[FNLEN], PATH_MAX - 1);
	memcpy(filename, kernal_filename, len);
	filename[len] = '\0';

	std::filesystem::path filepath = Options.hyper_path / filename;

	uint16_t start = RAM[a] | RAM[a + 1] << 8;
	uint16_t end   = x | y << 8;
	if (end < start) {
		status |= 1;
		a = 0;
		return;
	}

	SDL_RWops *f = SDL_RWFromFile(filepath.generic_string().c_str(), "wb");
	if (!f) {
		a           = 4; // FNF
		RAM[STATUS] = a;
		status |= 1;
		return;
	}

	SDL_WriteU8(f, start & 0xff);
	SDL_WriteU8(f, start >> 8);

	SDL_RWwrite(f, RAM + start, 1, end - start);
	SDL_RWclose(f);

	status &= 0xfe;
	RAM[STATUS] = 0;
	a           = 0;
}
