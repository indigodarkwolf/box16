#include "command.h"

#include <algorithm>
#include <string.h>

#include "boxmon.h"
#include "parser.h"

namespace boxmon
{
	boxmon_command::boxmon_command(char const *name, char const *description, std::function<bool(char const *, boxmon::parser &)> fn)
	    : m_name(name),
	      m_description(description),
	      m_run(fn)
	{
		auto &command_list = get_command_list();
		command_list.push_back(this);
	}

	std::strong_ordering boxmon_command::operator<=>(char const *name) const
	{
		return strcmp(m_name, name) <=> 0;
	}

	std::strong_ordering boxmon_command::operator<=>(const boxmon_command &cmd) const
	{
		return strcmp(m_name, cmd.m_name) <=> 0;
	}

	bool boxmon_command::run(char const *&input, boxmon::parser &parser) const
	{
		return m_run != nullptr ? m_run(input, parser) : false;
	}

	char const *boxmon_command::get_name() const
	{
		return m_name;
	}

	char const *boxmon_command::get_description() const
	{
		return m_description;
	}

	void boxmon_command::finalize_list()
	{
		auto &command_list = get_command_list();
		std::sort(begin(command_list), end(command_list), [](const boxmon_command *a, const boxmon_command *b) { return *a > *b; });
	}

	const boxmon_command *boxmon_command::find(char const *name)
	{
		const auto &command_list = get_command_list();

		size_t search_min = 0;
		size_t search_max = command_list.size()-1;

		while (search_min != search_max) {
			const auto search_i = (search_min + search_max) >> 1;
			const auto cmp_i    = *command_list[search_i] <=> name;
			if (is_eq(cmp_i)) {
				return command_list[search_i];
			} else if (is_lt(cmp_i)) {
				search_max = std::max(search_min, search_i - 1);
			} else {
				search_min = std::min(search_max, search_i + 1);
			}
		}

		if (is_eq(*command_list[search_min] <=> name)) {
			return command_list[search_min];
		}

		return nullptr;
	}

	void boxmon_command::for_each(std::function<void(const boxmon_command *cmd)> fn)
	{
		const auto &command_list = get_command_list();
		for (auto cmd : command_list) {
			fn(cmd);
		}
	}

	void boxmon_command::for_each_partial(char const *name, std::function<void(const boxmon_command *cmd)> fn)
	{
		const auto &command_list = get_command_list();
		for (auto cmd : command_list) {
			if (strstr(cmd->get_name(), name) != nullptr) {
				fn(cmd);
			} else if (strstr(cmd->get_description(), name) != nullptr) {
				fn(cmd);
			}
		}
	}

	std::vector<const boxmon_command *> &boxmon_command::get_command_list()
	{
		static std::vector<const boxmon_command *> command_list;
		return command_list;
	}

	boxmon_alias::boxmon_alias(char const *name, const boxmon_command &cmd)
	    : boxmon_command(name, cmd.get_description(), [this](const char *input, boxmon::parser &parser) { return m_cmd.run(input, parser); }),
	      m_cmd(cmd)
	{
	}
} // namespace boxmon

#include "symbols.h"

BOXMON_COMMAND(help, "help")
{
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
	return true;
}

BOXMON_COMMAND(eval, "eval <expr>")
{
	const boxmon::boxmon_expression *expr;
	if (parser.parse_expression(expr, input)) {
		boxmon_console_printf("%d", expr->evaluate());
		return true;
	}

	return false;
}

BOXMON_COMMAND(break, "break [load|store|exec] [address [address] [if <cond_expr>]]")
{
	uint8_t breakpoint_flags = 0;
	for (int option; parser.parse_option(option, { "exec", "load", "store" }, input);) {
		breakpoint_flags |= (1 << option);
	}
	if (breakpoint_flags == 0) {
		breakpoint_flags = DEBUG6502_EXEC;
	}

	std::list<breakpoint_type> bps;
	for (breakpoint_type bp; parser.parse_address(bp, input);) {
		bps.push_back(bp);
	}

	for (auto bp : bps) {
		debugger_add_breakpoint(std::get<0>(bp), std::get<1>(bp), breakpoint_flags);
	}

	return true;
}

BOXMON_ALIAS(br, break);

BOXMON_COMMAND(add_label, "add_label <address> <label>")
{
	breakpoint_type addr;
	if (!parser.parse_address(addr, input)) {
		return false;
	}

	std::string label;
	if (!parser.parse_label(label, input)) {
		return false;
	}

	// symbols_add_label(addr, label);
	return true;
}

BOXMON_ALIAS(al, add_label);

//// Machine state commands
// bool parse_backtrace(char const *&input);
// bool parse_cpuhistory(char const *&input);
// bool parse_dump(char const *&input);
// bool parse_goto(char const *&input);
// bool parse_io(char const *&input);
// bool parse_next(char const *&input);
// bool parse_registers(char const *&input);
// bool parse_reset(char const *&input);
// bool parse_return(char const *&input);
// bool parse_step(char const *&input);
// bool parse_stopwatch(char const *&input);
// bool parse_undump(char const *&input);
// bool parse_warp(char const *&input);

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

//// General commands
// bool parse_cd(char const *&input);
// bool parse_device(char const *&input);
// bool parse_dir(char const *&input);
// bool parse_pwd(char const *&input);
// bool parse_mkdir(char const *&input);
// bool parse_rmdir(char const *&input);
// bool parse_radix(char const *&input);
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