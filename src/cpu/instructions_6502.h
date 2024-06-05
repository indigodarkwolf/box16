#if !defined(INSTRUCTIONS_6502_H)
#	define INSTRUCTIONS_6502_H

#	include "support.h"

/*

                        Extracted from original single fake6502.c file

*/
//
//          65C02 changes.
//
//          BRK                 now clears D
//          ADC/SBC             set N and Z in decimal mode. They also set V, but this is
//                              essentially meaningless so this has not been implemented.
//
//
//
//          instruction handler functions
//
static void
adc()
{
	penaltyop = 1;

	if (state6502.status & FLAG_DECIMAL) {
		uint16_t tmp, tmp2;
		value = getvalue();
		tmp   = ((uint16_t)state6502.a & 0x0F) + (value & 0x0F) + (uint16_t)(state6502.status & FLAG_CARRY);
		tmp2  = ((uint16_t)state6502.a & 0xF0) + (value & 0xF0);
		if (tmp > 0x09) {
			tmp2 += 0x10;
			tmp += 0x06;
		}
		if (tmp2 > 0x90) {
			tmp2 += 0x60;
		}
		if (tmp2 & 0xFF00) {
			setcarry();
		} else {
			clearcarry();
		}
		result = (tmp & 0x0F) | (tmp2 & 0xF0);

		zerocalc(result); /* 65C02 change, Decimal Arithmetic sets NZV */
		signcalc(result);

		clockticks6502++;
	} else {

		value  = getvalue();
		result = (uint16_t)state6502.a + value + (uint16_t)(state6502.status & FLAG_CARRY);

		carrycalc(result);
		zerocalc(result);
		overflowcalc(result, state6502.a, value);
		signcalc(result);
	}

	saveaccum(result);
}

static void
and_op()
{
	penaltyop = 1;
	value     = getvalue();
	result    = (uint16_t)state6502.a & value;

	zerocalc(result);
	signcalc(result);

	saveaccum(result);
}

static void
asl()
{
	value  = getvalue();
	result = value << 1;

	carrycalc(result);
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

static void
bcc()
{
	if ((state6502.status & FLAG_CARRY) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
bcs()
{
	if ((state6502.status & FLAG_CARRY) == FLAG_CARRY) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
beq()
{
	if ((state6502.status & FLAG_ZERO) == FLAG_ZERO) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
bit()
{
	value  = getvalue();
	result = (uint16_t)state6502.a & value;

	zerocalc(result);
	state6502.status = (state6502.status & 0x3F) | (uint8_t)(value & 0xC0);
}

static void
bmi()
{
	if ((state6502.status & FLAG_SIGN) == FLAG_SIGN) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
bne()
{
	if ((state6502.status & FLAG_ZERO) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
bpl()
{
	if ((state6502.status & FLAG_SIGN) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
brk()
{
	state6502.pc++;

	push16(state6502.pc, _stack_op_type::push_op);                 // push next instruction address onto stack
	push8(state6502.status | FLAG_BREAK, _stack_op_type::push_op); // push CPU status to stack
	setinterrupt();                       // set interrupt flag
	cleardecimal();                       // clear decimal flag (65C02 change)
	vp6502();
	state6502.pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

static void
bvc()
{
	if ((state6502.status & FLAG_OVERFLOW) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
bvs()
{
	if ((state6502.status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00)) {
			clockticks6502 += 2; // check if jump crossed a page boundary
		} else {
			clockticks6502++;
		}
	}
}

static void
clc()
{
	clearcarry();
}

static void
cld()
{
	cleardecimal();
}

static void
cli()
{
	clearinterrupt();
}

static void
clv()
{
	clearoverflow();
}

static void
cmp()
{
	penaltyop = 1;
	value     = getvalue();
	result    = (uint16_t)state6502.a - value;

	if (state6502.a >= (uint8_t)(value & 0x00FF)) {
		setcarry();
	} else {
		clearcarry();
	}
	if (state6502.a == (uint8_t)(value & 0x00FF)) {
		setzero();
	} else {
		clearzero();
	}
	signcalc(result);
}

static void
cpx()
{
	value  = getvalue();
	result = (uint16_t)state6502.x - value;

	if (state6502.x >= (uint8_t)(value & 0x00FF)) {
		setcarry();
	} else {
		clearcarry();
	}
	if (state6502.x == (uint8_t)(value & 0x00FF)) {
		setzero();
	} else {
		clearzero();
	}
	signcalc(result);
}

static void
cpy()
{
	value  = getvalue();
	result = (uint16_t)state6502.y - value;

	if (state6502.y >= (uint8_t)(value & 0x00FF)) {
		setcarry();
	} else {
		clearcarry();
	}
	if (state6502.y == (uint8_t)(value & 0x00FF)) {
		setzero();
	} else {
		clearzero();
	}
	signcalc(result);
}

static void
dec()
{
	value  = getvalue();
	result = value - 1;

	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

static void
dex()
{
	state6502.x--;

	zerocalc(state6502.x);
	signcalc(state6502.x);
}

static void
dey()
{
	state6502.y--;

	zerocalc(state6502.y);
	signcalc(state6502.y);
}

static void
eor()
{
	penaltyop = 1;
	value     = getvalue();
	result    = (uint16_t)state6502.a ^ value;

	zerocalc(result);
	signcalc(result);

	saveaccum(result);
}

static void
inc()
{
	value  = getvalue();
	result = value + 1;

	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

static void
inx()
{
	state6502.x++;

	zerocalc(state6502.x);
	signcalc(state6502.x);
}

static void
iny()
{
	state6502.y++;

	zerocalc(state6502.y);
	signcalc(state6502.y);
}

static void
jmp()
{
	state6502.pc = ea;
}

static void
jsr()
{
	push16(state6502.pc - 1, _stack_op_type::push_jsr);
	state6502.pc = ea;

	smartstack_operations.add([]() {
		auto &ss = stack6502.allocate();
		ss.push.op_type = _stack_op_type::jsr;
		ss.push.state   = debug_state6502;
		ss.push.pc_bank   = bank6502(debug_state6502.pc);
		ss.push.jmp_data.dest_pc = ea;
		ss.push.jmp_data.dest_bank = bank6502(ea);
	});
}

static void
lda()
{
	penaltyop   = 1;
	value       = getvalue();
	state6502.a = (uint8_t)(value & 0x00FF);

	zerocalc(state6502.a);
	signcalc(state6502.a);
}

static void
ldx()
{
	penaltyop   = 1;
	value       = getvalue();
	state6502.x = (uint8_t)(value & 0x00FF);

	zerocalc(state6502.x);
	signcalc(state6502.x);
}

static void
ldy()
{
	penaltyop   = 1;
	value       = getvalue();
	state6502.y = (uint8_t)(value & 0x00FF);

	zerocalc(state6502.y);
	signcalc(state6502.y);
}

static void
lsr()
{
	value  = getvalue();
	result = value >> 1;

	if (value & 1)
		setcarry();
	else
		clearcarry();
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

static void
nop()
{
	switch (opcode) {
		case 0x1C:
		case 0x3C:
		case 0x5C:
		case 0x7C:
		case 0xDC:
		case 0xFC:
			penaltyop = 1;
			break;
	}
}

static void
ora()
{
	penaltyop = 1;
	value     = getvalue();
	result    = (uint16_t)state6502.a | value;

	zerocalc(result);
	signcalc(result);

	saveaccum(result);
}

static void
pha()
{
	push8(state6502.a, _stack_op_type::push_op);
}

static void
php()
{
	push8(state6502.status | FLAG_BREAK, _stack_op_type::push_op);

	if (stack6502.count() > 4) {
		if (auto op_type = stack6502[stack6502.count() - 4].push.op_type; op_type == _stack_op_type::nmi || op_type == _stack_op_type::irq) {
			smartstack_operations.add([]() {
				auto &ss = stack6502.allocate();
				ss.push.op_type = _stack_op_type::smart;
				ss.push.state   = debug_state6502;
				ss.push.pc_bank   = bank6502(debug_state6502.pc);
				ss.push.jmp_data.dest_pc   = state6502.pc;
				ss.push.jmp_data.dest_bank = bank6502(state6502.pc);
			});
		}
	}
}

static void
pla()
{
	state6502.a = pull8(_stack_op_type::pull_op);

	zerocalc(state6502.a);
	signcalc(state6502.a);
}

static void
plp()
{
	state6502.status = pull8(_stack_op_type::pull_op) | FLAG_CONSTANT;
}

static void
rol()
{
	value  = getvalue();
	result = (value << 1) | (state6502.status & FLAG_CARRY);

	carrycalc(result);
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

static void
ror()
{
	value  = getvalue();
	result = (value >> 1) | ((state6502.status & FLAG_CARRY) << 7);

	if (value & 1)
		setcarry();
	else
		clearcarry();
	zerocalc(result);
	signcalc(result);

	putvalue(result);
}

static void
rti()
{
	const uint16_t old_pc = state6502.pc;
	state6502.status      = pull8(_stack_op_type::rti);
	value                 = pull16(_stack_op_type::rti);
	state6502.pc          = value;
}

static void
rts()
{
	const uint16_t old_pc = state6502.pc;

	value        = pull16(_stack_op_type::rts);
	state6502.pc = value + 1;
}

static void
sbc()
{
	penaltyop = 1;

	if (state6502.status & FLAG_DECIMAL) {
		value  = getvalue();
		result = (uint16_t)state6502.a - (value & 0x0f) + (state6502.status & FLAG_CARRY) - 1;
		if ((result & 0x0f) > (state6502.a & 0x0f)) {
			result -= 6;
		}
		result -= (value & 0xf0);
		if ((result & 0xfff0) > ((uint16_t)state6502.a & 0xf0)) {
			result -= 0x60;
		}
		if (result <= (uint16_t)state6502.a) {
			setcarry();
		} else {
			clearcarry();
		}

		zerocalc(result); /* 65C02 change, Decimal Arithmetic sets NZV */
		signcalc(result);

		clockticks6502++;

	} else {
		value  = getvalue() ^ 0x00FF;
		result = (uint16_t)state6502.a + value + (uint16_t)(state6502.status & FLAG_CARRY);

		carrycalc(result);
		zerocalc(result);
		overflowcalc(result, state6502.a, value);
		signcalc(result);
	}

	saveaccum(result);
}

static void
sec()
{
	setcarry();
}

static void
sed()
{
	setdecimal();
}

static void
sei()
{
	setinterrupt();
}

static void
sta()
{
	putvalue(state6502.a);
}

static void
stx()
{
	putvalue(state6502.x);
}

static void
sty()
{
	putvalue(state6502.y);
}

static void
tax()
{
	state6502.x = state6502.a;

	zerocalc(state6502.x);
	signcalc(state6502.x);
}

static void
tay()
{
	state6502.y = state6502.a;

	zerocalc(state6502.y);
	signcalc(state6502.y);
}

static void
tsx()
{
	state6502.x = state6502.sp;

	zerocalc(state6502.x);
	signcalc(state6502.x);
}

static void
txa()
{
	state6502.a = state6502.x;

	zerocalc(state6502.a);
	signcalc(state6502.a);
}

uint8_t debug_read6502(uint16_t address);

static void
txs()
{
	state6502.sp = state6502.x;

	smartstack_operations.add([]() {
		const int sp_diff = static_cast<int>(state6502.sp) - static_cast<int>(debug_state6502.sp);
		if (sp_diff < 0) {
			// push onto stack
			for (int i = 0; i > sp_diff; --i) {
				auto &ss               = stack6502.allocate();
				ss.push.op_type        = _stack_op_type::push_op;
				ss.push.op_data.opcode = opcode;
				ss.push.state          = debug_state6502;
				ss.push.pc_bank        = bank6502(debug_state6502.pc);
				ss.push.op_data.value  = debug_read6502(static_cast<uint16_t>(BASE_STACK + static_cast<int>(debug_state6502.sp) + i));
			}
		} else if (sp_diff > 0) {
			// pop from stack
			for (int i = 0; i < sp_diff; ++i) {
				auto &ss = stack6502.pop_newest();
				ss.pop.op_type        = _stack_op_type::pull_op;
				ss.pop.op_data.opcode = opcode;
				ss.pop.state          = debug_state6502;
				ss.pop.pc_bank        = bank6502(debug_state6502.pc);
				ss.pop.op_data.value  = debug_read6502(static_cast<uint16_t>(BASE_STACK + static_cast<int>(debug_state6502.sp) + i + 1));

				if (ss.push.op_type < _stack_op_type::push_op) {
					auto &ss              = stack6502.pop_newest();
					ss.pop.op_type        = _stack_op_type::pull_op;
					ss.pop.op_data.opcode = opcode;
					ss.pop.state          = debug_state6502;
					ss.pop.pc_bank        = bank6502(debug_state6502.pc);
					ss.pop.op_data.value  = debug_read6502(static_cast<uint16_t>(BASE_STACK + static_cast<int>(debug_state6502.sp) + i + 1));
				}
			}					
		}
	});
}

static void
tya()
{
	state6502.a = state6502.y;

	zerocalc(state6502.a);
	signcalc(state6502.a);
}

#endif // defined(INSTRUCTIONS_6502_H)