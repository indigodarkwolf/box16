#include "symbols.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <unordered_map>

#include "debugger.h"

using symbol_address_type      = uint32_t;
using loaded_symbol_type       = std::tuple<symbol_address_type, std::string>;
using loaded_symbol_files_type = std::unordered_map<std::string, std::list<loaded_symbol_type>>;
using symbol_table_type        = std::map<symbol_address_type, symbol_list_type>;

symbol_table_type        Symbols_table;
loaded_symbol_files_type Loaded_symbols_by_file;
std::set<std::string>    Loaded_symbol_files;
std::set<std::string>    Visible_symbol_files;

const symbol_list_type Empty_symbols_list;

std::set<std::string> Ignore_list = {
	//".__BSS_LOAD__",
	//".__BSS_RUN__",
	".__BSS_SIZE__",
	".__EXEHDR__",
	".__HEADER_FILEOFFS__",
	//".__HEADER_LAST__",
	".__HEADER_SIZE__",
	//".__HEADER_START__",
	".__HIMEM__",
	".__LOADADDR__",
	".__MAIN_FILEOFFS__",
	//".__MAIN_LAST__",
	".__MAIN_SIZE__",
	//".__MAIN_START__",
	//".__ONCE_LOAD__",
	//".__ONCE_RUN__",
	".__ONCE_SIZE__",
	".__STACKSIZE__",
	".__ZP_FILEOFFS__",
	".__ZP_LAST__",
	".__ZP_SIZE__",
	".__ZP_START__"
};

namespace vice_label_file
{
	enum class parse_result {
		parse_ok,
		illegal_input,
	};

	enum class parse_type {
		device
	};

	enum class device {
		device_cpu,
		device_8,
		device_9,
		device_10,
		device_11
	};

	void skip_whitespace(char const *&input)
	{
		while (!isgraph(*input)) {
			++input;
		}
	}

	parse_result parse_device(device &result, char const *&input)
	{
		char const *look = input;
		switch (*look) {
			case 'c':
				result = device::device_cpu;
				++look;
				break;
			case '8':
				result = device::device_8;
				++look;
				break;
			case '9':
				result = device::device_9;
				++look;
				break;
			case '1':
				++look;
				switch (*look) {
					case '0':
						result = device::device_10;
						++look;
						break;
					case '1':
						result = device::device_11;
						++look;
						break;
				}
			default:
				return parse_result::illegal_input;
		}

		if (*input == ':') {
			++look;
			input = look;

			skip_whitespace(input);
			return parse_result::parse_ok;
		}

		return parse_result::illegal_input;
	}

	parse_result parse_hex_number(int &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '9') {
				return true;
			}
			if (c >= 'a' && c <= 'f') {
				return true;
			}
			if (c >= 'A' && c <= 'F') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}
			if (c >= 'a' && c <= 'f') {
				return 10 + c - 'a';
			}
			if (c >= 'A' && c <= 'F') {
				return 10 + c - 'A';
			}
			return -1;
		};

		char const *look = input;
		while (!isgraph(*look)) {
			if (!isnum(*look)) {
				return parse_result::illegal_input;
			}
			result <<= 4;
			result |= tonum(*look);
			++look;
		}
		input = look;

		return parse_result::parse_ok;
	}

	parse_result parse_dec_number(int &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '9') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}

			return -1;
		};

		char const *look = input;
		while (!isgraph(*look)) {
			if (!isnum(*look)) {
				return parse_result::illegal_input;
			}
			result *= 10;
			result |= tonum(*look);
			++look;
		}
		input = look;

		return parse_result::parse_ok;
	}

	parse_result parse_oct_number(int &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '7') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '7') {
				return c - '0';
			}

			return -1;
		};

		char const *look = input;
		while (!isgraph(*look)) {
			if (!isnum(*look)) {
				return parse_result::illegal_input;
			}
			result <<= 3;
			result |= tonum(*look);
			++look;
		}
		input = look;

		return parse_result::parse_ok;
	}

	parse_result parse_bin_number(int &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '1') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '1') {
				return c - '0';
			}

			return -1;
		};

		char const *look = input;
		while (!isgraph(*look)) {
			if (!isnum(*look)) {
				return parse_result::illegal_input;
			}
			result <<= 1;
			result |= tonum(*look);
			++look;
		}
		input = look;

		return parse_result::parse_ok;
	}
} // namespace vice_label_file

static void show_file_entries(const std::string &file_path)
{
	auto entry = Loaded_symbols_by_file.find(file_path);
	if (entry != Loaded_symbols_by_file.end()) {
		auto &symbols = entry->second;
		for (auto &sym : symbols) {
			auto &[addr, name] = sym;

			const auto &table_entry = Symbols_table.find(addr);
			if (table_entry != Symbols_table.end()) {
				table_entry->second.push_back(name);
			} else {
				Symbols_table.insert({ addr, std::list<std::string>{ name } });
			}
		}
	}

	Visible_symbol_files.insert(file_path);
}

static void hide_file_entries(const std::string &file_path)
{
	auto entry = Loaded_symbols_by_file.find(file_path);
	if (entry != Loaded_symbols_by_file.end()) {
		auto &symbols = entry->second;
		for (auto &sym : symbols) {
			auto &[addr, name] = sym;

			auto &sym_list = Symbols_table[addr];
			sym_list.remove(name);
			if (sym_list.empty()) {
				Symbols_table.erase(addr);
			}
		}
	}

	Visible_symbol_files.erase(file_path);
}

bool symbols_load_file(const std::string &file_path, symbol_bank_type bank)
{
	std::ifstream infile(file_path, std::ios_base::in);
	if (!infile.is_open()) {
		return false;
	}

	std::string cmd;

	std::list<loaded_symbol_type> file_symbols;

	std::string line;
	while (std::getline(infile, line)) {
		uint32_t start = 0;
		while (!isprint(line[start]) && (start < line.size())) {
			++start;
		}

		uint32_t end = start;
		while ((line[end] != ';') && (end < line.size())) {
			++end;
		}

		if (start == end) {
			continue;
		}

		std::istringstream sline(line.substr(start, end - start));
		sline >> cmd;

		if (cmd == "al" || cmd == "add_label") {
			uint32_t    addr;
			std::string label;

			sline >> std::hex;

			sline >> addr >> label;

			if (addr > 0xffff) {
				continue;
			}
			if (label.size() == 0) {
				continue;
			}
			if (Ignore_list.find(label) != Ignore_list.end()) {
				continue;
			}

			const symbol_bank_type sym_bank    = addr < 0xa000 ? 0 : bank;
			symbol_address_type    symbol_addr = (sym_bank << 16) + addr;

			bool already_exists = false;
			for (auto &[address, symbol] : file_symbols) {
				if ((symbol_addr == address) && !symbol.compare(label)) {
					already_exists = true;
					break;
				}
			}

			if (!already_exists) {
				file_symbols.push_back(std::tuple{ symbol_addr, label });
			}
		} else if (cmd == "break") {
			uint32_t    addr;
			std::string addr_str;

			sline >> addr_str;
			std::istringstream saddr_str(addr_str[0] == '$' ? addr_str.substr(1, addr_str.size() - 1) : addr_str);
			saddr_str >> std::hex;
			saddr_str >> addr;
			debugger_add_breakpoint(addr);
		}
	}

	Loaded_symbols_by_file.insert({ file_path, file_symbols });
	Loaded_symbol_files.insert(file_path);
	show_file_entries(file_path);

	return true;
}

void symbols_unload_file(const std::string &file_path)
{
	hide_file_entries(file_path);
	Loaded_symbol_files.erase(file_path);
	Loaded_symbols_by_file.erase(file_path);
}

// bool symbols_save_file(const std::filesystem::path &file_path)
//{
//	std::ofstream outfile(file_path.generic_string(), std::ios_base::out);
//	if (!outfile.is_open()) {
//		return false;
//	}
//
//
// }

void symbols_refresh_file(const std::string &file_path)
{
	symbols_unload_file(file_path);
	symbols_load_file(file_path);
}

void symbols_show_file(const std::string &file_path)
{
	if (Visible_symbol_files.find(file_path) == Visible_symbol_files.end()) {
		show_file_entries(file_path);
	}
}

void symbols_hide_file(const std::string &file_path)
{
	if (Visible_symbol_files.find(file_path) != Visible_symbol_files.end()) {
		hide_file_entries(file_path);
	}
}

const std::set<std::string> &symbols_get_loaded_files()
{
	return Loaded_symbol_files;
}

bool symbols_file_all_are_visible()
{
	for (const auto &file : Loaded_symbol_files) {
		if (!symbols_file_is_visible(file)) {
			return false;
		}
	}

	return true;
}

bool symbols_file_any_is_visible()
{
	return !Visible_symbol_files.empty();
}

bool symbols_file_is_visible(const std::string &file_path)
{
	return Visible_symbol_files.find(file_path) != Visible_symbol_files.end();
}

const symbol_list_type &symbols_find(uint32_t address, symbol_bank_type bank)
{
	if (address < 0xa000) {
		bank = 0;
	}

	auto entry = Symbols_table.find((bank << 16) + address);
	if (entry == Symbols_table.end()) {
		return Empty_symbols_list;
	}

	return entry->second;
}

void symbols_for_each(std::function<void(uint16_t, symbol_bank_type, const std::string &)> fn)
{
	for (auto &entry : Symbols_table) {
		uint16_t         addr = entry.first & 0xffff;
		symbol_bank_type bank = entry.first >> 16;
		for (const std::string &name : entry.second) {
			fn(addr, bank, name);
		}
	}
}