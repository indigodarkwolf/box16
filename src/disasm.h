#pragma once

#include <string>

const std::string &disasm_get_label(const uint16_t address, const uint8_t bank = 0);
std::string        disasm_code(const uint16_t pc, const uint8_t bank);
bool               disasm_is_branch(const uint8_t opcode);