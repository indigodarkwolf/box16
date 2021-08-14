#include "debugger.h"
#include "cpu/fake6502.h"
#include "glue.h"
#include "memory.h"

static breakpoint_list Breakpoints;
static breakpoint_list Active_breakpoints;
static bool            Breakpoint_check[0x10000];

enum debugger_mode {
	DEBUG_RUN,
	DEBUG_PAUSE,
	DEBUG_STEP_INTO,
	DEBUG_STEP_OVER,
	DEBUG_STEP_OUT_RUN,
	DEBUG_STEP_OUT_OVER,
	DEBUG_STEP_OUT_RETURN
};

static debugger_mode   Debug_mode      = DEBUG_RUN;
static uint64_t        Step_clocks     = 0;
static uint8_t         Step_interrupt  = 0x04;
static uint8_t         Interrupt_check = 0x04;
static breakpoint_type Step_target     = { 0, 0 };

uint16_t debug_peek16(uint16_t addr)
{
	return (uint16_t)debug_read6502(addr) | ((uint16_t)debug_read6502(addr + 1) << 8);
}

static breakpoint_type get_current_pc()
{
	return breakpoint_type{ pc, memory_get_current_bank(pc) };
}

static breakpoint_type get_bp_from_addr(uint16_t addr)
{
	return breakpoint_type{ addr, memory_get_current_bank(addr) };
}

static constexpr const uint16_t &breakpoint_addr(const breakpoint_type bp)
{
	return std::get<0>(bp);
}

static bool execution_exited_interrupt()
{
	return (Step_interrupt != 0) && (Step_interrupt != (status & 0x04));
}

static bool breakpoint_hit(breakpoint_type current_pc)
{
	// If the debugger was set to run, make sure it allows at least one CPU cycle...
	if (debugger_step_clocks() == 0) {
		return false;
	}
	if (Breakpoint_check[breakpoint_addr(current_pc)]) {
		return Active_breakpoints.find(current_pc) != Active_breakpoints.end();
	}
	return false;
}

bool debugger_is_paused()
{
	auto current_pc = get_current_pc();

	switch (Debug_mode) {
		case DEBUG_RUN:
			if (breakpoint_hit(current_pc)) {
				debugger_pause_execution();
				return true;
			}
			break;
		case DEBUG_PAUSE:
			return true;
		case DEBUG_STEP_INTO:
			if (Step_clocks != clockticks6502) {
				debugger_pause_execution();
				return true;
			}
			break;
		case DEBUG_STEP_OVER:
			if (execution_exited_interrupt()) {
				debugger_pause_execution();
				return true;
			}
			if (breakpoint_hit(current_pc)) {
				debugger_pause_execution();
				return true;
			}
			if ((Step_interrupt == (status & 0x04)) && current_pc == Step_target) {
				debugger_pause_execution();
				return true;
			}
			break;
		case DEBUG_STEP_OUT_RUN:
			if (execution_exited_interrupt()) {
				debugger_pause_execution();
				return true;
			}
			if (breakpoint_hit(current_pc)) {
				debugger_pause_execution();
				return true;
			}
			if (Step_interrupt == (status & 0x04)) {
				switch (debug_read6502(pc)) {
					case 0x20: { // jsr
						Debug_mode              = DEBUG_STEP_OUT_OVER;
						const auto [addr, bank] = get_current_pc();
						Step_target             = { addr + 3, bank };
					} break;
					case 0x60: // rts
						Debug_mode  = DEBUG_STEP_OUT_RETURN;
						Step_target = get_bp_from_addr(debug_peek16(0x100 + sp + 1) + 1);
						break;
					case 0x40: // rti
						Debug_mode  = DEBUG_STEP_OUT_RETURN;
						Step_target = get_bp_from_addr(debug_peek16(0x100 + sp + 2));
						break;
				}
			}
			break;
		case DEBUG_STEP_OUT_OVER:
			if (execution_exited_interrupt()) {
				Debug_mode = DEBUG_PAUSE;
				return true;
			}
			if (breakpoint_hit(current_pc)) {
				Debug_mode = DEBUG_PAUSE;
				return true;
			}
			if ((Step_interrupt == (status & 0x04)) && current_pc == Step_target) {
				Debug_mode = DEBUG_STEP_OUT_RUN;
				return true;
			}
			break;
		case DEBUG_STEP_OUT_RETURN:
			if (execution_exited_interrupt()) {
				// Exited interrupt
				debugger_pause_execution();
				return true;
			}
			if (breakpoint_hit(current_pc)) {
				debugger_pause_execution();
				return true;
			}
			if ((Step_interrupt == (status & 0x04)) && (current_pc == Step_target)) {
				debugger_pause_execution();
				return true;
			}
			break;
	}

	return false;
}

void debugger_pause_execution()
{
	Debug_mode = DEBUG_PAUSE;
}

void debugger_continue_execution()
{
	Debug_mode      = DEBUG_RUN;
	Step_clocks     = clockticks6502;
	Step_interrupt  = 0x04;
	Interrupt_check = 0x04;
}

void debugger_step_execution()
{
	Debug_mode  = DEBUG_STEP_INTO;
	Step_clocks = clockticks6502;
	Step_interrupt  = status & 0x04;
	Interrupt_check = Step_interrupt;
}

void debugger_step_over_execution()
{
	if (debug_read6502(pc) == 0x20) {
		Debug_mode              = DEBUG_STEP_OVER;
		Step_clocks             = clockticks6502;
		Step_interrupt          = status & 0x04;
		Interrupt_check         = Step_interrupt;
		const auto [addr, bank] = get_current_pc();
		Step_target             = { addr + 3, bank };
	} else {
		debugger_step_execution();
	}
}

void debugger_step_out_execution()
{
	Step_clocks     = clockticks6502;
	Step_interrupt  = status & 0x04;
	Interrupt_check = Step_interrupt;

	// Stepping out turned out to be harder than expected, since I have neither symbols nor
	// a reliable stack: it can have arbitrary, undocumented data; there are no standard
	// "stack frames"; it's possible for non-interrupt code to be interrupted; and it's
	// possible for interrupt code to exit, well, anywhere. So my approach is to put the 
	// debugger in a mode where it essentially performs repeated "step over" operations until 
	// it discovers an RTS or RTI op, at which point we can interpret the stack and set the 
	// stop point correctly.

	switch (debug_read6502(pc)) {
		case 0x20: { // jsr
			Debug_mode              = DEBUG_STEP_OUT_OVER;
			const auto [addr, bank] = get_current_pc();
			Step_target             = { addr + 3, bank };
		} break;
		case 0x60: // rts
			Debug_mode  = DEBUG_STEP_OUT_RETURN;
			Step_target = get_bp_from_addr(debug_peek16(0x100 + sp + 1));
			break;
		case 0x40: // rti
			Debug_mode  = DEBUG_STEP_OUT_RETURN;
			Step_target = get_bp_from_addr(debug_peek16(0x100 + sp + 2));
			break;
		default:
			Debug_mode = DEBUG_STEP_OUT_RUN;
			break;
	}
}

uint64_t debugger_step_clocks()
{
	return clockticks6502 - Step_clocks;
}

void debugger_interrupt()
{
	Interrupt_check |= status & 0x04;
}

bool debugger_step_interrupted()
{
	return Interrupt_check != Step_interrupt;
}

void debugger_add_breakpoint(uint16_t address, uint8_t bank /* = 0 */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	breakpoint_type new_bp{ address, bank };
	if (Breakpoints.find(new_bp) == Breakpoints.end()) {
		Breakpoints.insert(new_bp);
		Active_breakpoints.insert(new_bp);
	}
}

void debugger_remove_breakpoint(uint16_t address, uint8_t bank /* = 0 */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	breakpoint_type old_bp{ address, bank };
	Breakpoints.erase(old_bp);
	Active_breakpoints.erase(old_bp);
}

void debugger_activate_breakpoint(uint16_t address, uint8_t bank /* = 0 */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	breakpoint_type new_bp{ address, bank };
	if (Breakpoints.find(new_bp) == Breakpoints.end()) {
		return;
	}
	if (Active_breakpoints.find(new_bp) == Active_breakpoints.end()) {
		Active_breakpoints.insert(new_bp);
	}
}

void debugger_deactivate_breakpoint(uint16_t address, uint8_t bank /* = 0 */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	breakpoint_type old_bp{ address, bank };
	Active_breakpoints.erase(old_bp);
}

bool debugger_breakpoint_is_active(uint16_t address, uint8_t bank /* = 0 */)
{
	breakpoint_type bp{ address, bank };
	return (Active_breakpoints.find(bp) != Active_breakpoints.end());
}

const breakpoint_list &debugger_get_breakpoints()
{
	return Breakpoints;
}
