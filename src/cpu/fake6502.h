// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#ifndef FAKE6502_H
#define FAKE6502_H

#include <stdint.h>

#define DEBUG6502_EXEC 0x1
#define DEBUG6502_READ 0x2
#define DEBUG6502_WRITE 0x4

struct _state6502 {
	uint16_t pc;
	uint8_t  sp, a, x, y, status;
};

enum class _stack_op_type : uint8_t {
	nmi = 0,
	irq,
	jsr,
	smart,
	push_op,
	push_nmi,
	push_irq,
	push_jsr,
	rts,
	rti,
	hypercall,
	pull_op,
	unknown
};

struct _smart_stack {
	struct {
		_stack_op_type op_type;
		_state6502     state;
		uint8_t        pc_bank;

		struct _op_data {
			uint8_t opcode;
			uint8_t value;
		};

		struct _jmp_data {
			uint16_t dest_pc;
			uint8_t  dest_bank;
		};

		union {
 			_op_data op_data;
			_jmp_data jmp_data;
		};
	} push, pop;
};

struct _cpuhistory {
	_state6502 state;
	uint8_t    bank;
	uint8_t    opcode;
};

extern void     init6502();
extern void     reset6502();
extern void     step6502();
extern void     force6502();
extern void     exec6502(uint32_t tickcount);
extern void     nmi6502();
extern void     irq6502();
extern uint64_t clockticks6502;
extern uint8_t  debug6502;

#endif
