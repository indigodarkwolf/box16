// Commander X16 Emulator
// Copyright (c) 2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include "hypercalls.h"

#include "glue.h"
#include "ieee.h"
#include "keyboard.h"
#include "loadsave.h"
#include "memory.h"
#include "options.h"
#include "rom_symbols.h"
#include "unicode.h"
#include "vera/sdcard.h"

#define KERNAL_MACPTR (0xff44)
#define KERNAL_SECOND (0xff93)
#define KERNAL_TKSA (0xff96)
#define KERNAL_ACPTR (0xffa5)
#define KERNAL_CIOUT (0xffa8)
#define KERNAL_UNTLK (0xffab)
#define KERNAL_UNLSN (0xffae)
#define KERNAL_LISTEN (0xffb1)
#define KERNAL_TALK (0xffb4)
#define KERNAL_CHRIN (0xffcf)
#define KERNAL_CHROUT (0xffd2)
#define KERNAL_LOAD (0xffd5)
#define KERNAL_SAVE (0xffd8)
#define KERNAL_CRASH (0xffff)

static uint16_t Kernal_status  = 0;
static bool     Has_boot_tasks = false;

static bool (*Hypercall_table[0x100])(void);

static bool is_kernal()
{
	const uint8_t rom_bank = memory_get_rom_bank();

	return debug_read6502(0xfff6, rom_bank) == 'M' && // only for KERNAL
	       debug_read6502(0xfff7, rom_bank) == 'I' &&
	       debug_read6502(0xfff8, rom_bank) == 'S' &&
	       debug_read6502(0xfff9, rom_bank) == 'T';
}

static bool init_kernal_status()
{
	// There is no KERNAL API to write the STATUS variable.
	// But there is code to read it, READST, which should
	// always look like this:
	// 00:.,d6a0 ad 89 02 lda $0289
	// 00:.,d6a3 0d 89 02 ora $0289
	// 00:.,d6a6 8d 89 02 sta $0289
	// We can extract the location of the STATUS variable
	// from it.

	// JMP in the KERNAL API vectors
	if (debug_read6502(0xffb7, 0) != 0x4c) {
		return false;
	}
	// target of KERNAL API vector JMP
	uint16_t readst = debug_read6502(0xffb8, 0) | debug_read6502(0xffb9, 0) << 8;
	if (readst < 0xc000) {
		return false;
	}
	// ad 89 02 lda $0289
	if (debug_read6502(readst, 0) != 0xad) {
		return false;
	}
	// ad 89 02 lda $0289
	if (debug_read6502(readst + 3, 0) != 0x0d) {
		return false;
	}
	// ad 89 02 lda $0289
	if (debug_read6502(readst + 6, 0) != 0x8d) {
		return false;
	}
	uint16_t status0 = debug_read6502(readst + 1, 0) | debug_read6502(readst + 2, 0) << 8;
	uint16_t status1 = debug_read6502(readst + 4, 0) | debug_read6502(readst + 5, 0) << 8;
	uint16_t status2 = debug_read6502(readst + 7, 0) | debug_read6502(readst + 8, 0) << 8;
	// all three addresses must be the same
	if (status0 != status1 || status0 != status2) {
		return false;
	}

	Kernal_status = status0;
	return true;
}

static bool set_kernal_status(uint8_t s)
{
	// everything okay, write the status!
	RAM[Kernal_status] = s;
	return true;
}

static bool ieee_hypercalls_allowed()
{
	if (Options.no_ieee_hypercalls) {
		return false;
	}

	if (Options.enable_serial) {
		// if we do bit-level serial bus emulation, we don't
		// do high-level KERNAL IEEE API interception
		return false;
	}

	if (sdcard_is_attached()) {
		// if should emulate an SD card, we'll always skip host fs
		return false;
	}

	return true;
}

bool hypercalls_init()
{
	if (!init_kernal_status()) {
		return false;
	}

	// Setup whether we have boot tasks
	if (!Options.prg_path.empty()) {
		Has_boot_tasks = true;
	}

	if (!Options.bas_path.empty()) {
		Has_boot_tasks = true;
	}

	if (Options.run_geos) {
		Has_boot_tasks = true;
	}

	if (Options.run_test) {
		Has_boot_tasks = true;
	}

	hypercalls_update();

	return true;
}

void hypercalls_update()
{
	memset(Hypercall_table, 0, sizeof(Hypercall_table));

	if (ieee_hypercalls_allowed()) {
		Hypercall_table[KERNAL_MACPTR & 0xff] = []() -> bool {
			uint16_t count = a;

			const uint8_t s = MACPTR(y << 8 | x, &count);

			x = count & 0xff;
			y = count >> 8;
			status &= 0xfe; // clear C -> supported

			set_kernal_status(s);
			return true;
		};

		Hypercall_table[KERNAL_SECOND & 0xff] = []() -> bool {
			SECOND(a);
			return true;
		};

		Hypercall_table[KERNAL_TKSA & 0xff] = []() -> bool {
			TKSA(a);
			return true;
		};

		Hypercall_table[KERNAL_ACPTR & 0xff] = []() -> bool {
			const uint8_t s = ACPTR(&a);

			status = (status & ~2) | (!a << 1);

			set_kernal_status(s);
			return true;
		};

		Hypercall_table[KERNAL_CIOUT & 0xff] = []() -> bool {
			const uint8_t s = CIOUT(a);

			set_kernal_status(s);
			return true;
		};

		Hypercall_table[KERNAL_UNTLK & 0xff] = []() -> bool {
			UNTLK();
			return true;
		};

		Hypercall_table[KERNAL_UNLSN & 0xff] = []() -> bool {
			const uint8_t s = UNLSN();

			set_kernal_status(s);
			return true;
		};

		Hypercall_table[KERNAL_LISTEN & 0xff] = []() -> bool {
			LISTEN(a);
			return true;
		};

		Hypercall_table[KERNAL_TALK & 0xff] = []() -> bool {
			TALK(a);
			return true;
		};
	}

	if (!sdcard_is_attached()) {
		Hypercall_table[KERNAL_LOAD & 0xff] = []() -> bool {
			if (RAM[FA] == 8) {
				LOAD();
				return true;
			}
			return false;
		};

		Hypercall_table[KERNAL_SAVE & 0xff] = []() -> bool {
			if (RAM[FA] == 8) {
				SAVE();
				return true;
			}
			return false;
		};
	}

	if (Has_boot_tasks) {
		Hypercall_table[KERNAL_CHRIN & 0xff] = []() -> bool {
			// as soon as BASIC starts reading a line...
			if (!Options.prg_path.empty()) {
				std::filesystem::path prg_path;
				options_get_hyper_path(prg_path, Options.prg_path);

				SDL_RWops *prg_file = SDL_RWFromFile(prg_path.generic_string().c_str(), "rb");
				if (!prg_file) {
					printf("Cannot open PRG file %s (%s)!\n", prg_path.generic_string().c_str(), std::filesystem::absolute(prg_path).generic_string().c_str());
					exit(1);
				}

				// ...inject the app into RAM
				uint8_t  start_lo = SDL_ReadU8(prg_file);
				uint8_t  start_hi = SDL_ReadU8(prg_file);
				uint16_t start;
				if (Options.prg_override_start > 0) {
					start = Options.prg_override_start;
				} else {
					start = start_hi << 8 | start_lo;
				}
				uint16_t end = start + (uint16_t)SDL_RWread(prg_file, RAM + start, 1, 65536 - start);
				SDL_RWclose(prg_file);
				prg_file = nullptr;
				if (start == 0x0801) {
					// set start of variables
					RAM[VARTAB]     = end & 0xff;
					RAM[VARTAB + 1] = end >> 8;
				}

				if (Options.run_after_load) {
					if (start == 0x0801) {
						keyboard_add_text("RUN\r");
					} else {
						char sys_text[10];
						sprintf(sys_text, "SYS$%04X\r", start);
						keyboard_add_text(sys_text);
					}
				}
			}

			if (!Options.bas_path.empty()) {
				keyboard_add_file(Options.bas_path.generic_string().c_str());
				if (Options.run_after_load) {
					keyboard_add_text("RUN\r");
				}
			}

			if (Options.run_geos) {
				keyboard_add_text("GEOS\r");
			}

			if (Options.run_test) {
				char test_text[256];
				sprintf(test_text, "TEST %d\r", Options.test_number);
				keyboard_add_text(test_text);
			}

			Has_boot_tasks = false;
			hypercalls_update();
			return false;
		};
	}

	if (Options.echo_mode != echo_mode_t::ECHO_MODE_NONE) {
		Hypercall_table[KERNAL_CHROUT & 0xff] = []() -> bool {
			uint8_t c = a;
			if (Options.echo_mode == echo_mode_t::ECHO_MODE_COOKED) {
				if (c == 0x0d) {
					printf("\n");
				} else if (c == 0x0a) {
					// skip
				} else if (c < 0x20 || c >= 0x80) {
					printf("\\X%02X", c);
				} else {
					printf("%c", c);
				}
			} else if (Options.echo_mode == echo_mode_t::ECHO_MODE_ISO) {
				if (c == 0x0d) {
					printf("\n");
				} else if (c == 0x0a) {
					// skip
				} else if (c < 0x20 || (c >= 0x80 && c < 0xa0)) {
					printf("\\X%02X", c);
				} else {
					print_iso8859_15_char(c);
				}
			} else {
				printf("%c", c);
			}
			fflush(stdout);
			return false;
		};
	}
}

void hypercalls_process()
{
	if (!is_kernal() || pc < 0xFF44) {
		return;
	}

	const auto hypercall = Hypercall_table[pc & 0xff];
	if (hypercall != nullptr) {
		if (hypercall()) {
			pc = (RAM[0x100 + sp + 1] | (RAM[0x100 + sp + 2] << 8)) + 1;
			sp += 2;
		}
	}
}
