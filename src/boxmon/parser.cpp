#include "parser.h"

#include <sstream>

#include "memory.h"

namespace boxmon
{
	//
	// Expression
	//

	boxmon_expression::boxmon_expression(expression_type type)
		: m_type(type)
	{
	}

	boxmon_expression::~boxmon_expression()
	{
	}

	boxmon_expression::expression_type boxmon_expression::get_type() const
	{
		return m_type;
	}

	boxmon_value_expression::boxmon_value_expression(const int &value)
		: boxmon_expression(expression_type::value),
		    m_value(value)
	{
	}

	boxmon_value_expression::~boxmon_value_expression()
	{
	}

	int boxmon_value_expression::evaluate() const
	{
		return m_value;
	}


	boxmon_symbol_expression::boxmon_symbol_expression(const std::string &symbol)
		: boxmon_expression(expression_type::symbol),
		    m_symbol(symbol)
	{
	}

	boxmon_symbol_expression::~boxmon_symbol_expression()
	{
	}

	int boxmon_symbol_expression::evaluate() const
	{
		return 0;
	}

	boxmon_unary_expression::boxmon_unary_expression(expression_type type, const boxmon_expression *param)
		: boxmon_expression(type),
		    m_param(param)
	{
	}

	boxmon_unary_expression::~boxmon_unary_expression()
	{
		delete m_param;
	}

	int boxmon_unary_expression::evaluate() const
	{
		switch (get_type()) {
			case expression_type::dereference: {
				const int address = m_param->evaluate();
				return debug_read6502(address & 0xffff, (address >> 16) & 0xff);
			}
			case expression_type::negate:
				return -(m_param->evaluate());
			case expression_type::bit_not:
				return ~(m_param->evaluate());
			case expression_type::logical_not:
				return m_param->evaluate() == 0 ? 1 : 0;
			default:
				break;
		}
		return 0;
	}

	boxmon_binary_expression::boxmon_binary_expression(expression_type type, const boxmon_expression *lhs, const boxmon_expression *rhs)
		: boxmon_expression(type),
		    m_lhs(lhs),
		    m_rhs(rhs)
	{
	}

	boxmon_binary_expression::~boxmon_binary_expression()
	{
		delete m_lhs;
		delete m_rhs;
	}

	int boxmon_binary_expression::evaluate() const
	{
		switch (get_type()) {
			case expression_type::addition:
				return m_lhs->evaluate() + m_rhs->evaluate();
			case expression_type::subtraction:
				return m_lhs->evaluate() - m_rhs->evaluate();
			case expression_type::multiply:
				return m_lhs->evaluate() * m_rhs->evaluate();
			case expression_type::divide:
				return m_lhs->evaluate() / m_rhs->evaluate();
			case expression_type::modulo:
				return m_lhs->evaluate() % m_rhs->evaluate();
			case expression_type::pow: {
				const int lhs    = m_lhs->evaluate();
				const int rhs    = m_rhs->evaluate();
				int       result = 1;
				for (int i = 1; i < rhs; ++i) {
					result *= lhs;
				}
				return result;
			}
			case expression_type::bit_and:
				return m_lhs->evaluate() & m_rhs->evaluate();
			case expression_type::bit_or:
				return m_lhs->evaluate() | m_rhs->evaluate();
			case expression_type::bit_xor:
				return m_lhs->evaluate() ^ m_rhs->evaluate();
			case expression_type::left_shift:
				return m_lhs->evaluate() << m_rhs->evaluate();
			case expression_type::right_shift:
				return m_lhs->evaluate() >> m_rhs->evaluate();
			case expression_type::equal:
				return m_lhs->evaluate() == m_rhs->evaluate();
			case expression_type::not_equal:
				return m_lhs->evaluate() != m_rhs->evaluate();
			case expression_type::lt:
				return m_lhs->evaluate() < m_rhs->evaluate();
			case expression_type::gt:
				return m_lhs->evaluate() > m_rhs->evaluate();
			case expression_type::lte:
				return m_lhs->evaluate() <= m_rhs->evaluate();
			case expression_type::gte:
				return m_lhs->evaluate() >= m_rhs->evaluate();
			case expression_type::logical_and:
				return (m_lhs->evaluate() != 0) && (m_rhs->evaluate() != 0);
			case expression_type::logical_or:
				return (m_lhs->evaluate() != 0) || (m_rhs->evaluate() != 0);
			default:
				break;
		}
		return 0;
	}

	//
	// Parser
	//

	void parser::skip_whitespace(char const *&input)
	{
		while (*input != '\0' && !isgraph(*input)) {
			++input;
		}
	}

	bool parser::parse_separator(char const *&input)
	{
		if (*input != ',') {
			return false;
		}
		++input;

		skip_whitespace(input);
		return true;
	}

	// A radix type is one of the characters in `bdho` followed by a space.
	bool parser::parse_radix_type(radix_type &radix, char const *&input)
	{
		int found = 0;

		switch (tolower(*input)) {
			case 'h':
				radix = radix_type::hex;
				found = 1;
				break;
			case 'd':
				radix = radix_type::dec;
				found = 1;
				break;
			case 'o':
				radix = radix_type::oct;
				found = 1;
				break;
			case 'b':
				radix = radix_type::bin;
				found = 1;
				break;
		}

		if (!found) {
			return false;
		}

		if (*(input + found) != ' ') {
			return false;
		}

		input += found;

		skip_whitespace(input);
		return true;
	}

	// A radix prefix is one of the characters in `$%ho`, or the string "0x", and must not be followed by a space.
	bool parser::parse_radix_prefix(radix_type &radix, char const *&input)
	{
		int found = 0;

		switch (tolower(*input)) {
			case '$':
				[[fallthrough]];
			case 'h':
				radix = radix_type::hex;
				found = 1;
				break;
			case '0': {
				const char *look = input + 1;
				if (tolower(*look) == 'x') {
					radix = radix_type::hex;
					found = 2;
				}
			} break;
			case 'o':
				radix = radix_type::oct;
				found = 1;
				break;
			case '%':
				radix = radix_type::bin;
				found = 1;
				break;
		}

		if (!found) {
			return false;
		}

		if (*(input + found) == ' ') {
			return false;
		}

		input += found;
		return true;
	}

	bool parser::parse_device(device_type &result, char const *&input)
	{
		char const *look = input;
		switch (*look) {
			case 'c':
				result = device_type::device_cpu;
				++look;
				break;
			case '8':
				result = device_type::device_8;
				++look;
				break;
			case '9':
				result = device_type::device_9;
				++look;
				break;
			case '1':
				++look;
				switch (*look) {
					case '0':
						result = device_type::device_10;
						++look;
						break;
					case '1':
						result = device_type::device_11;
						++look;
						break;
				}
			default:
				return false;
		}

		if (*input == ':') {
			++look;
			input = look;

			skip_whitespace(input);
			return true;
		}

		return false;
	}

	bool parser::parse_word(std::string &result, char const *&input)
	{
		char const *look = input;

		std::stringstream rs;

		while (isalnum(*look) || *look == '_') {
			rs << *look;
			++look;
		}
		if (*look != '\0' && !isprint(*look)) {
			return false;
		}

		result = rs.str();
		input  = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_string(std::string &result, char const *&input)
	{
		if (*input != '"') {
			return parse_word(result, input);
		}

		char const *look = input + 1;

		std::stringstream rs;

		while (isprint(*look) && *look != '"') {
			rs << *look;
			++look;
		}
		if (*look != '"') {
			return false;
		}
		++look;

		result = rs.str();
		input  = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_option(int &result, std::initializer_list<char const *> options, char const *&input)
	{
		char const *look = input;

		std::string token;
		if (parse_string(token, look) == false) {
			return false;
		}

		result = 0;
		for (auto opt : options) {
			if (token == opt) {
				break;
			}
			++result;
		}

		if (result >= options.size()) {
			return false;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_label(std::string &result, char const *&input)
	{
		char const *look = input;

		if (*look != '.') {
			return false;
		}

		std::stringstream rs;
		rs << *look;
		++look;

		while (isalnum(*look) || *look == '_' || *look == '@') {
			rs << *look;
			++look;
		}

		if (!isprint(*look)) {
			return false;
		}

		result = rs.str();
		input  = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_comment(std::string &result, char const *&input)
	{
		char const *look = input;

		if (*look != ';') {
			return false;
		}

		std::stringstream rs;
		++look;

		while (isprint(*look)) {
			rs << *look;
			++look;
		}

		result = rs.str();
		input  = look;

		return true;
	}

	bool parser::parse_address(breakpoint_type &result, char const *&input)
	{
		char const *look = input;

		uint32_t addr = 0;
		if (parse_number(addr, look) == false) {
			return false;
		}
		if (addr > 0xffff) {
			result = { addr & 0xffff, addr >> 16 };
		} else {
			result = { addr, m_default_bank };
		}
		input = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_address_range(breakpoint_type &result0, breakpoint_type &result1, char const *&input)
	{
		char const *look = input;

		if (parse_address(result0, look) == false) {
			return false;
		}

		const bool explicit_range = (parse_separator(look) == true);
		if (parse_address(result1, look) == false) {
			if (explicit_range) {
				return false;
			}
			result1 = result0;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_bankname(uint8_t &bank, char const *&input)
	{
		char const *look = input;

		if (strncmp(look, "cpu", 3) == 0) {
			bank = memory_get_ram_bank();
			look += 3;
		} else {
			if (parse_number(bank, look) == false) {
				return false;
			}
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_expression(boxmon_expression *&expression, char const *&input)
	{
		switch (*input) {
			case '@': {
				char const        *look  = input + 1;
				boxmon_expression *child = nullptr;
				if (!parse_expression(child, look)) {
					return false;
				}
				expression = new boxmon_unary_expression(boxmon_expression::expression_type::dereference, child);
			} break;
		}

		return true;
	}

	void parser::set_default_radix(radix_type radix)
	{
		m_default_radix = radix;
	}

	void parser::set_default_bank(uint8_t bank)
	{
		m_default_bank = bank;
	}
} // namespace boxmon