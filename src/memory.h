// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef MEMORY_H
#define MEMORY_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

void memory_init();
void memory_reset();

uint8_t debug_read6502(uint16_t address);
uint8_t debug_read6502(uint16_t address, uint8_t bank);
uint8_t read6502(uint16_t address);
void    debug_write6502(uint16_t address, uint8_t bank, uint8_t value);
void    write6502(uint16_t address, uint8_t value);

void memory_save(SDL_RWops *f, bool dump_ram, bool dump_bank);

void memory_set_ram_bank(uint8_t bank);
void memory_set_rom_bank(uint8_t bank);

uint8_t memory_get_ram_bank();
uint8_t memory_get_rom_bank();

uint8_t memory_get_current_bank(uint16_t address);

#endif
