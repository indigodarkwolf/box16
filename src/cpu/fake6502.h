// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef FAKE6502_H
#define FAKE6502_H

#include <stdint.h>

extern void     reset6502();
extern void     step6502();
extern void     exec6502(uint32_t tickcount);
extern void     nmi6502();
extern void     irq6502();
extern uint64_t clockticks6502;

#endif
