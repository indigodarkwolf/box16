#include "disasm.h"

#include "cpu/mnemonics.h"
#include "memory.h"
#include "symbols.h"

static char const *disasm_label(uint16_t target, const char *hex_format)
{
	const char *symbol = disasm_get_label(target);

	static char inner[256];
	if (symbol != nullptr) {
		snprintf(inner, 256, "%s", symbol);
	} else {
		snprintf(inner, 256, hex_format, target);
	}
	inner[255] = '\0';
	return inner;
}

static char const *disasm_label_wrap(uint16_t target, const char *hex_format, const char *wrapper_format)
{
	const char *symbol = disasm_get_label(target);

	char inner[256];
	if (symbol != nullptr) {
		snprintf(inner, 256, "%s", symbol);
	} else {
		snprintf(inner, 256, hex_format, target);
	}
	inner[255] = '\0';

	static char wrapped[512];
	snprintf(wrapped, 512, wrapper_format, inner);
	wrapped[511] = '\0';
	return wrapped;
}

static void sncatf(char *&buffer_start, size_t &size_remaining, char const *fmt, ...)
{
	va_list arglist;
	va_start(arglist, fmt);
	size_t printed = vsnprintf(buffer_start, size_remaining, fmt, arglist);
	va_end(arglist);
	buffer_start += printed;
	size_remaining -= printed;
}

char const *disasm_get_label(uint16_t address)
{
	static char label[256];

	const symbol_list_type &symbols = symbols_find(address);
	if (symbols.size() > 0) {
		strncpy(label, symbols.front().c_str(), 256);
		label[255] = '\0';
		return label;
	}

	for (uint16_t i = 1; i < 3; ++i) {
		const symbol_list_type &symbols = symbols_find(address - i);
		if (symbols.size() > 0) {
			snprintf(label, 256, "%s+%d", symbols.front().c_str(), i);
			label[255] = '\0';
			return label;
		}
	}

	return nullptr;
}

size_t disasm_code(char *buffer, size_t buffer_size, uint16_t pc, uint8_t bank)
{
	uint8_t     opcode   = debug_read6502(pc, bank);
	char const *mnemonic = mnemonics[opcode];
	op_mode     mode     = mnemonics_mode[opcode];

	char *buffer_beg = buffer;

	switch (mode) {
		case op_mode::MODE_ZPREL: {
			uint8_t  zp     = debug_read6502(pc + 1, bank);
			uint16_t target = pc + 3 + (int8_t)debug_read6502(pc + 2, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label(zp, "$%02X"));
			sncatf(buffer, buffer_size, ", ");
			sncatf(buffer, buffer_size, "%s", disasm_label(target, "$%04X"));
		} break;

		case op_mode::MODE_IMP:
			sncatf(buffer, buffer_size, "%s", mnemonic);
			break;

		case op_mode::MODE_IMM: {
			uint16_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "#$%02X", value);
		} break;

		case op_mode::MODE_ZP: {
			uint8_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "$%02X", value);
		} break;

		case op_mode::MODE_REL: {
			uint16_t target = pc + 2 + (int8_t)debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label(target, "$%04X"));
		} break;

		case op_mode::MODE_ZPX: {
			uint8_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(value, "$%02X", "%s,x"));
		} break;

		case op_mode::MODE_ZPY: {
			uint8_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(value, "$%02X", "%s,y"));
		} break;

		case op_mode::MODE_ABSO: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label(target, "$%04X"));
		} break;

		case op_mode::MODE_ABSX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%04X", "%s,x"));
		} break;

		case op_mode::MODE_ABSY: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%04X", "%s,y"));
		} break;

		case op_mode::MODE_AINX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%04X", "(%s,x)"));
		} break;

		case op_mode::MODE_INDY: {
			uint8_t target = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%02X", "(%s),y"));
		} break;

		case op_mode::MODE_INDX: {
			uint8_t target = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%02X", "(%s,x)"));
		} break;

		case op_mode::MODE_IND: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%04X", "(%s)"));
		} break;

		case op_mode::MODE_IND0: {
			uint8_t target = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, "$%02X", "(%s)"));
		} break;

		case op_mode::MODE_A:
			sncatf(buffer, buffer_size, "%s a", mnemonic);
			break;
	}
	return (size_t)(buffer - buffer_beg);
}
