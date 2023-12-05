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
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
bcs()
{
	if ((state6502.status & FLAG_CARRY) == FLAG_CARRY) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
beq()
{
	if ((state6502.status & FLAG_ZERO) == FLAG_ZERO) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
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
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
bne()
{
	if ((state6502.status & FLAG_ZERO) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
bpl()
{
	if ((state6502.status & FLAG_SIGN) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
brk()
{
	state6502.pc++;

	push16(state6502.pc);                 // push next instruction address onto stack
	push8(state6502.status | FLAG_BREAK); // push CPU status to stack
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
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
bvs()
{
	if ((state6502.status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; // check if jump crossed a page boundary
		else
			clockticks6502++;
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

	if (state6502.a >= (uint8_t)(value & 0x00FF))
		setcarry();
	else
		clearcarry();
	if (state6502.a == (uint8_t)(value & 0x00FF))
		setzero();
	else
		clearzero();
	signcalc(result);
}

static void
cpx()
{
	value  = getvalue();
	result = (uint16_t)state6502.x - value;

	if (state6502.x >= (uint8_t)(value & 0x00FF))
		setcarry();
	else
		clearcarry();
	if (state6502.x == (uint8_t)(value & 0x00FF))
		setzero();
	else
		clearzero();
	signcalc(result);
}

static void
cpy()
{
	value  = getvalue();
	result = (uint16_t)state6502.y - value;

	if (state6502.y >= (uint8_t)(value & 0x00FF))
		setcarry();
	else
		clearcarry();
	if (state6502.y == (uint8_t)(value & 0x00FF))
		setzero();
	else
		clearzero();
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
	auto &ss       = stack6502[state6502.sp_depth++];
	ss.source_pc   = state6502.pc;
	ss.source_bank = bank6502(state6502.pc);
	ss.push_depth  = 0;
	state6502.sp_unwind_depth = state6502.sp_depth;

	push16(state6502.pc - 1);
	state6502.pc = ea;

	ss.dest_pc   = state6502.pc;
	ss.dest_bank = bank6502(state6502.pc);
	ss.op_type   = _stack_op_type::jsr;
	ss.pop_type  = _stack_pop_type::unknown;
	ss.opcode    = opcode;
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
	push8(state6502.a);
	auto &ss = stack6502[(state6502.sp_depth + 255) & 0xff];
	auto &ssx = ss.pushed_bytes[ss.push_depth++];
	ssx.push_type = _push_op_type::a;
	ssx.pull_type = _push_op_type::unknown;
	ssx.value     = state6502.a;
}

static void
php()
{
	push8(state6502.status | FLAG_BREAK);
	auto &ss                      = stack6502[(state6502.sp_depth + 255) & 0xff];
	auto &ssx                     = ss.pushed_bytes[ss.push_depth++];
	ssx.push_type                 = _push_op_type::status;
	ssx.pull_type                 = _push_op_type::unknown;
	ssx.value                     = state6502.status | FLAG_BREAK;

	if (ss.op_type != _stack_op_type::jsr && ss.push_depth == 5) {
		ss.push_depth -= 3;

		auto &ss2 = stack6502[state6502.sp_depth++];
		ss2.source_pc              = (static_cast<uint16_t>(ss.pushed_bytes[ss.push_depth].value) << 8) | static_cast<uint16_t>(ss.pushed_bytes[ss.push_depth+1].value);
		ss2.source_bank            = bank6502(state6502.pc);
		ss2.push_depth             = 0;
		state6502.sp_unwind_depth = state6502.sp_depth;

		ss2.dest_pc   = state6502.pc;
		ss2.dest_bank = bank6502(state6502.pc);
		ss2.op_type   = _stack_op_type::smart;
		ss2.pop_type  = _stack_pop_type::unknown;
		ss2.opcode    = 0;
	}
}

static void
pla()
{
	state6502.a = pull8();

	zerocalc(state6502.a);
	signcalc(state6502.a);

	auto &ss = stack6502[(state6502.sp_depth + 255) & 0xff];
	ss.push_depth -= !!ss.push_depth;
	auto &ssx     = ss.pushed_bytes[ss.push_depth];
	ssx.pull_type = _push_op_type::a;
}

static void
plp()
{
	state6502.status = pull8() | FLAG_CONSTANT;

	auto &ss = stack6502[(state6502.sp_depth + 255) & 0xff];
	ss.push_depth -= !!ss.push_depth;
	auto &ssx     = ss.pushed_bytes[ss.push_depth];
	ssx.pull_type = _push_op_type::status;
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
	state6502.status = pull8();
	value            = pull16();
	state6502.pc     = value;
	stack6502_underflow |= !state6502.sp_depth;
	state6502.sp_depth -= !!state6502.sp_depth;

	auto &ss  = stack6502[state6502.sp_depth];	
	ss.pop_type = _stack_pop_type::rti;
	ss.pop_pc   = old_pc - 1;
	ss.pop_bank = bank6502(old_pc);
}

static void
rts()
{
	const uint16_t old_pc = state6502.pc;

	value                 = pull16();
	state6502.pc = value + 1;
	stack6502_underflow |= !state6502.sp_depth;
	state6502.sp_depth -= !!state6502.sp_depth;

	auto &ss    = stack6502[state6502.sp_depth];
	ss.pop_type = _stack_pop_type::rts;
	ss.pop_pc   = old_pc - 1;
	ss.pop_bank = bank6502(old_pc);
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

static void
txs()
{
	state6502.sp = state6502.x;
}

static void
tya()
{
	state6502.a = state6502.y;

	zerocalc(state6502.a);
	signcalc(state6502.a);
}

#endif // defined(INSTRUCTIONS_6502_H)