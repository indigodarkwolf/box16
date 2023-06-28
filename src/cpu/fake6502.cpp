/* Fake6502 CPU emulator core v1.1 *******************
 * (c)2011 Mike Chambers (miker00lz@gmail.com)       *
 *****************************************************
 * v1.1 - Small bugfix in BIT opcode, but it was the *
 *        difference between a few games in my NES   *
 *        emulator working and being broken!         *
 *        I went through the rest carefully again    *
 *        after fixing it just to make sure I didn't *
 *        have any other typos! (Dec. 17, 2011)      *
 *                                                   *
 * v1.0 - First release (Nov. 24, 2011)              *
 *****************************************************
 * LICENSE: This source code is released into the    *
 * public domain, but if you use it please do give   *
 * credit. I put a lot of effort into writing this!  *
 *                                                   *
 *****************************************************
 * Fake6502 is a MOS Technology 6502 CPU emulation   *
 * engine in C. It was written as part of a Nintendo *
 * Entertainment System emulator I've been writing.  *
 *                                                   *
 * If you do discover an error in timing accuracy,   *
 * or operation in general please e-mail me at the   *
 * address above so that I can fix it. Thank you!    *
 *                                                   *
 *****************************************************
 * Usage:                                            *
 *                                                   *
 * Fake6502 requires you to provide two external     *
 * functions:                                        *
 *                                                   *
 * uint8_t read6502(uint16_t address)                *
 * void write6502(uint16_t address, uint8_t value)   *
 *                                                   *
 *****************************************************
 * Useful functions in this emulator:                *
 *                                                   *
 * void reset6502()                                  *
 *   - Call this once before you begin execution.    *
 *                                                   *
 * void exec6502(uint32_t tickcount)                 *
 *   - Execute 6502 code up to the next specified    *
 *     count of clock ticks.                         *
 *                                                   *
 * void step6502()                                   *
 *   - Execute a single instrution.                  *
 *                                                   *
 * void irq6502()                                    *
 *   - Trigger a hardware IRQ in the 6502 core.      *
 *                                                   *
 * void nmi6502()                                    *
 *   - Trigger an NMI in the 6502 core.              *
 *                                                   *
 *****************************************************
 * Useful variables in this emulator:                *
 *                                                   *
 * uint64_t clockticks6502                           *
 *   - A running total of the emulated cycle count.  *
 *                                                   *
 * uint32_t instructions                             *
 *   - A running total of the total emulated         *
 *     instruction count. This is not related to     *
 *     clock cycle timing.                           *
 *                                                   *
 *****************************************************/

#include "fake6502.h"

#include "../debugger.h"
#include <stdint.h>
#include <stdio.h>
#include <ring_buffer.h>

#define FLAG_CARRY 0x01
#define FLAG_ZERO 0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL 0x08
#define FLAG_BREAK 0x10
#define FLAG_CONSTANT 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_SIGN 0x80

#define BASE_STACK 0x100

// 6502 CPU registers
_state6502 state6502, debug_state6502;

// helper variables
uint32_t instructions   = 0; // keep track of total instructions executed
uint64_t clockticks6502 = 0, clockgoal6502 = 0;
uint16_t oldpc, ea, reladdr, value, result;
uint8_t  opcode, oldstatus;
uint8_t  debug6502 = 0;

uint8_t penaltyop, penaltyaddr;
uint8_t waiting = 0;

ring_buffer<_smart_stack, 256>  stack6502;

// externally supplied functions
extern uint8_t read6502(uint16_t address);
extern void    write6502(uint16_t address, uint8_t value);
extern uint8_t bank6502(uint16_t address);
extern void    vp6502(void);

static uint16_t getvalue();
static void     putvalue(uint16_t saveval);

#include "instructions_6502.h"
#include "instructions_65c02.h"
#include "modes.h"
#include "tables.h"

static uint16_t getvalue()
{
	if (addrtable[opcode] == acc)
		return ((uint16_t)state6502.a);
	else
		return ((uint16_t)read6502(ea));
}

static void putvalue(uint16_t saveval)
{
	if (addrtable[opcode] == acc)
		state6502.a = (uint8_t)(saveval & 0x00FF);
	else
		write6502(ea, (saveval & 0x00FF));
}

void nmi6502()
{
	auto &ss     = stack6502.allocate();
	ss.source_pc = state6502.pc;
	ss.source_bank = bank6502(state6502.pc);
	ss.state       = state6502;

	push16(state6502.pc);
	push8(state6502.status & ~FLAG_BREAK);
	setinterrupt();
	cleardecimal();
	vp6502();
	state6502.pc = (uint16_t)read6502(0xFFFA) | ((uint16_t)read6502(0xFFFB) << 8);
	waiting      = 0;

	ss.dest_pc = state6502.pc;
	ss.dest_bank = bank6502(state6502.pc);
	ss.op_type = _stack_op_type::nmi;
	ss.opcode  = 0;
}

void irq6502()
{
	if (!(state6502.status & FLAG_INTERRUPT)) {
		auto &ss     = stack6502.allocate();
		ss.source_pc = state6502.pc;
		ss.source_bank = bank6502(state6502.pc);
		ss.state       = state6502;
		push16(state6502.pc);
		push8(state6502.status & ~FLAG_BREAK);
		setinterrupt();
		cleardecimal();
		vp6502();
		state6502.pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);

		ss.dest_pc = state6502.pc;
		ss.dest_bank = bank6502(state6502.pc);
		ss.op_type   = _stack_op_type::irq;
		ss.opcode  = 0;
	}
	waiting = 0;
}

void exec6502(uint32_t tickcount)
{
	debug6502 = 0;

	if (waiting) {
		clockticks6502 += tickcount;
		clockgoal6502 = clockticks6502;
		return;
	}

	clockgoal6502 += tickcount;

	while (clockticks6502 < clockgoal6502) {
		debug_state6502                     = state6502;
		const uint64_t debug_clockticks6502 = clockticks6502;

		opcode = read6502(state6502.pc++);
		if (debug6502 & DEBUG6502_EXEC) {
			state6502      = debug_state6502;
			clockticks6502 = debug_clockticks6502;
			return;
		}
		state6502.status |= FLAG_CONSTANT;

		penaltyop   = 0;
		penaltyaddr = 0;

		(*addrtable[opcode])();
		(*optable[opcode])();

		if (debug6502 & (DEBUG6502_READ | DEBUG6502_WRITE)) {
			state6502      = debug_state6502;
			clockticks6502 = debug_clockticks6502;
			return;
		}

		clockticks6502 += ticktable[opcode];
		if (penaltyop && penaltyaddr)
			clockticks6502++;

		instructions++;
		debug6502 = 0;
	}
}

void step6502()
{
	debug6502 = 0;

	if (waiting) {
		++clockticks6502;
		clockgoal6502 = clockticks6502;
		return;
	}

	debug_state6502                     = state6502;
	const uint64_t debug_clockticks6502 = clockticks6502;

	opcode = read6502(state6502.pc++);
	if (debug6502 & DEBUG6502_EXEC) {
		state6502      = debug_state6502;
		clockticks6502 = debug_clockticks6502;
		return;
	}
	state6502.status |= FLAG_CONSTANT;

	penaltyop   = 0;
	penaltyaddr = 0;

	(*addrtable[opcode])();
	(*optable[opcode])();

	if (debug6502 & (DEBUG6502_READ | DEBUG6502_WRITE)) {
		state6502      = debug_state6502;
		clockticks6502 = debug_clockticks6502;
		return;
	}

	clockticks6502 += ticktable[opcode];
	if (penaltyop && penaltyaddr)
		clockticks6502++;
	clockgoal6502 = clockticks6502;

	instructions++;
	debug6502 = 0;
}

void force6502()
{
	debug6502 = 0;

	if (waiting) {
		++clockticks6502;
		clockgoal6502 = clockticks6502;
		return;
	}

	opcode = read6502(state6502.pc++);
	state6502.status |= FLAG_CONSTANT;

	penaltyop   = 0;
	penaltyaddr = 0;

	(*addrtable[opcode])();
	(*optable[opcode])();

	clockticks6502 += ticktable[opcode];
	if (penaltyop && penaltyaddr)
		clockticks6502++;
	clockgoal6502 = clockticks6502;

	instructions++;
}

//  Fixes from http://6502.org/tutorials/65c02opcodes.html
//
//  65C02 Cycle Count differences.
//        ADC/SBC work differently in decimal mode.
//        The wraparound fixes may not be required.
