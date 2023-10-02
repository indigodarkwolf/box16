// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include "memory.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "cpu/fake6502.h"
#include "debugger.h"
#include "files.h"
#include "gif_recorder.h"
#include "glue.h"
#include "hypercalls.h"
#include "vera/vera_video.h"
#include "via.h"
#include "wav_recorder.h"
#include "ym2151/ym2151.h"

#define RAM_SIZE (0xa000 + (uint32_t)Options.num_ram_banks * 8192) /* $0000-$9FFF + banks at $A000-$BFFF */

#define RAM_BANK (RAM[0])
#define ROM_BANK (rom_bank_register)

uint8_t *RAM;
uint8_t  ROM[ROM_SIZE];

uint8_t rom_bank_register;

#define RAM_WRITE_BLOCKS (((RAM_SIZE) + 0x3f) >> 6)
static uint64_t *RAM_written;

static uint8_t addr_ym = 0;

#define DEVICE_EMULATOR (0x9fb0)

//
// The idea behind this mapping scheme is to try and avoid chaining a bunch of
// if()'s by moving to a table lookup. We potentially *could* just have a 64K-sized
// table, but there's only a handful of ranges that we need to worry about, so we
// can easily express it as a trio of 256-entry tables, from which we need at most
// two lookups.
//

#define MEMMAP_NULL (0)
#define MEMMAP_DIRECT (1)
#define MEMMAP_RAMBANK (2)
#define MEMMAP_ROMBANK (3)
#define MEMMAP_IO (4)
#define MEMMAP_IO_SOUND (5)
#define MEMMAP_IO_VIDEO (6)
#define MEMMAP_IO_VIA1 (7)
#define MEMMAP_IO_VIA2 (8)
#define MEMMAP_IO_EMU (9)

struct memmap_table_entry {
	uint8_t entry_start;
	uint8_t final_entry;
	uint8_t memory_type;
};

// High byte mapping of memory
memmap_table_entry memmap_table_hi[] = {
	{ 0x00, 0x9f - 1, MEMMAP_DIRECT },
	{ 0x9f, 0xa0 - 1, MEMMAP_IO },
	{ 0xa0, 0xc0 - 1, MEMMAP_RAMBANK },
	{ 0xc0, 0xff, MEMMAP_ROMBANK },
};

// Low byte mapping for IO region
memmap_table_entry memmap_table_io[] = {
	{ 0x00, 0x10 - 1, MEMMAP_IO_VIA1 },
	{ 0x10, 0x20 - 1, MEMMAP_IO_VIA2 },
	{ 0x20, 0x40 - 1, MEMMAP_IO_VIDEO },
	{ 0x40, 0x42 - 1, MEMMAP_IO_SOUND },
	{ 0x42, 0x60 - 1, MEMMAP_NULL },
	{ 0x60, 0xb0 - 1, MEMMAP_NULL }, // External devices, currently mapped to NULL.
	{ 0xb0, 0xc0 - 1, MEMMAP_IO_EMU },
	{ 0xc0, 0xff, MEMMAP_NULL },
};

uint8_t memory_map_hi[0x100];
uint8_t memory_map_io[0x100];

static void build_memory_map(memmap_table_entry *table_entries, uint8_t *map)
{
	int e = 0;
	while (table_entries[e].final_entry < 0xff) {
		for (int i = table_entries[e].entry_start; i <= table_entries[e].final_entry; ++i) {
			map[i] = table_entries[e].memory_type;
		}
		++e;
	}
	for (int i = table_entries[e].entry_start; i <= table_entries[e].final_entry; ++i) {
		map[i] = table_entries[e].memory_type;
	}
}

static memory_init_params Memory_params;

//
// Initialization and re-initialization
//

void memory_init(const memory_init_params &init_params)
{
	Memory_params = init_params;

	const uint32_t ram_size = RAM_SIZE;
	RAM                     = new uint8_t[ram_size];
	if (Memory_params.randomize) {
		srand((uint32_t)SDL_GetPerformanceCounter());
		for (uint32_t i = 0; i < RAM_SIZE; ++i) {
			RAM[i] = rand();
		}
	} else {
		memset(RAM, 0, RAM_SIZE);
	}

	const uint32_t ram_write_blocks = RAM_WRITE_BLOCKS;
	RAM_written                     = new uint64_t[RAM_WRITE_BLOCKS];
	memset(RAM_written, 0, RAM_WRITE_BLOCKS * sizeof(uint64_t));

	build_memory_map(memmap_table_hi, memory_map_hi);
	build_memory_map(memmap_table_io, memory_map_io);

	memory_reset();
}

void memory_reset()
{
	// default banks are 0
	memory_set_ram_bank(0);
	memory_set_rom_bank(0);
}

//
// Banked RAM access
//

static uint8_t effective_ram_bank()
{
	return RAM_BANK % Options.num_ram_banks;
}

static uint8_t effective_rom_bank()
{
	return ROM_BANK % TOTAL_ROM_BANKS;
}


static uint8_t debug_ram_read(uint16_t address, uint8_t bank)
{
	const int ramBank      = bank % Options.num_ram_banks;
	const int real_address = (ramBank << 13) + address;
	return RAM[real_address];
}

static uint8_t real_ram_read(uint16_t address)
{
	const int ramBank      = effective_ram_bank();
	const int real_address = (ramBank << 13) + address;

	if ((RAM_written[real_address >> 6] & ((uint64_t)1 << (real_address & 0x3f))) == 0 && Memory_params.enable_uninitialized_access_warning) {
		printf("Warning: %02X:%04X accessed uninitialized RAM address %02X:%04X\n", state6502.pc < 0xa000 ? 0 : ramBank, state6502.pc, address < 0xa000 ? 0 : ramBank, address);
	}

	return RAM[real_address];
}

static void debug_ram_write(uint16_t address, uint8_t bank, uint8_t value)
{
	RAM[((uint32_t)bank << 13) + address] = value;
}

static void real_ram_write(uint16_t address, uint8_t value)
{
	const int ramBank      = effective_ram_bank();
	const int real_address = (ramBank << 13) + address;

	RAM_written[real_address >> 6] |= (uint64_t)1 << (real_address & 0x3f);

	RAM[real_address] = value;

	if (address == 1) ROM_BANK = value;
}

//
// Trivial ROM access
//

static uint8_t debug_rom_read(uint16_t address, uint8_t bank)
{
	const int romBank = bank % TOTAL_ROM_BANKS;
	return ROM[(romBank << 14) + address - 0xc000];
}

static uint8_t real_rom_read(uint16_t address)
{
	return ROM[(ROM_BANK << 14) + address - 0xc000];
}

static void debug_rom_write(uint16_t address, uint8_t bank, uint8_t value)
{
	if (bank >= NUM_ROM_BANKS) {
		ROM[((uint32_t)bank << 14) + address - 0xc000] = value;
	}
}

static void real_rom_write(uint16_t address, uint8_t value)
{	
	const int romBank = effective_rom_bank();
	if (romBank >= NUM_ROM_BANKS) {
		const int real_address = (romBank << 14) + address - 0xc000;

		ROM[real_address] = value;

		//printf("Writing to hidden ram at addr: $%hx, bank $%hhx\n", address, romBank);
	}
	
}

//
// Emulator state
//

uint8_t debug_emu_read(uint8_t reg)
{
	switch (reg) {
		case 0: return 1; // debugger enabled?
		case 1: return vera_video_get_log_video() ? 1 : 0;
		case 2: return Options.log_keyboard ? 1 : 0;
		case 3: return (int)Options.echo_mode;
		case 4: return save_on_exit ? 1 : 0;
		case 5: return gif_recorder_get_state();
		case 6: return wav_recorder_get_state();
		case 7: return Options.no_keybinds ? 1 : 0;
		case 8: return (clockticks6502 >> 0) & 0xff;
		case 9: return (clockticks6502 >> 8) & 0xff;
		case 10: return (clockticks6502 >> 16) & 0xff;
		case 11: return (clockticks6502 >> 24) & 0xff;
		// case 12: return -1;
		case 13: return Options.keymap;
		case 14: return '1'; // emulator detection
		case 15: return '6'; // emulator detection
		default: return -1;
	}
}

uint8_t real_emu_read(uint8_t reg)
{
	switch (reg) {
		case 0: return 1; // debugger enabled?
		case 1: return vera_video_get_log_video() ? 1 : 0;
		case 2: return Options.log_keyboard ? 1 : 0;
		case 3: return (int)Options.echo_mode;
		case 4: return save_on_exit ? 1 : 0;
		case 5: return gif_recorder_get_state();
		case 6: return wav_recorder_get_state();
		case 7: return Options.no_keybinds ? 1 : 0;
		case 8: return (clockticks6502 >> 0) & 0xff;
		case 9: return (clockticks6502 >> 8) & 0xff;
		case 10: return (clockticks6502 >> 16) & 0xff;
		case 11: return (clockticks6502 >> 24) & 0xff;
		// case 12: return -1;
		case 13: return Options.keymap;
		case 14: return '1'; // emulator detection
		case 15: return '6'; // emulator detection
		default:
			printf("WARN: Invalid register %x\n", DEVICE_EMULATOR + reg);
			return -1;
	}
}

void emu_write(uint8_t reg, uint8_t value)
{
	const bool v = (value != 0);
	switch (reg) {
		case 0: break; // debugger_enabled = v; break;
		case 1: vera_video_set_log_video(v); break;
		case 2: Options.log_keyboard = v; break;
		case 3:
			Options.echo_mode = static_cast<echo_mode_t>(value);
			hypercalls_update();
			break;
		case 4: save_on_exit = v; break;
		case 5: gif_recorder_set((gif_recorder_command_t)value); break;
		case 6: wav_recorder_set((wav_recorder_command_t)value); break;
		case 7: Options.no_keybinds = v; break;
		// case 8: clock_base = clockticks6502; break;
		case 9: printf("User debug 1: $%02x\n", value); break;
		case 10: printf("User debug 2: $%02x\n", value); break;
		case 11: {
		    if (value == 0x09 || value == 0x0a || value == 0x0d || (value >= 0x20 && value < 0x7f)) {
			putchar(value);
		    } else if (value >= 0xa1) {
			putchar(value);   // print_iso8859_15_char((char) value);
		    } else {
			printf("\xef\xbf\xbd"); // �
		    }
		    fflush(stdout);
		    break;
		}
		default: break; // printf("WARN: Invalid register %x\n", DEVICE_EMULATOR + reg);
	}
}

//
// Other IO helpers
//

static void sound_write(uint16_t address, uint8_t value)
{
	YM_write(static_cast<uint8_t>(address & 1), value);
}

static uint8_t sound_read(uint16_t address)
{
	address = address & 0x01;
	if (address == 0) {
		return 0;
	} else {
		return YM_read_status();
	}
}

//
// Memory Table Access
//

template <const uint8_t MAP[100], uint8_t BYTE>
static void real_write(uint16_t address, uint8_t value);

template <const uint8_t MAP[100], uint8_t BYTE>
static uint8_t debug_read(uint16_t address, uint8_t bank)
{
	switch (MAP[(address >> (BYTE * 8)) & 0xff]) {
		case MEMMAP_NULL: return 0;
		case MEMMAP_DIRECT: return RAM[address];
		case MEMMAP_RAMBANK: return debug_ram_read(address, bank);
		case MEMMAP_ROMBANK: return debug_rom_read(address, bank);
		case MEMMAP_IO: return debug_read<memory_map_io, 0>(address, bank);
		case MEMMAP_IO_SOUND: return 0;
		case MEMMAP_IO_VIDEO: return vera_debug_video_read(address & 0x1f);
		case MEMMAP_IO_VIA1: return via1_read(address & 0xf, true);
		case MEMMAP_IO_VIA2: return via2_read(address & 0xf, true);
		case MEMMAP_IO_EMU: return debug_emu_read(address & 0xf);
		default: return 0;
	}
}

template <const uint8_t MAP[100], uint8_t BYTE>
static uint8_t real_read(uint16_t address)
{
	switch (MAP[(address >> (BYTE * 8)) & 0xff]) {
		case MEMMAP_NULL: return 0;
		case MEMMAP_DIRECT: return RAM[address];
		case MEMMAP_RAMBANK: return real_ram_read(address); break;
		case MEMMAP_ROMBANK: return real_rom_read(address); break;
		case MEMMAP_IO: return real_read<memory_map_io, 0>(address);
		case MEMMAP_IO_SOUND: return sound_read(address);
		case MEMMAP_IO_VIDEO: return vera_video_read(address & 0x1f);
		case MEMMAP_IO_VIA1: return via1_read(address & 0xf, false);
		case MEMMAP_IO_VIA2: return via2_read(address & 0xf, false);
		case MEMMAP_IO_EMU: return real_emu_read(address & 0xf);
		default: return 0;
	}
}

template <const uint8_t MAP[100], uint8_t BYTE>
static void debug_write(uint16_t address, uint8_t bank, uint8_t value)
{
	switch (MAP[(address >> (BYTE * 8)) & 0xff]) {
		case MEMMAP_NULL: break;
		case MEMMAP_DIRECT: RAM[address] = value; if (address == 1) ROM_BANK = value; break;
		case MEMMAP_RAMBANK: debug_ram_write(address, bank, value); break;
		case MEMMAP_ROMBANK: debug_rom_write(address, bank, value); break;
		case MEMMAP_IO: real_write<memory_map_io, 0>(address, value); break;
		case MEMMAP_IO_SOUND: sound_write(address & 0x1f, value); break; // TODO: Sound
		case MEMMAP_IO_VIDEO: vera_video_write(address & 0x1f, value); break;
		case MEMMAP_IO_VIA1: via1_write(address & 0xf, value); break;
		case MEMMAP_IO_VIA2: via2_write(address & 0xf, value); break;
		case MEMMAP_IO_EMU: emu_write(address & 0xf, value); break;
		default: break;
	}
}

template <const uint8_t MAP[100], uint8_t BYTE>
static void real_write(uint16_t address, uint8_t value)
{
	switch (MAP[(address >> (BYTE * 8)) & 0xff]) {
		case MEMMAP_NULL: break;
		case MEMMAP_DIRECT: RAM[address] = value; if (address == 1) ROM_BANK = value; break;
		case MEMMAP_RAMBANK: real_ram_write(address, value); break;
		case MEMMAP_ROMBANK: real_rom_write(address, value); break;
		case MEMMAP_IO: real_write<memory_map_io, 0>(address, value); break;
		case MEMMAP_IO_SOUND: sound_write(address & 0x1f, value); break; // TODO: Sound
		case MEMMAP_IO_VIDEO: vera_video_write(address & 0x1f, value); break;
		case MEMMAP_IO_VIA1: via1_write(address & 0xf, value); break;
		case MEMMAP_IO_VIA2: via2_write(address & 0xf, value); break;
		case MEMMAP_IO_EMU: emu_write(address & 0xf, value); break;
		default: break;
	}
}

//
// interface for fake6502
//

uint8_t debug_read6502(uint16_t address)
{
	return debug_read6502(address, address >= 0xc000 ? memory_get_rom_bank() : memory_get_ram_bank());
}

uint8_t debug_read6502(uint16_t address, uint8_t bank)
{
	return debug_read<memory_map_hi, 1>(address, bank);
}

uint8_t read6502(uint16_t address)
{
	debug6502 |= (DEBUG6502_READ | DEBUG6502_EXEC) & debugger_get_flags(address, address >= 0xc000 ? memory_get_rom_bank() : memory_get_ram_bank());

	uint8_t value = real_read<memory_map_hi, 1>(address);
#if defined(TRACE)
	if (Options.log_mem_read)
		printf("%04X -> %02X\n", address, value);
#endif
	return value;
}

void debug_write6502(uint16_t address, uint8_t bank, uint8_t value)
{
	debug_write<memory_map_hi, 1>(address, bank, value);
}

void write6502(uint16_t address, uint8_t value)
{
	debug6502 |= DEBUG6502_WRITE & debugger_get_flags(address, address >= 0xc000 ? memory_get_rom_bank() : memory_get_ram_bank());
	if (~debug6502 & DEBUG6502_WRITE) {
#if defined(TRACE)
		if (Options.log_mem_write)
			printf("%02X -> %04X\n", value, address);
#endif
		real_write<memory_map_hi, 1>(address, value);
	}
}

uint8_t bank6502(uint16_t address)
{
	return memory_get_current_bank(address);
}

void vp6502(void)
{
	ROM_BANK = 0;
}

//
// saves the memory content into a file
//

void memory_save(x16file *f, bool dump_ram, bool dump_bank)
{
	if (dump_ram) {
		x16write(f, &RAM[0], sizeof(uint8_t), 0xa000);
	}
	if (dump_bank) {
		x16write(f, &RAM[0xa000], sizeof(uint8_t), (Options.num_ram_banks * 8192));
	}
}

//
// Banking access/mutates
//

void memory_set_ram_bank(uint8_t bank)
{
	RAM_BANK = bank & (NUM_MAX_RAM_BANKS - 1);
}

uint8_t memory_get_ram_bank()
{
	return RAM_BANK;
}

void memory_set_rom_bank(uint8_t bank)
{
	ROM_BANK = bank & (TOTAL_ROM_BANKS - 1);
}

uint8_t memory_get_rom_bank()
{
	return ROM_BANK;
}

uint8_t memory_get_current_bank(uint16_t address)
{
	if (address >= 0xc000) {
		return memory_get_rom_bank();
	} else if (address >= 0xa000) {
		return memory_get_ram_bank();
	} else {
		return 0;
	}
}
