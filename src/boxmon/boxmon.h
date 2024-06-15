#pragma once

#include <filesystem>
#include <string>
#include <vector>

extern bool Console_suppress_output;
extern bool Console_suppress_warnings;
extern bool Console_suppress_errors;

void boxmon_system_init();
void boxmon_system_shutdown();

bool boxmon_load_file(const std::filesystem::path &path);
bool boxmon_do_console_command(const std::string &command);

namespace boxmon
{
	enum message_severity {
		output,
		warning,
		error
	};

	using console_line_type = std::tuple<message_severity, const std::string>;
} // namespace boxmon

const std::vector<boxmon::console_line_type> &boxmon_get_console_history();
const std::vector<std::string>               &boxmon_get_command_history();

void boxmon_clear_console_history();

void boxmon_console_print(boxmon::message_severity severity, const std::string &message);

template <typename... T>
void boxmon_console_print(const std::string &format_string, T && ...args)
{
	if (Console_suppress_output) {
		return;
	}

	boxmon_console_print(boxmon::message_severity::output, fmt::format(fmt::runtime(format_string), args...));
}

template <typename... T>
void boxmon_warning_print(const std::string &format_string, T &&...args)
{
	if (Console_suppress_warnings) {
		return;
	}

	boxmon_console_print(boxmon::message_severity::warning, fmt::format(fmt::runtime(format_string), args...));
}

template <typename... T>
void boxmon_error_print(const std::string &format_string, T &&...args)
{
	if (Console_suppress_errors) {
		return;
	}

	boxmon_console_print(boxmon::message_severity::error, fmt::format(fmt::runtime(format_string), args...));
}
