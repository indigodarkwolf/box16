// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef GLUE_H
#define GLUE_H

#include <stdbool.h>
#include <stdint.h>

#include "options.h"

#define LOAD_HYPERCALLS
//#define TRACE
#define TRACE_VIA

#define MHZ 8

#define NUM_MAX_RAM_BANKS 256
#define NUM_ROM_BANKS 32

#define RAM_SIZE (0xa000 + (uint32_t)Options.num_ram_banks * 8192) /* $0000-$9FFF + banks at $A000-$BFFF */
#define ROM_SIZE (NUM_ROM_BANKS * 16384)                   /* banks at $C000-$FFFF */

extern uint8_t  a, x, y, sp, status;
extern uint16_t pc;
extern uint8_t *RAM;
extern uint8_t  ROM[ROM_SIZE];
extern uint32_t instructions;

extern bool        save_on_exit;

extern void machine_dump();
extern void machine_reset();
extern void machine_toggle_warp();
extern void init_audio();

#endif
