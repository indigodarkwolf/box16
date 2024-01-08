#pragma once

#include <filesystem>
#include <string>
#include <vector>

void boxmon_system_init();
void boxmon_system_shutdown();

bool boxmon_load_file(const std::filesystem::path &path);
bool boxmon_do_console_command(const std::string &command);

void boxmon_console_printf(char const *format, ...);
void boxmon_warning_printf(char const *format, ...);
void boxmon_error_printf(char const *format, ...);

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
