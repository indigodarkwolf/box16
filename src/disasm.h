#pragma once

char const *disasm_get_label(uint16_t address, uint8_t bank = 0);
size_t      disasm_code(char *buffer, size_t buffer_size, uint16_t pc, uint8_t bank);
bool        disasm_is_branch(uint8_t opcode);