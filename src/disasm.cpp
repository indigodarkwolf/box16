#include "disasm.h"

#include <string>

#include "cpu/mnemonics.h"
#include "memory.h"
#include "symbols.h"

static std::string disasm_label(const uint16_t target, const uint8_t bank, const std::string &hex_format)
{
	const std::string &symbol = disasm_get_label(target, bank);

	if (symbol.empty()) {
		return symbol;
	} else {
		return fmt::format(fmt::runtime(hex_format), target);
	}
}

static std::string disasm_label_wrap(const uint16_t target, const uint8_t bank, const std::string &hex_format, const std::string &wrapper_format)
{
	const std::string &symbol = disasm_get_label(target, bank);

	const std::string inner = (symbol.empty() ? fmt::format(fmt::runtime(hex_format), target) : symbol);
	return fmt::format(fmt::runtime(wrapper_format), inner);
}

const std::string &disasm_get_label(const uint16_t address, const uint8_t bank)
{
	const symbol_list_type &symbols = symbols_find(address, bank);
	if (symbols.size() > 0) {
		return symbols.front();
	}

	for (uint16_t i = 1; i < 3; ++i) {
		const symbol_list_type &symbols = symbols_find(address - i, bank);
		if (symbols.size() > 0) {
			return symbols.front();
		}
	}

	static std::string empty_string;
	return empty_string;
}

std::string disasm_code(const uint16_t pc, const uint8_t bank)
{
	const uint8_t     opcode   = debug_read6502(pc, bank);
	const char *const mnemonic = mnemonics[opcode];
	const op_mode     mode     = mnemonics_mode[opcode];

	switch (mode) {
		case op_mode::MODE_ZPREL: {
			const uint8_t  zp     = debug_read6502(pc + 1, bank);
			const uint16_t target = pc + 3 + (int8_t)debug_read6502(pc + 2, bank);
			return fmt::format("{} {}, {}", mnemonic, disasm_label(zp, bank, "${:02X}"), disasm_label(target, bank, "${:04X}"));
		} break;

		case op_mode::MODE_IMP:
			return mnemonic;
			break;

		case op_mode::MODE_IMM: {
			const uint16_t value = debug_read6502(pc + 1, bank);
			return fmt::format("{} #${:02X}", mnemonic, value);
		} break;

		case op_mode::MODE_ZP: {
			const uint8_t value = debug_read6502(pc + 1, bank);
			return fmt::format("{} ${:02X}", mnemonic, value);
		} break;

		case op_mode::MODE_REL: {
			const uint16_t target = pc + 2 + (int8_t)debug_read6502(pc + 1, bank);
			return fmt::format("{} {}", mnemonic, disasm_label(target, bank, "${:04X}"));
		} break;

		case op_mode::MODE_ZPX: {
			const uint8_t value = debug_read6502(pc + 1, bank);
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(value, bank, "${:02X}", "{},x"));
		} break;

		case op_mode::MODE_ZPY: {
			const uint8_t value = debug_read6502(pc + 1, bank);
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(value, bank, "${:02X}", "{},y"));
		} break;

		case op_mode::MODE_ABSO: {
			const uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;
			return fmt::format("{} {}", mnemonic, disasm_label(target, bank, "${:04X}"));
		} break;

		case op_mode::MODE_ABSX: {
			const uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:04X}", "{},x"));
		} break;

		case op_mode::MODE_ABSY: {
			const uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:04X}", "{},y"));
		} break;

		case op_mode::MODE_AINX: {
			const uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:04X}", "({},x)"));
		} break;

		case op_mode::MODE_INDY: {
			const uint8_t target = debug_read6502(pc + 1, bank);
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:02X}", "({}),y"));
		} break;

		case op_mode::MODE_INDX: {
			const uint8_t target = debug_read6502(pc + 1, bank);
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:02X}", "({},x)"));
		} break;

		case op_mode::MODE_IND: {
			const uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:04X}", "({})"));
		} break;

		case op_mode::MODE_IND0: {
			const uint8_t target = debug_read6502(pc + 1, bank);
			return fmt::format("{} {}", mnemonic, disasm_label_wrap(target, bank, "${:02X}", "({})"));
		} break;

		case op_mode::MODE_A:
			return fmt::format("{} a", mnemonic);
			break;

		default:
			return std::string();
	}
}

bool disasm_is_branch(const uint8_t opcode)
{
	//		Test bbr and bbs, the "zero-page, relative" ops. These all count as branch ops.
	//		$0F,$1F,$2F,$3F,$4F,$5F,$6F,$7F,$8F,$9F,$AF,$BF,$CF,$DF,$EF,$FF
	//
	const bool is_zprel = (opcode & 0x0F) == 0x0F;

	const bool is_jump = (*reinterpret_cast<const int *>(mnemonics[opcode]) == 0x00706d6a);

	//		Test for branches. These are BRA ($80) and
	//		$10,$30,$50,$70,$90,$B0,$D0,$F0.
	//		All 'jmp' ops count as well.
	//
	const bool is_branch = is_zprel || is_jump || ((opcode == 0x80) || ((opcode & 0x1F) == 0x10) || (opcode == 0x20));

	return is_branch;
}
