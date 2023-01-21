// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#ifndef VIA_H
#define VIA_H

#include <stdint.h>
#include <stdbool.h>

void    via1_init();
uint8_t via1_read(uint8_t reg, bool debug);
void    via1_write(uint8_t reg, uint8_t value);
void    via1_step(uint32_t clocks);
bool    via1_irq();

void    via2_init();
uint8_t via2_read(uint8_t reg, bool debug);
void    via2_write(uint8_t reg, uint8_t value);
void    via2_step(uint32_t clocks);
bool    via2_irq();

#endif
