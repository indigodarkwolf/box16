// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#ifndef GLUE_H
#define GLUE_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu/fake6502.h"
#include "options.h"

#define LOAD_HYPERCALLS
//#define TRACE
//#define PROFILE

#define MHZ 8

#define NUM_ROM_BANKS 32

#define HIDDEN_RAM_BANKS (256 - 32)
#define TOTAL_ROM_BANKS (NUM_ROM_BANKS + HIDDEN_RAM_BANKS)

#define ROM_SIZE (TOTAL_ROM_BANKS * 16384) /* banks at $C000-$FFFF */

extern _state6502   state6502;
extern uint8_t      waiting;
extern _smart_stack stack6502[256];
extern uint8_t      stack6502_underflow;

extern uint8_t *RAM;
extern uint8_t  ROM[ROM_SIZE];
extern uint32_t instructions;
extern uint8_t  debug6502;

extern bool save_on_exit;

extern void machine_dump(const char *reason);
extern void machine_reset();
extern void machine_toggle_warp();
extern void init_audio();
extern void main_shutdown();


#endif
