#pragma once

#include <list>
#include <set>
#include <string>
#include <functional>

using symbol_list_type = std::list<std::string>;
using symbol_bank_type = uint8_t;

bool symbols_load_file(const std::string &file_path, symbol_bank_type bank = 0);
void symbols_unload_file(const std::string &file_path);
void symbols_refresh_file(const std::string &file_path);
void symbols_show_file(const std::string &file_path);
void symbols_hide_file(const std::string &file_path);

const std::set<std::string> &symbols_get_loaded_files();

bool symbols_file_all_are_visible();
bool symbols_file_any_is_visible();
bool symbols_file_is_visible(const std::string &file_path);

// Bank parameter is only meaninful for addresses >= $A000.
// Addresses < $A000 will force bank to 0.
const symbol_list_type &symbols_find(uint32_t address, symbol_bank_type bank = 0);

void symbols_for_each(std::function<void(uint16_t, symbol_bank_type, const std::string &)> fn);
