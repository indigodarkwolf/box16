#pragma once

char const *disasm_get_label(uint16_t address);
size_t      disasm_code(char *buffer, size_t buffer_size, uint16_t pc, uint8_t bank);

