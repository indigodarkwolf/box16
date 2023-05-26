// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include "loadsave.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "files.h"
#include "glue.h"
#include "memory.h"
#include "options.h"
#include "rom_symbols.h"
#include "vera/vera_video.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

int create_directory_listing(uint8_t *data)
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

	const std::string path_str = Options.fsroot_path.generic_string();
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

	if (!std::filesystem::exists(Options.fsroot_path)) {
		return 0;
	}

	for (const auto &entry : std::filesystem::directory_iterator(Options.fsroot_path)) {
		const std::string            filename = entry.path().filename().generic_string();
		size_t                       namlen   = filename.length();
		std::filesystem::file_status st       = entry.status();

		if (entry.is_directory()) {
			file_size = 0;
		} else {
			file_size = ((int)entry.file_size() + 255) / 256;
			if (file_size > 0xFFFF) {
				file_size = 0xFFFF;
			}
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
	char const    *kernal_filename = (char *)&RAM[RAM[FNADR] | RAM[FNADR + 1] << 8];
	const uint16_t override_start  = (state6502.x | (state6502.y << 8));

	if (kernal_filename[0] == '$') {
		const uint16_t dir_len = create_directory_listing(RAM + override_start);
		const uint16_t end     = override_start + dir_len;
		state6502.x            = end & 0xff;
		state6502.y            = end >> 8;
		state6502.status &= 0xfe;
		RAM[STATUS] = 0;
		state6502.a = 0;
	} else {
		char      filename[PATH_MAX];
		const int len = MIN(RAM[FNLEN], PATH_MAX - 1);
		memcpy(filename, kernal_filename, len);
		filename[len] = 0;

		std::filesystem::path filepath = Options.fsroot_path / filename;

		x16file *f = x16open(filepath.generic_string().c_str(), "rb");
		if (f == nullptr) {
			state6502.a = 4; // FNF
			RAM[STATUS] = state6502.a;
			state6502.status |= 1;
			return;
		}
		const uint8_t sa = RAM[SA];
		uint8_t       start_lo;
		uint8_t       start_hi;
		x16read(f, &start_lo, sizeof(start_lo), 1);
		x16read(f, &start_hi, sizeof(start_hi) , 1);

		uint16_t start = [override_start, sa, start_lo, start_hi]() -> uint16_t {
			if (sa & 1) {
				return start_hi << 8 | start_lo;
			} else {
				return override_start;
			}
		}();

		if (sa & 0x2) {
			x16seek(f, -2, SEEK_CUR);
		}

		uint16_t bytes_read = 0;
		if (state6502.a > 1) {
			// Video RAM
			vera_video_write(0, start & 0xff);
			vera_video_write(1, start >> 8);
			vera_video_write(2, ((state6502.a - 2) & 0xf) | 0x10);
			uint8_t buf[2048];
			while (1) {
				uint16_t n = (uint16_t)x16read(f, buf, sizeof(uint8_t), sizeof(buf));
				if (n == 0) {
					break;
				}
				for (size_t i = 0; i < n; i++) {
					vera_video_write(3, buf[i]);
				}
				bytes_read += n;
			}
		} else if (start < 0x9f00) {
			// Fixed RAM
			bytes_read = (uint16_t)x16read(f, RAM + start, sizeof(uint8_t), 0x9f00 - start);
		} else if (start < 0xa000) {
			// IO addresses
		} else if (start < 0xc000) {
			// banked RAM
			while (1) {
				size_t len = 0xc000 - start;
				bytes_read = (uint16_t)x16read(f, RAM + (((memory_get_ram_bank() % (uint16_t)Options.num_ram_banks) << 13) & 0xffffff) + start, sizeof(uint8_t), static_cast<unsigned int>(len));
				if (bytes_read < len)
					break;

				// Wrap into the next bank
				start = 0xa000;
				memory_set_ram_bank(1 + memory_get_ram_bank());
			}
		} else {
			// ROM
		}

		x16close(f);

		uint16_t end = start + bytes_read;
		state6502.x  = end & 0xff;
		state6502.y  = end >> 8;
		state6502.status &= 0xfe;
		RAM[STATUS] = 0;
		state6502.a = 0;
	}
}

void SAVE()
{
	char const *kernal_filename = (char *)&RAM[RAM[FNADR] | RAM[FNADR + 1] << 8];

	char      filename[PATH_MAX];
	const int len = MIN(RAM[FNLEN], PATH_MAX - 1);
	memcpy(filename, kernal_filename, len);
	filename[len] = '\0';

	std::filesystem::path filepath = Options.fsroot_path / filename;

	uint16_t start = RAM[state6502.a] | RAM[state6502.a + 1] << 8;
	uint16_t end   = state6502.x | state6502.y << 8;
	if (end < start) {
		state6502.status |= 1;
		state6502.a = 0;
		return;
	}
	char const *flags = "wb0";
	if (filepath.extension().generic_string() == ".gz") {
		flags = "wb6";
	}
	x16file *f = x16open(filepath.generic_string().c_str(), flags);
	if (f == nullptr) {
		state6502.a = 4; // FNF
		RAM[STATUS] = state6502.a;
		state6502.status |= 1;
		return;
	}

	x16write8(f, start & 0xff);
	x16write8(f, start >> 8);

	x16write(f, RAM + start, sizeof(uint8_t), end - start);
	x16close(f);

	state6502.status &= 0xfe;
	RAM[STATUS] = 0;
	state6502.a = 0;
}
