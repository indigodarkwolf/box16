#include "parser.h"

#include <sstream>
#include <stack>

#include "memory.h"
#include "symbols.h"

#include "stdio.h"

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

	bool parser::parse_expression(const boxmon_expression *&expression, char const *&input)
	{
		std::stack<boxmon_expression::expression_type> operator_stack;
		std::stack<boxmon_expression *>                expression_stack;

		char const *look = input;

		auto clear_stacks = [&]() {
			operator_stack = std::stack<boxmon_expression::expression_type>();
			while (!expression_stack.empty()) {
				delete expression_stack.top();
				expression_stack.pop();
			}
		};

		auto should_pop_op = [&](const boxmon_expression::expression_type next_op) -> bool {
			if (operator_stack.empty()) {
				return false;
			}

			const boxmon_expression::expression_type top_op = operator_stack.top();

			if (top_op == boxmon_expression::expression_type::parenthesis) {
				return false;
			}

			if (boxmon_expression::expression_type_precedence[static_cast<int>(top_op)] < boxmon_expression::expression_type_precedence[static_cast<int>(next_op)]) {
				return true;
			}

			if (boxmon_expression::expression_type_precedence[static_cast<int>(top_op)] == boxmon_expression::expression_type_precedence[static_cast<int>(next_op)]) {
				return boxmon_expression::expression_type_left_associative[static_cast<int>(next_op)];
			}

			return false;
		};

		auto pop_op = [&]() -> bool {
			if (operator_stack.empty()) {
				printf("Expression parse failed (internal error, popping op with no more ops left) at: \"%s\"\n", look);
				return false;
			}
			const boxmon_expression::expression_type op = operator_stack.top();
			operator_stack.pop();

			if (expression_stack.empty()) {
				printf("Expression parse failed (operand expected) at: \"%s\"\n", look);
				return false;
			}
			const boxmon_expression *rhs = expression_stack.top();
			expression_stack.pop();

			switch (op) {
				case boxmon_expression::expression_type::dereference: [[fallthrough]];
				case boxmon_expression::expression_type::negate: [[fallthrough]];
				case boxmon_expression::expression_type::bit_not: [[fallthrough]];
				case boxmon_expression::expression_type::logical_not:
					expression_stack.push(new boxmon_unary_expression(op, rhs));
					break;
				default:
					if (expression_stack.empty()) {
						printf("Expression parse failed (operand expected) at: \"%s\"\n", look);
						return false;
					} else {
						const boxmon_expression *lhs = expression_stack.top();
						expression_stack.pop();
						expression_stack.push(new boxmon_binary_expression(op, lhs, rhs));
					}
					break;
			}

			return true;
		};

		auto read_token = [&]() -> boxmon_expression::expression_type {
			switch (*look) {
				case '@': {
					++look;
					return boxmon_expression::expression_type::dereference;
				} break;

				case '~': {
					++look;
					return boxmon_expression::expression_type::bit_not;
				} break;

				case '(': {
					++look;
					return boxmon_expression::expression_type::parenthesis;
				} break;

				case ')': {
					++look;
					return boxmon_expression::expression_type::parenthesis_end;
				} break;

				case '^': {
					++look;
					if (*look == '^') {
						++look;
						return boxmon_expression::expression_type::pow;
					} else {
						return boxmon_expression::expression_type::bit_xor;
					}
				} break;

				case '%': {
					++look;
					return boxmon_expression::expression_type::modulo;
				} break;

				case '*': {
					++look;
					return boxmon_expression::expression_type::multiply;
				} break;

				case '/': {
					++look;
					return boxmon_expression::expression_type::divide;
				} break;

				case '+': {
					++look;
					return boxmon_expression::expression_type::addition;
				} break;

				case '-': {
					++look;
					return boxmon_expression::expression_type::subtraction;
				} break;

				case '&': {
					++look;
					if (*look == '&') {
						++look;
						return boxmon_expression::expression_type::logical_and;
					} else {
						return boxmon_expression::expression_type::bit_and;
					}
				} break;
				case '|': {
					++look;
					if (*look == '|') {
						++look;
						return boxmon_expression::expression_type::logical_or;
					} else {
						return boxmon_expression::expression_type::bit_or;
					}
				} break;
				case '=': {
					++look;
					if (*look == '=') {
						++look;
						return boxmon_expression::expression_type::equal;
					} else {
						// return boxmon_expression::expression_type::assign;
						return boxmon_expression::expression_type::invalid;
					}
				} break;

				case '!': {
					++look;
					if (*look == '=') {
						++look;
						return boxmon_expression::expression_type::not_equal;
					} else {
						return boxmon_expression::expression_type::logical_not;
					}
				} break;

				case '<': {
					++look;
					if (*look == '=') {
						++look;
						return boxmon_expression::expression_type::lte;
					} else {
						return boxmon_expression::expression_type::lt;
					}
				} break;

				case '>': {
					++look;
					if (*look == '=') {
						++look;
						return boxmon_expression::expression_type::gte;
					} else {
						return boxmon_expression::expression_type::gt;
					}
				} break;

				default: {
					boxmon_expression *subexpression = nullptr;
					if (int num; parse_number(num, look)) {
						expression_stack.push(new boxmon_value_expression(num));
						return expression_stack.top()->get_type();
					} else if (std::string symbol; parse_word(symbol, look)) {
						expression_stack.push(new boxmon_symbol_expression(symbol));
						return expression_stack.top()->get_type();
					} else {
						return boxmon_expression::expression_type::invalid;
					}
				} break;
			}
			return boxmon_expression::expression_type::invalid;
		};

		boxmon_expression::expression_type last_parse_type = boxmon_expression::expression_type::invalid;
		while (*look) {
			boxmon_expression::expression_type parse_type = read_token();
			switch (parse_type) {
				case boxmon_expression::expression_type::invalid:
					printf("Expression parse failed (invalid token) at: \"%s\"\n", look);
					clear_stacks();
					return false;
				case boxmon_expression::expression_type::value:
					[[fallthrough]];
				case boxmon_expression::expression_type::symbol:
					break;
				case boxmon_expression::expression_type::parenthesis:
					operator_stack.push(parse_type);
					break;
				case boxmon_expression::expression_type::parenthesis_end:
					while (!operator_stack.empty() && operator_stack.top() != boxmon_expression::expression_type::parenthesis) {
						if (!pop_op()) {
							clear_stacks();
							return false;
						}
					}
					if (operator_stack.empty()) {
						printf("Expression parse failed (mismatched parenthesis) at: \"%s\"\n", look);
					} else {
						operator_stack.pop();
					}
					break;
				case boxmon_expression::expression_type::subtraction:
					if (expression_stack.empty() || last_parse_type == boxmon_expression::expression_type::subtraction) {
						operator_stack.push(boxmon_expression::expression_type::negate);
					}
					[[fallthrough]];
				default:
					// TODO: operator precedence
					while (should_pop_op(parse_type)) {
						if (!pop_op()) {
							clear_stacks();
							return false;
						}
					}
					operator_stack.push(parse_type);
					break;
			}
			skip_whitespace(look);
		}

		while (!operator_stack.empty()) {
			if (!pop_op()) {
				clear_stacks();
				return false;
			}
		}

		if (expression_stack.empty()) {
			printf("Expression parse failed (internal error, no final expression) at: \"%s\"\n", look);
			return false;
		}

		expression = expression_stack.top();
		expression_stack.pop();

		if (!expression_stack.empty()) {
			printf("Expression parse failed (too many expressions) at: \"%s\"\n", look);
			return false;
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