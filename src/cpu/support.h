#if !defined(SUPPORT_6502_H)
#	define SUPPORT_6502_H

/*

                        Extracted from original single fake6502.c file

*/

#	define saveaccum(n) state6502.a = (uint8_t)((n)&0x00FF)

// flag modifier macros
#	define setcarry() state6502.status |= FLAG_CARRY
#	define clearcarry() state6502.status &= (~FLAG_CARRY)
#	define setzero() state6502.status |= FLAG_ZERO
#	define clearzero() state6502.status &= (~FLAG_ZERO)
#	define setinterrupt() state6502.status |= FLAG_INTERRUPT
#	define clearinterrupt() state6502.status &= (~FLAG_INTERRUPT)
#	define setdecimal() state6502.status |= FLAG_DECIMAL
#	define cleardecimal() state6502.status &= (~FLAG_DECIMAL)
#	define setoverflow() state6502.status |= FLAG_OVERFLOW
#	define clearoverflow() state6502.status &= (~FLAG_OVERFLOW)
#	define setsign() state6502.status |= FLAG_SIGN
#	define clearsign() state6502.status &= (~FLAG_SIGN)

// flag calculation macros
#	define zerocalc(n)        \
		{                     \
			if ((n)&0x00FF) { \
				clearzero();  \
			} else {          \
				setzero();    \
			}                 \
		}

#	define signcalc(n)        \
		{                     \
			if ((n)&0x0080) { \
				setsign();    \
			} else {          \
				clearsign();  \
			}                 \
		}

#	define carrycalc(n)       \
		{                     \
			if ((n)&0xFF00) { \
				setcarry();   \
			} else {          \
				clearcarry(); \
			}                 \
		}

#	define overflowcalc(n, m, o)                                \
		{ /* n = result, m = accumulator, o = memory */         \
			if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) { \
				setoverflow();                                  \
			} else {                                            \
				clearoverflow();                                \
			}                                                   \
		}

// a few general functions used by various other functions
void push16(uint16_t pushval, _stack_op_type op_type)
{
	smartstack_operations.add([pushval, op_type]() {
		{
			auto &ss        = stack6502.allocate();
			ss.push.op_type = op_type;
			ss.push.op_data.opcode  = opcode;
			ss.push.state   = debug_state6502;
			ss.push.pc_bank = bank6502(debug_state6502.pc);
			ss.push.op_data.value   = (pushval >> 8) & 0xFF;
		}

		{
			auto &ss        = stack6502.allocate();
			ss.push.op_type = op_type;
			ss.push.op_data.opcode  = opcode;
			ss.push.state   = debug_state6502;
			ss.push.pc_bank = bank6502(debug_state6502.pc);
			ss.push.op_data.value   = pushval & 0xFF;
		}
	});
	write6502(BASE_STACK + state6502.sp, (pushval >> 8) & 0xFF);
	write6502(BASE_STACK + ((state6502.sp - 1) & 0xFF), pushval & 0xFF);
	state6502.sp -= 2;
}

void push8(uint8_t pushval, _stack_op_type op_type)
{
	smartstack_operations.add([pushval, op_type]() {
		auto &ss        = stack6502.allocate();
		ss.push.op_type = op_type;
		ss.push.op_data.opcode  = opcode;
		ss.push.state   = debug_state6502;
		ss.push.pc_bank = bank6502(debug_state6502.pc);
		ss.push.op_data.value   = pushval;
	});
	write6502(BASE_STACK + state6502.sp--, pushval);
}

uint16_t pull16(_stack_op_type op_type)
{
	const uint16_t temp16 = read6502(BASE_STACK + ((state6502.sp + 1) & 0xFF)) | ((uint16_t)read6502(BASE_STACK + ((state6502.sp + 2) & 0xFF)) << 8);
	state6502.sp += 2;

	smartstack_operations.add([op_type, temp16]() {
		{
			auto &ss       = stack6502.pop_newest();
			ss.pop.op_type = op_type;
			ss.pop.op_data.opcode  = opcode;
			ss.pop.state   = debug_state6502;
			ss.pop.pc_bank = bank6502(debug_state6502.pc);
			ss.pop.op_data.value   = temp16 & 0xFF;

			if (ss.push.op_type < _stack_op_type::push_op) {
				auto &ss       = stack6502.pop_newest();
				ss.pop.op_type = op_type;
				ss.pop.op_data.opcode  = opcode;
				ss.pop.state   = debug_state6502;
				ss.pop.pc_bank = bank6502(debug_state6502.pc);
				ss.pop.op_data.value   = temp16 & 0xFF;
			}
		}

		{
			auto &ss       = stack6502.pop_newest();
			ss.pop.op_type = op_type;
			ss.pop.op_data.opcode  = opcode;
			ss.pop.state   = debug_state6502;
			ss.pop.pc_bank = bank6502(debug_state6502.pc);
			ss.pop.op_data.value   = (temp16 >> 8) & 0xFF;

			if (ss.push.op_type < _stack_op_type::push_op) {
				auto &ss       = stack6502.pop_newest();
				ss.pop.op_type = op_type;
				ss.pop.op_data.opcode  = opcode;
				ss.pop.state   = debug_state6502;
				ss.pop.pc_bank = bank6502(debug_state6502.pc);
				ss.pop.op_data.value   = (temp16 >> 8) & 0xFF;
			}
		}
	});

	return (temp16);
}

uint8_t pull8(_stack_op_type op_type)
{
	const uint8_t temp8 = read6502(BASE_STACK + ++state6502.sp);
	smartstack_operations.add([op_type, temp8]() {
		auto &ss       = stack6502.pop_newest();
		ss.pop.op_type = op_type;
		ss.pop.op_data.opcode  = opcode;
		ss.pop.state   = debug_state6502;
		ss.pop.pc_bank = bank6502(debug_state6502.pc);
		ss.pop.op_data.value   = temp8;

		if (ss.push.op_type < _stack_op_type::push_op) {
			auto &ss       = stack6502.pop_newest();
			ss.pop.op_type = op_type;
			ss.pop.op_data.opcode  = opcode;
			ss.pop.state   = debug_state6502;
			ss.pop.pc_bank = bank6502(debug_state6502.pc);
			ss.pop.op_data.value   = temp8;
		}
	});

	return (temp8);
}

void reset6502()
{
	vp6502();
	state6502.pc = (uint16_t)read6502(0xFFFC) | ((uint16_t)read6502(0xFFFD) << 8);
	state6502.a  = 0;
	state6502.x  = 0;
	state6502.y  = 0;
	state6502.sp = 0xFD;
	state6502.status = FLAG_CONSTANT | FLAG_BREAK;
	setinterrupt();
	cleardecimal();
	waiting = 0;
	stack6502.clear();
	history6502.clear();
	smartstack_operations.clear();
}

#endif // !defined(SUPPORT_6502_H)