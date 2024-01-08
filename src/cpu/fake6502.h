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
	uint8_t  sp_depth;
	uint8_t  sp_unwind_depth;
	uint8_t  sp, a, x, y, status;
};

enum class _stack_op_type : uint8_t {
	nmi,
	irq,
	op,
	smart,
};

enum class _stack_pop_type : uint8_t {
	unknown,
	rts,
	rti
};

enum class _push_op_type : uint8_t {
	unknown,
	a,
	x,
	y,
	status,
	smart,
};

struct _smart_stack_ex {
	_push_op_type push_type;
	_push_op_type pull_type;
	uint8_t       value;
	uint16_t      pc;
	uint8_t       bank;
};

struct _smart_stack {
	uint16_t        source_pc;
	uint16_t        dest_pc;
	uint8_t         source_bank;
	uint8_t         dest_bank;
	_stack_op_type  op_type;
	_stack_pop_type pop_type;
	uint16_t        pop_pc;
	uint8_t         pop_bank;
	uint8_t         opcode;
	uint8_t         push_depth;
	uint8_t         push_unwind_depth;
	_smart_stack_ex pushed_bytes[256];
	_state6502 		state;
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
