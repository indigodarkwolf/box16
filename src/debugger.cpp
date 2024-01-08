#include "debugger.h"
#include "boxmon/parser.h"
#include "cpu/fake6502.h"
#include "cpu/mnemonics.h"
#include "glue.h"
#include "memory.h"

#include <map>

//
// Breakpoints
//

static breakpoint_list                                Breakpoints;
static breakpoint_list                                Active_breakpoints;
static uint8_t                                       *Breakpoint_flags = nullptr;
static std::map<uint32_t, std::string>                Breakpoint_conditions;
static std::map<uint32_t, const boxmon::expression *> Breakpoint_expressions;

static boxmon::parser Condition_parser;
static const std::string Empty_string("");

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
static uint32_t        Step_instructions = 0;
static uint8_t         Step_interrupt  = 0x04;
static uint8_t         Interrupt_check = 0x04;
static breakpoint_type Step_target     = { 0, 0 };
static uint32_t        Step_instruction_count = 0;

uint16_t debug_peek16(uint16_t addr)
{
	return (uint16_t)debug_read6502(addr) | ((uint16_t)debug_read6502(addr + 1) << 8);
}

static breakpoint_type get_bp_from_addr(uint16_t addr)
{
	return breakpoint_type{ addr, memory_get_current_bank(addr) };
}

static breakpoint_type get_current_pc()
{
	return get_bp_from_addr(state6502.pc - waiting);
}

static constexpr const uint16_t breakpoint_addr(const breakpoint_type bp)
{
	return std::get<0>(bp);
}

static constexpr const uint8_t breakpoint_bank(const breakpoint_type bp)
{
	return std::get<1>(bp);
}

static uint32_t get_offset(const uint16_t addr, const uint8_t bank)
{
	if (addr >= 0xa000) {
		return addr + (bank << 13) + (bank << 14);
	} else {
		return addr;
	}
}

static uint8_t &get_flags(const uint16_t addr, const uint8_t bank)
{
	return Breakpoint_flags[get_offset(addr, bank)];
}

static void set_flags(const uint16_t addr, const uint8_t bank, uint8_t flags)
{
	Breakpoint_flags[get_offset(addr, bank)] = flags;
}

static bool execution_exited_interrupt()
{
	return (Step_interrupt != 0) && (Step_interrupt != (state6502.status & 0x04));
}

// static bool is_breakpoint_hit(breakpoint_type bp)
//{
//	// If the debugger was set to run, make sure it allows at least one CPU cycle...
//	if (debugger_step_clocks() == 0) {
//		return false;
//	}
//
//	if (get_flags(std::get<0>(bp), std::get<1>(bp)) & DEBUG6502_EXEC) {
//		return true;
//	}
//
//	return false;
// }

void debugger_init(int max_ram_banks)
{
	constexpr const int breakpoint_flags_size = 0xa000 + 0x6000 * NUM_MAX_RAM_BANKS;

	Breakpoint_flags = new uint8_t[breakpoint_flags_size];
	memset(Breakpoint_flags, 0, breakpoint_flags_size);

	Breakpoint_conditions.clear();
	Breakpoint_expressions.clear();

	options_apply_debugger_opts();
}

void debugger_shutdown()
{
	delete[] Breakpoint_flags;

	for (auto [key, value] : Breakpoint_expressions) {
		delete value;
	}
	Breakpoint_expressions.clear();
	Breakpoint_conditions.clear();
}

bool debugger_is_paused()
{
	auto current_pc = get_current_pc();

	switch (Debug_mode) {
		case DEBUG_RUN:
			// if (is_breakpoint_hit(current_pc)) {
			//	debugger_pause_execution();
			//	return true;
			// }
			break;
		case DEBUG_PAUSE:
			return true;
		case DEBUG_STEP_INTO:
			if (Step_instruction_count != 0) {
				if (debugger_step_instructions() >= Step_instruction_count) {
					Step_instruction_count = 0;
					debugger_pause_execution();
					return true;
				}
			} else {
				if (!waiting && Step_clocks != clockticks6502) {
					debugger_pause_execution();
					return true;
				}
			}
			break;
		case DEBUG_STEP_OVER:
			if (execution_exited_interrupt()) {
				debugger_pause_execution();
				return true;
			}
			// if (is_breakpoint_hit(current_pc)) {
			//	debugger_pause_execution();
			//	return true;
			// }
			if (!waiting && (Step_interrupt == (state6502.status & 0x04)) && current_pc == Step_target) {
				debugger_pause_execution();
				return true;
			}
			break;
		case DEBUG_STEP_OUT_RUN:
			if (execution_exited_interrupt()) {
				debugger_pause_execution();
				return true;
			}
			// if (is_breakpoint_hit(current_pc)) {
			//	debugger_pause_execution();
			//	return true;
			// }
			if (Step_interrupt == (state6502.status & 0x04)) {
				switch (debug_read6502(state6502.pc)) {
					case 0x20: { // jsr
						Debug_mode              = DEBUG_STEP_OUT_OVER;
						const auto [addr, bank] = get_current_pc();
						Step_target             = { addr + 3, bank };
					} break;
					case 0x60: // rts
						Debug_mode  = DEBUG_STEP_OUT_RETURN;
						Step_target = get_bp_from_addr(debug_peek16(0x100 + state6502.sp + 1) + 1);
						break;
					case 0x40: // rti
						Debug_mode  = DEBUG_STEP_OUT_RETURN;
						Step_target = get_bp_from_addr(debug_peek16(0x100 + state6502.sp + 2));
						break;
				}
			}
			break;
		case DEBUG_STEP_OUT_OVER:
			if (execution_exited_interrupt()) {
				Debug_mode = DEBUG_PAUSE;
				return true;
			}
			// if (is_breakpoint_hit(current_pc)) {
			//	Debug_mode = DEBUG_PAUSE;
			//	return true;
			// }
			if ((Step_interrupt == (state6502.status & 0x04)) && current_pc == Step_target) {
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
			// if (is_breakpoint_hit(current_pc)) {
			//	debugger_pause_execution();
			//	return true;
			// }
			if ((Step_interrupt == (state6502.status & 0x04)) && (current_pc == Step_target)) {
				debugger_pause_execution();
				return true;
			}
			break;
	}

	return false;
}

void debugger_process_cpu()
{
	if (debugger_step_clocks() == 0) {
		return;
	}

	if (Step_instruction_count != 0 && debugger_step_instructions() == Step_instruction_count) {
		Step_instruction_count = 0;
		debugger_pause_execution();
		return;
	}

	const auto [addr, bank] = get_current_pc();
	const auto flags        = get_flags(addr, bank);
	if (flags & DEBUG6502_CONDITION) {
		if (flags & DEBUG6502_EXPRESSION) {
			if (!debugger_evaluate_condition(addr, bank)) {
				return;
			}
		} else {
			return;
		}
	}

	debugger_pause_execution();
}

void debugger_pause_execution()
{
	Debug_mode = DEBUG_PAUSE;
}

void debugger_continue_execution()
{
	Debug_mode      = DEBUG_RUN;
	Step_clocks     = clockticks6502;
	Step_instructions = instructions;
	Step_interrupt  = 0x04;
	Interrupt_check = 0x04;
}

void debugger_step_execution(uint32_t instruction_count)
{
	Debug_mode      = DEBUG_STEP_INTO;
	Step_clocks     = clockticks6502;
	Step_instructions = instructions;
	Step_interrupt    = state6502.status & 0x04;
	Interrupt_check = Step_interrupt;
	Step_instruction_count = instruction_count;
}

void debugger_step_over_execution()
{
	const uint8_t op = debug_read6502(state6502.pc - waiting);
	if (op == 0x20) {
		Debug_mode              = DEBUG_STEP_OVER;
		Step_clocks             = clockticks6502;
		Step_instructions       = instructions;
		Step_interrupt          = state6502.status & 0x04;
		Interrupt_check         = Step_interrupt;
		const auto [addr, bank] = get_current_pc();
		Step_target             = { addr + 3, bank };
	} else if (op == 0xcb) {
		Debug_mode              = DEBUG_STEP_OVER;
		Step_clocks             = clockticks6502;
		Step_instructions       = instructions;
		Step_interrupt          = state6502.status & 0x04;
		Interrupt_check         = Step_interrupt;
		const auto [addr, bank] = get_current_pc();
		Step_target             = { addr + 1, bank };
	} else {
		debugger_step_execution();
	}
}

void debugger_step_out_execution()
{
	Step_clocks     = clockticks6502;
	Step_interrupt  = state6502.status & 0x04;
	Interrupt_check = Step_interrupt;

	// Stepping out turned out to be harder than expected, since I have neither symbols nor
	// a reliable stack: it can have arbitrary, undocumented data; there are no standard
	// "stack frames"; it's possible for non-interrupt code to be interrupted; and it's
	// possible for interrupt code to exit, well, anywhere. So my approach is to put the
	// debugger in a mode where it essentially performs repeated "step over" operations until
	// it discovers an RTS or RTI op, at which point we can interpret the stack and set the
	// stop point correctly.

	switch (debug_read6502(state6502.pc)) {
		case 0x20: { // jsr
			Debug_mode              = DEBUG_STEP_OUT_OVER;
			const auto [addr, bank] = get_current_pc();
			Step_target             = { addr + 3, bank };
		} break;
		case 0x60: // rts
			Debug_mode  = DEBUG_STEP_OUT_RETURN;
			Step_target = get_bp_from_addr(debug_peek16(0x100 + state6502.sp + 1));
			break;
		case 0x40: // rti
			Debug_mode  = DEBUG_STEP_OUT_RETURN;
			Step_target = get_bp_from_addr(debug_peek16(0x100 + state6502.sp + 2));
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

uint32_t debugger_step_instructions()
{
	return instructions - Step_instructions;
}

void debugger_interrupt()
{
	Interrupt_check |= state6502.status & 0x04;
}

bool debugger_step_interrupted()
{
	return Interrupt_check != Step_interrupt;
}

uint8_t debugger_get_flags(uint16_t address, uint8_t bank)
{
	if (address < 0xa000) {
		bank = 0;
	}
	const uint32_t offset = get_offset(address, bank);
	const uint8_t  flags = Breakpoint_flags[offset];
	return flags & 0xf;
}

std::string debugger_get_condition(uint16_t address, uint8_t bank)
{
	if (auto condition = Breakpoint_conditions.find(get_offset(address, bank)); condition != Breakpoint_conditions.end()) {
		return condition->second;
	}
	return "";
}

void debugger_set_condition(uint16_t address, uint8_t bank, const std::string &condition)
{
	const uint32_t offset = get_offset(address, bank);

	if (condition.empty()) {
		Breakpoint_flags[offset] &= ~DEBUG6502_EXPRESSION;

		if (auto citer = Breakpoint_conditions.find(offset); citer != Breakpoint_conditions.end()) {
			Breakpoint_conditions.erase(citer);
		}
		if (auto eiter = Breakpoint_expressions.find(offset); eiter != Breakpoint_expressions.end()) {
			Breakpoint_expressions.erase(eiter);
		}
	} else {
		Breakpoint_conditions[offset] = condition;

		const boxmon::expression *expression = nullptr;
		const char               *condition_cstr = condition.c_str();
		if (Condition_parser.parse_expression(expression, condition_cstr, boxmon::expression_parse_flags_must_consume_all | boxmon::expression_parse_flags_suppress_errors)) {
			Breakpoint_expressions[offset] = expression;
			Breakpoint_flags[offset] |= DEBUG6502_EXPRESSION;
		} else {
			if (auto eiter = Breakpoint_expressions.find(offset); eiter != Breakpoint_expressions.end()) {
				Breakpoint_expressions.erase(eiter);
			}
			Breakpoint_flags[offset] &= ~DEBUG6502_EXPRESSION;
		}
	}
}

bool debugger_evaluate_condition(uint16_t address, uint8_t bank)
{
	const uint32_t offset = get_offset(address, bank);
	if (auto eiter = Breakpoint_expressions.find(offset); eiter != Breakpoint_expressions.end()) {
		return (eiter->second->evaluate() != 0);
	}
	return false;
}

bool debugger_has_valid_expression(uint16_t address, uint8_t bank)
{
	const uint32_t offset = get_offset(address, bank);
	return (Breakpoint_flags[offset] & DEBUG6502_EXPRESSION);
}

void debugger_add_breakpoint(uint16_t address, uint8_t bank /* = 0 */, uint8_t flags /* = DEBUG6502_EXEC */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	flags &= 0x0f;

	flags |= (flags << 4);
	flags |= get_flags(address, bank);
	set_flags(address, bank, flags);

	breakpoint_type new_bp{ address, bank };
	if (Breakpoints.find(new_bp) == Breakpoints.end()) {
		Breakpoints.insert(new_bp);
		Active_breakpoints.insert(new_bp);
	}
}

void debugger_remove_breakpoint(uint16_t address, uint8_t bank /* = 0 */, uint8_t flags /* = DEBUG6502_EXEC */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	flags &= 0x0f;

	flags |= (flags << 4);
	flags = get_flags(address, bank) & ~flags;
	set_flags(address, bank, flags);

	if (flags == 0) {
		breakpoint_type old_bp{ address, bank };
		Breakpoints.erase(old_bp);
		Active_breakpoints.erase(old_bp);

		const uint32_t offset = get_offset(address, bank);
		if (auto citer = Breakpoint_conditions.find(offset); citer != Breakpoint_conditions.end()) {
			Breakpoint_conditions.erase(citer);
		}
		if (auto eiter = Breakpoint_expressions.find(offset); eiter != Breakpoint_expressions.end()) {
			Breakpoint_expressions.erase(eiter);
		}
	}
}

void debugger_activate_breakpoint(uint16_t address, uint8_t bank /* = 0 */, uint8_t flags /* = DEBUG6502_EXEC */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	flags &= 0x0f;

	flags = get_flags(address, bank) | flags;
	set_flags(address, bank, flags);

	breakpoint_type new_bp{ address, bank };
	if (Active_breakpoints.find(new_bp) == Active_breakpoints.end()) {
		Active_breakpoints.insert(new_bp);
	}
}

void debugger_deactivate_breakpoint(uint16_t address, uint8_t bank /* = 0 */, uint8_t flags /* = DEBUG6502_EXEC */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	flags &= 0x0f;
	flags = get_flags(address, bank) & ~flags;
	set_flags(address, bank, flags);

	if ((flags & 0x0f) == 0) {
		breakpoint_type old_bp{ address, bank };
		Active_breakpoints.erase(old_bp);
	}
}

bool debugger_has_breakpoint(uint16_t address, uint8_t bank /* = 0 */, uint8_t flags /* = DEBUG6502_EXEC */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	flags &= 0x0f;
	flags |= flags << 4;

	return get_flags(address, bank) & flags;
}

bool debugger_breakpoint_is_active(uint16_t address, uint8_t bank /* = 0 */, uint8_t flags /* = DEBUG6502_EXEC */)
{
	if (address < 0xa000) {
		bank = 0;
	}

	flags &= 0x0f;

	return get_flags(address, bank) & flags;
}

const breakpoint_list &debugger_get_breakpoints()
{
	return Breakpoints;
}

//
// Memory watch
//

char const *Debugger_size_types[Num_debugger_size_types] = {
	"U8",
	"U16",
	"U24",
	"U32",
	"S8",
	"S16",
	"S24",
	"S32"
};

static watch_address_list Watchlist;

void debugger_add_watch(uint16_t address, uint8_t bank, uint8_t size_type)
{
	if (address < 0xa000) {
		bank = 0;
	}

	watch_address_type new_watch{ address, bank, size_type };
	if (Watchlist.find(new_watch) == Watchlist.end()) {
		Watchlist.insert(new_watch);
	}
}

void debugger_remove_watch(uint16_t address, uint8_t bank, uint8_t size_type)
{
	if (address < 0xa000) {
		bank = 0;
	}

	watch_address_type old_watch{ address, bank, size_type };
	Watchlist.erase(old_watch);
}

const watch_address_list &debugger_get_watchlist()
{
	return Watchlist;
}
