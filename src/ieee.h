// Commander X16 Emulator
// Copyright (c) 2022 Michael Steil
// All rights reserved. License: 2-clause BSD

#pragma once
#if !defined(IEEE_H)
#	define IEEE_H

void ieee_init();
int  SECOND(uint8_t a);
int  TKSA(uint8_t a);
int  ACPTR(uint8_t *a);
int  CIOUT(uint8_t a);
int  UNTLK();
int  UNLSN();
int  LISTEN(uint8_t a);
int  TALK(uint8_t a);
int  MACPTR(uint16_t addr, uint16_t *count, uint8_t stream_mode);
int  MCIOUT(uint16_t addr, uint16_t *count, uint8_t stream_mode);

#endif
