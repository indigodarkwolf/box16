// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#ifndef MEMORY_H
#define MEMORY_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "files.h"

#define NUM_MAX_RAM_BANKS 256

struct memory_init_params {
	uint16_t num_banks;
	bool     randomize;
	bool     enable_uninitialized_access_warning;
};

void memory_init(const memory_init_params &params);
void memory_reset();

uint8_t debug_read6502(uint16_t address);
uint8_t debug_read6502(uint16_t address, uint8_t bank);
uint8_t read6502(uint16_t address);
void    debug_write6502(uint16_t address, uint8_t bank, uint8_t value);
void    write6502(uint16_t address, uint8_t value);
uint8_t bank6502(uint16_t address);
void    memory_save(x16file *f, bool dump_ram, bool dump_bank);
void    vp6502(void);

void memory_set_ram_bank(uint8_t bank);
void memory_set_rom_bank(uint8_t bank);

uint8_t memory_get_ram_bank();
uint8_t memory_get_rom_bank();

uint8_t memory_get_current_bank(uint16_t address);

#endif
