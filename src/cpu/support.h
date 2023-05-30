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
#	define zerocalc(n)      \
		{                    \
			if ((n)&0x00FF)  \
				clearzero(); \
			else             \
				setzero();   \
		}

#	define signcalc(n)      \
		{                    \
			if ((n)&0x0080)  \
				setsign();   \
			else             \
				clearsign(); \
		}

#	define carrycalc(n)      \
		{                     \
			if ((n)&0xFF00)   \
				setcarry();   \
			else              \
				clearcarry(); \
		}

#	define overflowcalc(n, m, o)                             \
		{ /* n = result, m = accumulator, o = memory */       \
			if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) \
				setoverflow();                                \
			else                                              \
				clearoverflow();                              \
		}

// a few general functions used by various other functions
void push16(uint16_t pushval)
{
	write6502(BASE_STACK + state6502.sp, (pushval >> 8) & 0xFF);
	write6502(BASE_STACK + ((state6502.sp - 1) & 0xFF), pushval & 0xFF);
	state6502.sp -= 2;
}

void push8(uint8_t pushval)
{
	write6502(BASE_STACK + state6502.sp--, pushval);
}

uint16_t pull16()
{
	uint16_t temp16;
	temp16 = read6502(BASE_STACK + ((state6502.sp + 1) & 0xFF)) | ((uint16_t)read6502(BASE_STACK + ((state6502.sp + 2) & 0xFF)) << 8);
	state6502.sp += 2;
	return (temp16);
}

uint8_t pull8()
{
	return (read6502(BASE_STACK + ++state6502.sp));
}

void reset6502()
{
	vpb6502();
	state6502.pc       = (uint16_t)read6502(0xFFFC) | ((uint16_t)read6502(0xFFFD) << 8);
	state6502.sp_depth = 0;
	state6502.a        = 0;
	state6502.x        = 0;
	state6502.y        = 0;
	state6502.sp       = 0xFD;
	state6502.status |= FLAG_CONSTANT | FLAG_BREAK;
	setinterrupt();
	cleardecimal();
	waiting = 0;
}

#endif // !defined(SUPPORT_6502_H)