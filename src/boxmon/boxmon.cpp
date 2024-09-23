#include "boxmon.h"

#include <cstdarg>
#include <fstream>

#include "command.h"
#include "parser.h"

boxmon::parser Console_parser;

std::vector<boxmon::console_line_type> Console_history;
std::vector<std::string>               Command_history;

bool Console_suppress_output   = false;
bool Console_suppress_warnings = false;
bool Console_suppress_errors   = false;

enum class parse_command_result {
	ok,
	parse_error,
	not_found
};

void boxmon_system_init()
{
}

void boxmon_system_shutdown()
{
}

bool boxmon_load_file(const std::filesystem::path &path)
{
	std::ifstream infile(path, std::ios_base::in);

	if (!infile.is_open()) {
		return false;
	}

	boxmon::parser file_parser;

	int         line_number = 0;
	std::string line;
	while (std::getline(infile, line)) {
		++line_number;
		char const *input = line.c_str();
		file_parser.skip_whitespace(input);
		if (*input == '\0') {
			continue;
		}

		std::string command_name;
		if (!file_parser.parse_word(command_name, input)) {
			std::stringstream ss;
			ss << "Parse error on line " << line_number << ": " << line << std::endl;
			Console_history.push_back({ boxmon::message_severity::error, ss.str() });
			continue;
		}

		const boxmon::boxmon_command *cmd = boxmon::boxmon_command::find(command_name.c_str());
		if (cmd == nullptr) {
			std::stringstream ss;
			ss << "Unknown command on line " << line_number << ": \"" << command_name << "\"." << std::endl;
			Console_history.push_back({ boxmon::message_severity::error, ss.str() });
			continue;
		}

		if (!cmd->run(input, file_parser, false)) {
			std::stringstream ss;
			ss << "Parse error on line " << line_number << " while running \"" << command_name << "\" with args: " << input << std::endl;
			Console_history.push_back({ boxmon::message_severity::error, ss.str() });
		}
	}
	return true;
}

bool boxmon_do_console_command(const std::string &line)
{
	char const *input = line.c_str();
	Console_parser.skip_whitespace(input);
	if (*input == '\0') {
		return true;
	}

	std::string command_name;
	if (!Console_parser.parse_word(command_name, input)) {
		std::stringstream ss;
		ss << "Parse error: " << line << std::endl;
		Console_history.push_back({ boxmon::message_severity::error, ss.str() });
		return false;
	}

	const boxmon::boxmon_command *cmd = boxmon::boxmon_command::find(command_name.c_str());
	if (cmd == nullptr) {
		std::stringstream ss;
		ss << "Unknown command \"" << command_name << "\"" << std::endl;
		Console_history.push_back({ boxmon::message_severity::error, ss.str() });
		return false;
	}

	if (!cmd->run(input, Console_parser, false)) {
		std::stringstream ss;
		ss << "Parse error while running \"" << command_name << "\" with args: " << input << std::endl;
		Console_history.push_back({ boxmon::message_severity::error, ss.str() });
		return false;
	}
	return true;
}

const std::vector<boxmon::console_line_type> &boxmon_get_console_history()
{
	return Console_history;
}

const std::vector<std::string> &boxmon_get_command_history()
{
	return Command_history;
}

void boxmon_clear_console_history()
{
	Console_history.clear();
}

void boxmon_console_print(boxmon::message_severity severity, const std::string& message)
{
	Console_history.push_back({ severity, message });
}
