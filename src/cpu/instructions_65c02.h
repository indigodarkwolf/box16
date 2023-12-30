#include "support.h"

// *******************************************************************************************
// *******************************************************************************************
//
//		File:		instructions_65c02.h
//		Date:		3rd September 2019
//		Purpose:	Additional functions for new 65C02 Opcodes.
//		Author:		Paul Robson (paul@robson.org.uk)
//
// *******************************************************************************************
// *******************************************************************************************

// *******************************************************************************************
//
//					Indirect without indexation.  (copied from indy)
//
// *******************************************************************************************

static void
ind0()
{
	uint16_t eahelp, eahelp2;
	eahelp  = (uint16_t)read6502(state6502.pc++);
	eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //zero-page wraparound
	ea      = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
}

// *******************************************************************************************
//
//						(Absolute,Indexed) address mode for JMP
//
// *******************************************************************************************

static void
ainx()
{ // absolute indexed branch
	uint16_t eahelp, eahelp2;
	eahelp = (uint16_t)read6502(state6502.pc) | (uint16_t)((uint16_t)read6502(state6502.pc + 1) << 8);
	eahelp = (eahelp + (uint16_t)state6502.x) & 0xFFFF;
#if 0
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //replicate 6502 page-boundary wraparound bug
#else
	eahelp2 = eahelp + 1; // the 65c02 doesn't have the bug
#endif
	ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
	state6502.pc += 2;
}

// *******************************************************************************************
//
//								Store zero to memory.
//
// *******************************************************************************************

static void
stz()
{
	putvalue(0);
}

// *******************************************************************************************
//
//								Unconditional Branch
//
// *******************************************************************************************

static void
bra()
{
	oldpc = state6502.pc;
	state6502.pc += reladdr;
	if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
		clockticks6502++; //check if jump crossed a page boundary
}

// *******************************************************************************************
//
//									Push/Pull X and Y
//
// *******************************************************************************************

static void
phx()
{
	push8(state6502.x);
	auto &ss                      = stack6502[(state6502.sp_depth + 255) & 0xff];
	auto &ssx                     = ss.pushed_bytes[ss.push_depth++];
	ssx.push_type                 = _push_op_type::x;
	ssx.pull_type                 = _push_op_type::unknown;
	ssx.value                     = state6502.x;
	ssx.pc                        = state6502.pc - 1;
	ssx.bank                      = bank6502(state6502.pc - 1);
	ss.push_unwind_depth          = ss.push_depth;
}

static void
plx()
{
	state6502.x = pull8();

	zerocalc(state6502.x);
	signcalc(state6502.x);

	auto &ss = stack6502[(state6502.sp_depth + 255) & 0xff];
	ss.push_depth -= !!ss.push_depth;
	auto &ssx     = ss.pushed_bytes[ss.push_depth];
	ssx.pull_type = _push_op_type::x;
}

static void
phy()
{
	push8(state6502.y);
	auto &ss                      = stack6502[(state6502.sp_depth + 255) & 0xff];
	auto &ssx                     = ss.pushed_bytes[ss.push_depth++];
	ssx.push_type                 = _push_op_type::y;
	ssx.pull_type                 = _push_op_type::unknown;
	ssx.value                     = state6502.y;
	ssx.pc                        = state6502.pc - 1;
	ssx.bank                      = bank6502(state6502.pc - 1);
	ss.push_unwind_depth          = ss.push_depth;
}

static void
ply()
{
	state6502.y = pull8();

	zerocalc(state6502.y);
	signcalc(state6502.y);

	auto &ss  = stack6502[(state6502.sp_depth + 255) & 0xff];
	ss.push_depth -= !!ss.push_depth;
	auto &ssx = ss.pushed_bytes[ss.push_depth];
	ssx.pull_type = _push_op_type::y;
}

// *******************************************************************************************
//
//								TRB & TSB - Test and Change bits
//
// *******************************************************************************************

static void
tsb()
{
	value  = getvalue();          // Read memory
	result = (uint16_t)state6502.a & value; // calculate A & memory
	zerocalc(result);             // Set Z flag from this.
	result = value | state6502.a;           // Write back value read, A bits are set.
	putvalue(result);
}

static void
trb()
{
	value  = getvalue();          // Read memory
	result = (uint16_t)state6502.a & value; // calculate A & memory
	zerocalc(result);             // Set Z flag from this.
	result = value & (state6502.a ^ 0xFF);  // Write back value read, A bits are clear.
	putvalue(result);
}

// *******************************************************************************************
//
//                                     Invoke Debugger
//
// *******************************************************************************************

static void
dbg()
{
	debugger_pause_execution(); // Invoke debugger.
}

// *******************************************************************************************
//
//                                     Wait for interrupt
//
// *******************************************************************************************

static void
wai()
{
	waiting = 1;
}

// *******************************************************************************************
//
//                                     BBR and BBS
//
// *******************************************************************************************
static void
bbr(uint16_t bitmask)
{
	if ((getvalue() & bitmask) == 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; //check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
bbr0()
{
	bbr(0x01);
}
static void
bbr1()
{
	bbr(0x02);
}
static void
bbr2()
{
	bbr(0x04);
}
static void
bbr3()
{
	bbr(0x08);
}
static void
bbr4()
{
	bbr(0x10);
}
static void
bbr5()
{
	bbr(0x20);
}
static void
bbr6()
{
	bbr(0x40);
}
static void
bbr7()
{
	bbr(0x80);
}

static void
bbs(uint16_t bitmask)
{
	if ((getvalue() & bitmask) != 0) {
		oldpc = state6502.pc;
		state6502.pc += reladdr;
		if ((oldpc & 0xFF00) != (state6502.pc & 0xFF00))
			clockticks6502 += 2; //check if jump crossed a page boundary
		else
			clockticks6502++;
	}
}

static void
bbs0()
{
	bbs(0x01);
}
static void
bbs1()
{
	bbs(0x02);
}
static void
bbs2()
{
	bbs(0x04);
}
static void
bbs3()
{
	bbs(0x08);
}
static void
bbs4()
{
	bbs(0x10);
}
static void
bbs5()
{
	bbs(0x20);
}
static void
bbs6()
{
	bbs(0x40);
}
static void
bbs7()
{
	bbs(0x80);
}

// *******************************************************************************************
//
//                                     SMB and RMB
//
// *******************************************************************************************

static void
smb0()
{
	putvalue(getvalue() | 0x01);
}
static void
smb1()
{
	putvalue(getvalue() | 0x02);
}
static void
smb2()
{
	putvalue(getvalue() | 0x04);
}
static void
smb3()
{
	putvalue(getvalue() | 0x08);
}
static void
smb4()
{
	putvalue(getvalue() | 0x10);
}
static void
smb5()
{
	putvalue(getvalue() | 0x20);
}
static void
smb6()
{
	putvalue(getvalue() | 0x40);
}
static void
smb7()
{
	putvalue(getvalue() | 0x80);
}

static void
rmb0()
{
	putvalue(getvalue() & ~0x01);
}
static void
rmb1()
{
	putvalue(getvalue() & ~0x02);
}
static void
rmb2()
{
	putvalue(getvalue() & ~0x04);
}
static void
rmb3()
{
	putvalue(getvalue() & ~0x08);
}
static void
rmb4()
{
	putvalue(getvalue() & ~0x10);
}
static void
rmb5()
{
	putvalue(getvalue() & ~0x20);
}
static void
rmb6()
{
	putvalue(getvalue() & ~0x40);
}
static void
rmb7()
{
	putvalue(getvalue() & ~0x80);
}
