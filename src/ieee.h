// Commander X16 Emulator
// Copyright (c) 2022 Michael Steil
// All rights reserved. License: 2-clause BSD

#pragma once
#if !defined(IEEE_H)
#	define IEEE_H

void ieee_init();
void SECOND(uint8_t a);
void TKSA(uint8_t a);
int  ACPTR(uint8_t *a);
int  CIOUT(uint8_t a);
void UNTLK();
int  UNLSN();
void LISTEN(uint8_t a);
void TALK(uint8_t a);
int  MACPTR(uint16_t addr, uint16_t *count);

#endif
