// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2022 Stephen Horn, et al.
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

#define ROM_SIZE (NUM_ROM_BANKS * 16384) /* banks at $C000-$FFFF */

extern _state6502 state6502;
extern uint8_t    waiting;

extern uint8_t *RAM;
extern uint8_t  ROM[ROM_SIZE];
extern uint32_t instructions;
extern uint8_t  debug6502;

extern bool save_on_exit;

extern void machine_dump();
extern void machine_reset();
extern void machine_toggle_warp();
extern void init_audio();

#endif
