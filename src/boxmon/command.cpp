#include "command.h"

#include <algorithm>
#include <string>
#include <cinttypes>

#include "boxmon.h"
#include "parser.h"

#include "cpu/fake6502.h"
#include "cpu/mnemonics.h"
#include "debugger.h"
#include "glue.h"
#include "hypercalls.h"
#include "memory.h"
#include "vera/sdcard.h"
#include "vera/vera_video.h"

namespace boxmon
{
	boxmon_command::boxmon_command(char const *name, char const *description, std::function<bool(char const *, boxmon::parser &, bool)> fn)
	    : m_name(name),
	      m_description(description),
	      m_run(fn)
	{
		auto &command_list = get_command_list();
		command_list.insert({ name, this });
	}

	std::strong_ordering boxmon_command::operator<=>(char const *name) const
	{
		return strcmp(m_name, name) <=> 0;
	}

	std::strong_ordering boxmon_command::operator<=>(const boxmon_command &cmd) const
	{
		return strcmp(m_name, cmd.m_name) <=> 0;
	}

	bool boxmon_command::run(char const *&input, boxmon::parser &parser, bool help) const
	{
		return m_run != nullptr ? m_run(input, parser, help) : false;
	}

	char const *boxmon_command::get_name() const
	{
		return m_name;
	}

	char const *boxmon_command::get_description() const
	{
		return m_description;
	}

	const boxmon_command *boxmon_command::find(char const *name)
	{
		const auto &command_list = get_command_list();
		const auto icmd = command_list.find(name);
		if (icmd != command_list.end()) {
			return icmd->second;
		}
		return nullptr;
	}

	void boxmon_command::for_each(std::function<void(const boxmon_command *cmd)> fn)
	{
		const auto &command_list = get_command_list();
		for (auto cmd : command_list) {
			fn(cmd.second);
		}
	}

	void boxmon_command::for_each_partial(char const *name, std::function<void(const boxmon_command *cmd)> fn)
	{
		const auto &command_list = get_command_list();
		for (auto cmd : command_list) {
			if (strstr(cmd.second->get_name(), name) != nullptr) {
				fn(cmd.second);
			} else if (strstr(cmd.second->get_description(), name) != nullptr) {
				fn(cmd.second);
			}
		}
	}

	std::map<const std::string, const boxmon_command *> &boxmon_command::get_command_list()
	{
		static std::map<const std::string, const boxmon_command *> command_list;
		return command_list;
	}

	boxmon_alias::boxmon_alias(char const *name, const boxmon_command &cmd)
	    : boxmon_command(name, cmd.get_description(), [this](const char *input, boxmon::parser &parser, bool help) { return m_cmd.run(input, parser, help); }),
	      m_cmd(cmd)
	{
	}
} // namespace boxmon

#include "symbols.h"

BOXMON_COMMAND(help, "help [<command>]")
{
	if (help) {
		boxmon_console_printf("Print extended use information about a command.");
		boxmon_console_printf("If no command is specified, help returns a list of all commands the console will accept.");
		return true;
	}

	std::string command;
	if (parser.parse_word(command, input)) {
		auto const *cmd = boxmon::boxmon_command::find(command.c_str());
		if (cmd == nullptr) {
			boxmon_warning_printf("Could not find any command named \"%s\"", command.c_str());
		} else {
			boxmon_console_printf("%s", cmd->get_description());
			const char *help_input = "";
			return cmd->run(help_input, parser, true);
		}
	} else {
		std::string name;
		if (parser.parse_word(name, input)) {
			if (auto *cmd = boxmon::boxmon_command::find(name.c_str()); cmd != nullptr) {
				boxmon_console_printf("%s: %s", cmd->get_name(), cmd->get_description());
				return true;
			}
		}
		boxmon::boxmon_command::for_each([](const boxmon::boxmon_command *cmd) {
			boxmon_console_printf("%s: %s", cmd->get_name(), cmd->get_description());
		});
	}
	return true;
}

BOXMON_COMMAND(eval, "eval <expr>")
{
	if (help) {
		boxmon_console_printf("Evaluates an expression and prints the result to the console as a decimal integer.");
		boxmon_console_printf("Intermediate values are stored as signed 32-bit integers. Memory reads from dereferencing are treated as unsigned 8-bit integers.");
		boxmon_console_printf("Expressions support most C-style mathematical, comparison, boolean, and bitwise operators:");
		boxmon_console_printf("Math: +, -, *, /, %, ^^, ()");
		boxmon_console_printf("\t+: Addition. 2+3 returns 5.");
		boxmon_console_printf("\t-: Subtraction and negation. 2-3 returns -1. -2 returns -2.");
		boxmon_console_printf("\t*: Multiplication. 2*3 returns 6.");
		boxmon_console_printf("\t/: Division. 10/2 returns 5.");
		boxmon_console_printf("\t%: Modulo. 4%3 returns 1.");
		boxmon_console_printf("\t^^: Exponentiation. 2^^3 returns 8.");
		boxmon_console_printf("\t(): Parenthesis. (1+2)*3 returns 9.");
		boxmon_console_printf("Compare: ==, !=, <, >, <=, >=");
		boxmon_console_printf("\t==: Equality. 2==2 returns 1 (true), 2==3 returns 0 (false).");
		boxmon_console_printf("\t!=: Inequality. 2!=2 returns 0 (false), 2!=3 returns 1 (true).");
		boxmon_console_printf("\t<: Less than. 2<3 returns 1 (true), 3<2 returns 0 (false).");
		boxmon_console_printf("\t>: Greater than. 2>3 returns 0 (false), 3>2 returns 1 (true).");
		boxmon_console_printf("\t<=: Less than or equal to. 2<=2 returns 1, 2<=3 returns 1.");
		boxmon_console_printf("\t>=: Greater than or equal to. 2>=2 returns 1, 3>=2 returns 1.");
		boxmon_console_printf("Bool: &&, ||, !");
		boxmon_console_printf("\t&&: Boolean AND. 1 && 1 returns 1, 1 && 0 returns 0, 0 && 0 returns 0.");
		boxmon_console_printf("\t||: Boolean OR. 1 || 1 returns 1, 1 || 0 returns 1, 0 || 0 returns 0.");
		boxmon_console_printf("\t!: Boolean NOT. !1 returns 0, !0 returns 1.");
		boxmon_console_printf("Bitwise: &, |, ~, ^, <<, >>");
		boxmon_console_printf("\t&: Bitwise AND. 3&1 returns 1, 2&1 returns 0.");
		boxmon_console_printf("\t|: Bitwise OR. 3|1 returns 3, 2|1 returns 3.");
		boxmon_console_printf("\t~: Bitwise NOT. ~1 returns -2, ~2 returns -3. (Additional reading left to the user: Two's complement signed integers.)");
		boxmon_console_printf("\t^: Bitwise XOR. 3^1 returns 2, 2^1 returns 3.");
		boxmon_console_printf("\t<<: Left shift. 1<<2 returns 4.");
		boxmon_console_printf("\t>>: Right shift. 4>>2 returns 1.");
		boxmon_console_printf("Additionally, the symbol @ will treat the value to its right as a memory address and attempt to retrieve the value at that address, similar to the C-style * for pointer dereferencing.");
		boxmon_console_printf("\t@: Dereferencing. @3 returns the value stored at $0003.");
		boxmon_console_printf("C-style precedence rules should apply to each of these operators.");
		boxmon_console_printf("Expressions may include integer values and symbol names. Symbol names are substituted as the address associated with the symbol.");
		boxmon_console_printf("If the same symbol name is defined multiple times, the selection process is undefined.");
		boxmon_console_printf("Numbers are parsed assuming a default radix, see the \"radix\" command for more information.");
		boxmon_console_printf("The default radix can be overridden, however, by specifying a radix followed by a space, immediately before a number. Examples include:");
		boxmon_console_printf("\t\"b <number>\": Parse this number as a binary integer.");
		boxmon_console_printf("\t\"o <number>\": Parse this number as an octal integer.");
		boxmon_console_printf("\t\"d <number>\": Parse this number as a decimal integer.");
		boxmon_console_printf("\t\"h <number>\": Parse this number as a hexadecimal integer.");
		boxmon_console_printf("Certain C-like number prefixes will also override the default radix:");
		boxmon_console_printf("\t%%101: This number is parsed as a binary integer.");
		boxmon_console_printf("\t0101, o101, O101: These numbers are parsed as octal integers.");
		boxmon_console_printf("\t#101: This number is parsed as a decimal integer.");
		boxmon_console_printf("\t$101, h101, 0x101: These numbers are parsed as hexadecimal integers.");
		return true;
	}
	const boxmon::expression *expr;
	if (parser.parse_expression(expr, input, boxmon::expression_parse_flags_must_consume_all)) {
		boxmon_console_printf("%d", expr->evaluate());
		return true;
	}

	return false;
}

BOXMON_ALIAS(print, eval);

BOXMON_COMMAND(break, "break [load|store|exec] [address [address] [if <cond_expr>]]")
{
	if (help) {
		boxmon_console_printf("Create a breakpoint, optionally with a conditional expression.");
		boxmon_console_printf("\tload: Break if the CPU attempts to load data from this address.");
		boxmon_console_printf("\tstore: Break if the CPU attempts to store data to this address.");
		boxmon_console_printf("\texec: Break if the CPU attempts to execute an instruction from this address.");
		boxmon_console_printf("\taddress: One or more addresses to set as breakpoints.");
		boxmon_console_printf("\tcond_expr: Conditional expression following the same rules and syntax as \"eval\". If specified, the breakpoint will only pause execution if the conditional expression evaluates to a non-zero value.");
		boxmon_console_printf("\t           (In the case of boolean comparisons, \"true\" evaluates to 1, \"false\" evaluates to 0.)");
		return true;
	}
	uint8_t breakpoint_flags = 0;
	for (int option; parser.parse_option(option, { "exec", "load", "store" }, input);) {
		breakpoint_flags |= (1 << option);
	}
	if (breakpoint_flags == 0) {
		breakpoint_flags = DEBUG6502_EXEC;
	}

	std::list<boxmon::address_type> bps;
	for (boxmon::address_type bp; parser.parse_address(bp, input);) {
		bps.push_back(bp);
	}

	if (int option; parser.parse_option(option, { "if" }, input)) {
		if (const boxmon::expression *expr = nullptr; parser.parse_expression(expr, input, boxmon::expression_parse_flags_must_consume_all)) {
			for (auto bp : bps) {
				breakpoint_flags |= DEBUG6502_CONDITION;
				debugger_add_breakpoint(std::get<0>(bp), std::get<1>(bp), breakpoint_flags);
				debugger_set_condition(std::get<0>(bp), std::get<1>(bp), expr->get_string());
			}
		}
	} else {
		for (auto bp : bps) {
			debugger_add_breakpoint(std::get<0>(bp), std::get<1>(bp), breakpoint_flags);
		}	
	}

	return true;
}

BOXMON_ALIAS(br, break);

BOXMON_COMMAND(add_label, "add_label <address> <label>")
{
	if (help) {
		boxmon_console_printf("Add a label for a specified address.");
		return true;
	}
	boxmon::address_type addr;
	if (!parser.parse_address(addr, input)) {
		return false;
	}

	std::string label;
	if (!parser.parse_label(label, input)) {
		return false;
	}

	symbols_add(std::get<0>(addr), std::get<1>(addr), label);
	return true;
}

BOXMON_ALIAS(al, add_label);

BOXMON_COMMAND(backtrace, "backtrace")
{
	if (help) {
		boxmon_console_printf("Attempt to unwind the callstack of execution.");
		boxmon_console_printf("This is a best-effort attempt based on a history of jsr, rts, and rti instructions, as well as interrupt triggers.");
		boxmon_console_printf("Coding practices that manually push or pop values in lieu of subroute and interrupt instructions will easily confuse this.");
		return true;
	}

	char const *names[] = { "N", "V", "-", "B", "D", "I", "Z", "C" };
	for (size_t i = 0; i < stack6502.count(); ++i) {
		const auto &ss = stack6502[static_cast<int>(i)];
		if (ss.push.op_type >= _stack_op_type::push_op) {
			continue;
		}
		char const *op = [&]() -> char const * {
			switch (ss.push.op_type) {
				case _stack_op_type::nmi:
					return "NMI";
					break;
				case _stack_op_type::irq:
					return "IRQ";
					break;
				case _stack_op_type::jsr:
					return "JSR";
					break;
				case _stack_op_type::smart:
					return "---";
					break;
				default:
					break;
			}
			return "???";
		}();

		boxmon_console_printf("% 3d: %s PC:%02x:%04X -> %02x:%04X A:%02X X:%02X Y:%02X SP:%02X ST:%c%c-%c%c%c%c%c", 
			i, 
			op, ss.push.pc_bank, ss.push.state.pc, ss.push.jmp_data.dest_bank, ss.push.jmp_data.dest_pc, ss.push.state.a, ss.push.state.x, ss.push.state.y, ss.push.state.sp, 
			ss.push.state.status & 0x80 ? 'N' : '-', 
			ss.push.state.status & 0x40 ? 'V' : '-', 
			ss.push.state.status & 0x10 ? 'B' : '-', 
			ss.push.state.status & 0x08 ? 'D' : '-', 
			ss.push.state.status & 0x04 ? 'I' : '-', 
			ss.push.state.status & 0x02 ? 'Z' : '-', 
			ss.push.state.status & 0x01 ? 'C' : '-');
	}
	return true;
}

BOXMON_ALIAS(bt, backtrace);

//// Machine state commands
BOXMON_COMMAND(cpuhistory, "cpuhistory [length]")
{
	if (help) {
		boxmon_console_printf("Show a history of recently-executed instructions.");
		return true;
	}
	int history_length = 0;
	if (parser.parse_dec_number(history_length, input)) {
		history_length = history_length <= static_cast<int>(history6502.count()) ? history_length : static_cast<int>(history6502.count());
	} else {
		history_length = static_cast<int>(history6502.count());
	}

	for (int i = 0; i < history_length; ++i) {
		const auto &history = history6502[i];

		char const *op = mnemonics[history.opcode];

		boxmon_console_printf("% 3d: %s PC:%02x:%04X A:%02X X:%02X Y:%02X SP:%02X ST:%c%c-%c%c%c%c%c", i, op, history.bank, history.state.pc, history.state.a, history.state.x, history.state.y, history.state.sp, history.state.status & 0x80 ? 'N' : '-', history.state.status & 0x40 ? 'V' : '-', history.state.status & 0x10 ? 'B' : '-', history.state.status & 0x08 ? 'D' : '-', history.state.status & 0x04 ? 'I' : '-', history.state.status & 0x02 ? 'Z' : '-', history.state.status & 0x01 ? 'C' : '-');
	}
	return true;
}

BOXMON_ALIAS(chis, cpuhistory);

BOXMON_COMMAND(dump, "dump")
{
	if (help) {
		boxmon_console_printf("Perform a machine dump to file.");
		return true;
	}

	extern void machine_dump(const char *reason);
	machine_dump("monitor command");
	return true;
}

BOXMON_COMMAND(goto, "goto <address>")
{
	if (help) {
		boxmon_console_printf("Set the program counter to a specified memory address.");
		boxmon_console_printf("If the address is greater than $FFFF, this will also set the appropriate memory bank to the contents of the high byte in the specified address.");
		return true;
	}

	boxmon::address_type addr;
	if (!parser.parse_address(addr, input)) {
		return false;
	}

	const auto &[pc, bank] = addr;
	state6502.pc = pc;
	memory_set_bank(pc, bank);

	return true;
}

BOXMON_ALIAS(g, goto);

BOXMON_COMMAND(io, "io")
{
	if (help) {
		boxmon_console_printf("Print the current read values of the IO registers to console.");
		return true;
	}

	auto printio = [](char const *name, uint16_t addr) {
		boxmon_console_printf("%-4s $%04X: $%02X", name, addr, debug_read6502(addr));
	};

	// VIA1
	for (uint16_t i = 0; i < 16; ++i) {
		printio("VIA1", 0x9f00 + i);
	}
	// VIA2
	for (uint16_t i = 0; i < 16; ++i) {
		printio("VIA2", 0x9f10 + i);
	}
	// VERA
	for (uint16_t i = 0; i < 32; ++i) {
		printio("VERA", 0x9f20 + i);
	}
	// YM
	for (uint16_t i = 0; i < 2; ++i) {
		printio("YM", 0x9f40 + i);
	}
	// IO3
	for (uint16_t i = 0; i < 32; ++i) {
		printio("IO3", 0x9f60 + i);
	}
	// IO4
	for (uint16_t i = 0; i < 32; ++i) {
		printio("IO4", 0x9f80 + i);
	}
	// IO5
	for (uint16_t i = 0; i < 32; ++i) {
		printio("IO5", 0x9fA0 + i);
	}
	// IO6
	for (uint16_t i = 0; i < 32; ++i) {
		printio("IO6", 0x9fC0 + i);
	}
	// IO7
	for (uint16_t i = 0; i < 32; ++i) {
		printio("IO7", 0x9fE0 + i);
	}

	return true;
}

BOXMON_COMMAND(iowide, "iowide")
{
	if (help) {
		boxmon_console_printf("Print the current read values of the IO registers to console, but grouped into lines of 16 bytes.");
		return true;
	}

	auto printio = [](char const *name, uint16_t addr) {
		boxmon_console_printf("%-4s $%04X: $%02X $%02X $%02X $%02X $%02X $%02X $%02X $%02X   $%02X $%02X $%02X $%02X $%02X $%02X $%02X $%02X", name, addr, 
			debug_read6502(addr + 0),
			debug_read6502(addr + 1),
			debug_read6502(addr + 2),
			debug_read6502(addr + 3),
			debug_read6502(addr + 4),
			debug_read6502(addr + 5),
			debug_read6502(addr + 6),
			debug_read6502(addr + 7), 
			debug_read6502(addr + 8), 
			debug_read6502(addr + 9), 
			debug_read6502(addr + 10), 
			debug_read6502(addr + 11), 
			debug_read6502(addr + 12), 
			debug_read6502(addr + 13), 
			debug_read6502(addr + 14), 
			debug_read6502(addr + 15));
	};

	// VIA1
	for (uint16_t i = 0; i < 16; i += 16) {
		printio("VIA1", 0x9f00 + i);
	}
	// VIA2
	for (uint16_t i = 0; i < 16; i += 16) {
		printio("VIA2", 0x9f10 + i);
	}
	// VERA
	for (uint16_t i = 0; i < 32; i += 16) {
		printio("VERA", 0x9f20 + i);
	}
	// YM
	for (uint16_t i = 0; i < 2; i += 16) {
		boxmon_console_printf("%-4s $%04X: $%02X $%02X", "YM", 0xf940, 
			debug_read6502(0xf940 + 0), 
			debug_read6502(0xf940 + 1));
	}
	// IO3
	for (uint16_t i = 0; i < 32; i += 16) {
		printio("IO3", 0x9f60 + i);
	}
	// IO4
	for (uint16_t i = 0; i < 32; i += 16) {
		printio("IO4", 0x9f80 + i);
	}
	// IO5
	for (uint16_t i = 0; i < 32; i += 16) {
		printio("IO5", 0x9fA0 + i);
	}
	// IO6
	for (uint16_t i = 0; i < 32; i += 16) {
		printio("IO6", 0x9fC0 + i);
	}
	// IO7
	for (uint16_t i = 0; i < 32; i += 16) {
		printio("IO7", 0x9fE0 + i);
	}

	return true;
}

BOXMON_ALIAS(iow, iowide);

BOXMON_COMMAND(next, "next [<count>]")
{
	if (help) {
		boxmon_console_printf("Execute the next <count> instructions.");
		boxmon_console_printf("If left unspecified, <count> defaults to 1.");
		return true;
	}

	int count = 0;
	(void)parser.parse_dec_number(count, input);

	if (count > 0) {
		debugger_step_execution(static_cast<uint32_t>(count));
	} else {
		debugger_step_execution();
	}

	return true;
}

// TODO: registers
// bool parse_registers(char const *&input);

// bool parse_reset(char const *&input);
BOXMON_COMMAND(reset, "reset")
{
	if (help) {
		boxmon_console_printf("Perform a machine reset.");
		return true;
	}

	machine_reset();
	return true;
}

BOXMON_COMMAND(return, "return")
{
	if (help) {
		boxmon_console_printf("Continue execution until after the next rts or rti instruction.");
		return true;
	}

	debugger_step_out_execution();
	return true;
}

BOXMON_ALIAS(step, next);

BOXMON_COMMAND(stopwatch, "stopwatch")
{
	if (help) {
		boxmon_console_printf("Print the current CPU clock tick value to the console.");
		return true;
	}

	boxmon_console_printf("%" PRIu64, clockticks6502);
	return true;
}

// TODO: undump
// bool parse_undump(char const *&input);

BOXMON_COMMAND(warp, "warp [<factor>]")
{
	if (help) {
		boxmon_console_printf("Set or toggle warp mode.");
		boxmon_console_printf("\tfactor: A value from 0-16 indicating the warp factor to use. If not specified, warp will be disabled if currently active and will be set to factor 1 if currently inactive.");
		boxmon_console_printf("\tWhen activated, warp mode removes all throttling from the emulator and attempts to run the emulated system as quickly as possible.");
		boxmon_console_printf("\tLarger warp factors reduce the number of attempts to draw the screen, as that is the single most expensive task to perform.");
		return true;
	}

	int factor = 0;
	if (parser.parse_dec_number(factor, input)) {
		Options.warp_factor = std::clamp(factor, 0, 16);
		if (Options.warp_factor == 0) {
			vera_video_set_cheat_mask(0);
		} else {
			vera_video_set_cheat_mask((1 << (Options.warp_factor - 1)) - 1);
		}
	} else {
		if (Options.warp_factor > 0) {
			Options.warp_factor = 0;
			vera_video_set_cheat_mask(0);
		} else {
			Options.warp_factor = 1;
			vera_video_set_cheat_mask(1);
		}
	}
	return true;
}

//// Memory commands
// bool parse_bank(char const *&input);
// bool parse_compare(char const *&input);
// bool parse_device(char const *&input);
// bool parse_fill(char const *&input);
// bool parse_hunt(char const *&input);
// bool parse_i(char const *&input);
// bool parse_ii(char const *&input);
// bool parse_mem(char const *&input);
// bool parse_memmapshow(char const *&input);
// bool parse_memmapzap(char const *&input);
// bool parse_memmapsave(char const *&input);
// bool parse_memchar(char const *&input);
// bool parse_memsprite(char const *&input);
// bool parse_move(char const *&input);
// bool parse_screen(char const *&input);
// bool parse_sidefx(char const *&input);
// bool parse_write(char const *&input);

//// Assembly commands
// bool parse_a(char const *&input);
// bool parse_disass(char const *&input);

//// Checkpoint commands
// bool parse_break(char const *&input);
// bool parse_enable(char const *&input);
// bool parse_disable(char const *&input);
// bool parse_command(char const *&input);
// bool parse_condition(char const *&input);
// bool parse_delete(char const *&input);
// bool parse_ignore(char const *&input);
// bool parse_trace(char const *&input);
// bool parse_until(char const *&input);
// bool parse_watch(char const *&input);
// bool parse_dummy(char const *&input);

static bool check_hostfs()
{
	if (!hypercalls_allowed()) {
		boxmon_warning_printf("Hostfs emulation is currently disabled.");

		if (sdcard_is_attached()) {
			boxmon_warning_printf("SDCard is attached.");
		}

		if (Options.no_ieee_hypercalls) {
			boxmon_warning_printf("IEEE hypercalls have been disabled.");
		}

		if (Options.enable_serial) {
			boxmon_warning_printf("Bit-level serial bus emulation is enabled.");
		}
		return false;
	}
	return true;
}

//// General commands
BOXMON_COMMAND(cd, "cd <directory>")
{
	if (help) {
		boxmon_console_printf("Change the base directory for filesystem access.");
		boxmon_console_printf("At present, this only works when in hostfs emulation mode.");
		boxmon_console_printf("This will also reset the cwd of hostfs emulation to the new location.");
		return true;
	}

	if (!check_hostfs()) {
		return true;
	}

	std::string path_string;
	if (!parser.parse_string(path_string, input)) {
		return false;
	}

	std::filesystem::path path = path_string;
	std::filesystem::path abs_path = path.is_absolute() ? path : std::filesystem::absolute(Options.fsroot_path / path);

	if (std::filesystem::exists(abs_path)) {
		if (std::filesystem::is_directory(abs_path)) {
			Options.fsroot_path = abs_path;
			Options.startin_path = Options.fsroot_path;
			boxmon_console_printf("%s", Options.fsroot_path.generic_string().c_str());
		} else {
			boxmon_warning_printf("Path is not a directory: %s", abs_path.generic_string().c_str());		
		}
	} else {
		boxmon_warning_printf("Path does not exist: %s", abs_path.generic_string().c_str());
	}

	return true;
}

// bool parse_device(char const *&input);

BOXMON_COMMAND(dir, "dir")
{
	if (help) {
		boxmon_console_printf("List the contents of the current directory.");
		boxmon_console_printf("At present, this only works when in hostfs emulation mode.");
		return true;
	}

	if (!check_hostfs()) {
		return true;
	}

	boxmon_console_printf("Directory listing of %s", Options.fsroot_path.generic_string().c_str());
	for (auto const &dir_entry : std::filesystem::directory_iterator{ Options.fsroot_path }) {
		auto const relative = std::filesystem::relative(dir_entry.path(), Options.fsroot_path);
		boxmon_console_printf("%8d %s", std::filesystem::file_size(dir_entry.path()), relative.generic_string().c_str());
	}

	return true;
}

// bool parse_pwd(char const *&input);
BOXMON_COMMAND(pwd, "pwd")
{
	if (help) {
		boxmon_console_printf("Print the current working directory.");
		boxmon_console_printf("At present, this only works when in hostfs emulation mode.");
		return true;
	}

	if (!check_hostfs()) {
		return true;
	}

	boxmon_console_printf("%s", Options.fsroot_path.generic_string().c_str());
	return true;
}

// bool parse_mkdir(char const *&input);
BOXMON_COMMAND(mkdir, "mkdir <directory>")
{
	if (help) {
		boxmon_console_printf("Make a directory at the current filesystem location.");
		boxmon_console_printf("At present, this only works when in hostfs emulation mode.");
		return true;
	}

	if (!check_hostfs()) {
		return true;
	}

	std::string path_string;
	if (!parser.parse_string(path_string, input)) {
		return false;
	}

	std::filesystem::path path     = path_string;
	std::filesystem::path abs_path = path.is_absolute() ? path : std::filesystem::absolute(Options.fsroot_path / path);

	if (std::filesystem::exists(abs_path)) {
		boxmon_warning_printf("Path already exists: %s", abs_path.generic_string().c_str());
	} else {
		std::error_code ec;
		if (std::filesystem::create_directory(abs_path, ec)) {
			boxmon_console_printf("Created %s", abs_path.generic_string().c_str());
		} else {
			boxmon_warning_printf("Create failed: %s", ec.message().c_str());
		}
	}

	return true;
}

// bool parse_rmdir(char const *&input);
BOXMON_COMMAND(rmdir, "rmdir <directory>")
{
	if (help) {
		boxmon_console_printf("Remove a directory at the current filesystem location.");
		boxmon_console_printf("At present, this only works when in hostfs emulation mode.");
		return true;
	}

	if (!check_hostfs()) {
		return true;
	}

	std::string path_string;
	if (!parser.parse_string(path_string, input)) {
		return false;
	}

	std::filesystem::path path     = path_string;
	std::filesystem::path abs_path = path.is_absolute() ? path : std::filesystem::absolute(Options.fsroot_path / path);

	if (std::filesystem::exists(abs_path)) {
		if (std::filesystem::is_directory(abs_path)) {
			std::error_code ec;
			if (std::filesystem::remove(abs_path, ec)) {
				boxmon_console_printf("Removed %s", abs_path.generic_string().c_str());
			} else {
				boxmon_warning_printf("Remove failed: %s", ec.message().c_str());
			}
		} else {
			boxmon_warning_printf("Path is not a directory: %s", abs_path.generic_string().c_str());
		}
	} else {
		boxmon_warning_printf("Path does not exist: %s", abs_path.generic_string().c_str());
	}

	return true;
}

BOXMON_COMMAND(rm, "rm <file_or_directory>")
{
	if (help) {
		boxmon_console_printf("Remove a file or directory at the current filesystem location.");
		boxmon_console_printf("At present, this only works when in hostfs emulation mode.");
		return true;
	}

	if (!check_hostfs()) {
		return true;
	}

	std::string path_string;
	if (!parser.parse_string(path_string, input)) {
		return false;
	}

	std::filesystem::path path     = path_string;
	std::filesystem::path abs_path = path.is_absolute() ? path : std::filesystem::absolute(Options.fsroot_path / path);

	if (std::filesystem::exists(abs_path)) {
		std::error_code ec;
		if (std::filesystem::remove(abs_path, ec)) {
			boxmon_console_printf("Removed %s", abs_path.generic_string().c_str());
		} else {
			boxmon_warning_printf("Remove failed: %s", ec.message().c_str());
		}
	} else {
		boxmon_warning_printf("Path does not exist: %s", abs_path.generic_string().c_str());
	}

	return true;
}

static const char *radix_string(boxmon::radix_type r)
{
	switch (r) {
		case boxmon::radix_type::bin: return "binary";
		case boxmon::radix_type::oct: return "octal";
		case boxmon::radix_type::dec: return "decimal";
		case boxmon::radix_type::hex: return "hexadecimal";
	}
	return "(unknown)";
}

// bool parse_radix(char const *&input);
BOXMON_COMMAND(radix, "radix [b|o|d|h]")
{
	if (help) {
		boxmon_console_printf("Get or set the default radix for inputs to the command line.");
		boxmon_console_printf("Radix types are a single character from the following set:");
		boxmon_console_printf("\tb: Binary (base 2)");
		boxmon_console_printf("\to: Octal (base 8)");
		boxmon_console_printf("\td: Decimal (base 10)");
		boxmon_console_printf("\th: Hexadecimal (base 16)");
		boxmon_console_printf("If a number can't be parsed under the default radix, the parser will attempt to interpret it using the smallest possible radix option from the above list.");
		boxmon_console_printf("The default radix is currently: %s", radix_string(parser.get_default_radix()));
		return true;
	}

	if (*input == '\0') {
		boxmon_console_printf("Default radix is: %s", radix_string(parser.get_default_radix()));
		return true;
	}

	boxmon::radix_type radix;
	if (!parser.parse_radix_type(radix, input)) {
		return false;
	}

	parser.set_default_radix(radix);
	boxmon_console_printf("Default radix set to: %s", radix_string(parser.get_default_radix()));
	return true;
}

// bool parse_log(char const *&input);
// bool parse_logname(char const *&input);

//// Disk commands
// bool parse_attach(char const *&input);
// bool parse_block_read(char const *&input);
// bool parse_block_write(char const *&input);
// bool parse_detach(char const *&input);
// bool parse_at(char const *&input);
// bool parse_list(char const *&input);
// bool parse_load(char const *&input);
// bool parse_bload(char const *&input);
// bool parse_save(char const *&input);
// bool parse_bsave(char const *&input);
// bool parse_verify(char const *&input);
// bool parse_bverify(char const *&input);

//// Command file commands
// bool parse_playback(char const *&input);
// bool parse_record(char const *&input);
// bool parse_stop(char const *&input);

//// Label commands
// bool parse_add_label(char const *&input);
// bool parse_delete_label(char const *&input);
// bool parse_load_labels(char const *&input);
// bool parse_save_labels(char const *&input);
// bool parse_show_labels(char const *&input);
// bool parse_clear_labels(char const *&input);

//// Miscellaneous commands
// bool parse_cartfreeze(char const *&input);
// bool parse_cpu(char const *&input);
// bool parse_exit(char const *&input);
// bool parse_export(char const *&input);
// bool parse_help(char const *&input);
// bool parse_keybug(char const *&input);
// bool parse_print(char const *&input);
// bool parse_resourceget(char const *&input);
// bool parse_resourceset(char const *&input);
// bool parse_load_resources(char const *&input);
// bool parse_save_resources(char const *&input);
// bool parse_screenshot(char const *&input);
// bool parse_tapectrl(char const *&input);
// bool parse_quit(char const *&input);
// bool parse_tilde(char const *&input);
