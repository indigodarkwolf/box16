#include "parser.h"

#include <sstream>
#include <stack>

#include "boxmon.h"
#include "expression.h"
#include "memory.h"

namespace boxmon
{
	class boxmon_expression_internal : public expression
	{
	public:
		boxmon_expression_internal(const std::string &expr_string, const boxmon::expression_base *expr_ptr)
		    : m_string(expr_string),
		      m_expression(expr_ptr)
		{
		}

		virtual ~boxmon_expression_internal() override
		{
			delete m_expression;
		}

		virtual const std::string &get_string() const
		{
			return m_string;
		}

		virtual int evaluate() const
		{
			return m_expression->evaluate();
		}

	private:
		const std::string                m_string;
		const boxmon::expression_base *m_expression;
	};

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

		if (*look == '.') {
			rs << *look;
			++look;
		}

		while (isalnum(*look) || *look == '_') {
			rs << *look;
			++look;
		}

		if (look == input) {
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

		if (result >= static_cast<int>(options.size())) {
			return false;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_label(std::string &result, char const *&input)
	{
		char const *look = input;

		std::stringstream rs;

		if (*look == '.') {
			rs << *look;
			++look;
		}

		while (isalnum(*look) || *look == '_' || *look == '@') {
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

	bool parser::parse_address(address_type &result, char const *&input)
	{
		char const *look = input;

		uint32_t addr = 0;
		if (parse_number(addr, look) == false) {
			return false;
		}
		if (addr > 0xffff) {
			result = { static_cast<uint16_t>(addr & 0xffff), static_cast<uint8_t>(addr >> 16) };
		} else {
			result = { addr, m_default_bank };
		}
		input = look;

		skip_whitespace(input);
		return true;
	}

	bool parser::parse_address_range(address_type &result0, address_type &result1, char const *&input)
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

	bool parser::parse_expression(const expression *&expression, char const *&input, expression_parse_flags flags)
	{
		std::stack<expression_type>     operator_stack;
		std::stack<expression_base *> expression_stack;

		char const *look = input;

		auto clear_stacks = [&]() {
			operator_stack = std::stack<expression_type>();
			while (!expression_stack.empty()) {
				delete expression_stack.top();
				expression_stack.pop();
			}
		};

		auto should_pop_op = [&](const expression_type next_op) -> bool {
			if (operator_stack.empty()) {
				return false;
			}

			const expression_type top_op = operator_stack.top();

			if (top_op == expression_type::parenthesis) {
				return false;
			}

			const auto &top_info  = get_expression_type_info(top_op);
			const auto &next_info = get_expression_type_info(next_op);

			if (top_info.precedence < next_info.precedence) {
				return true;
			}

			if (top_info.precedence == next_info.precedence) {
				return next_info.left_associative;
			}

			return false;
		};

		auto pop_op = [&]() -> bool {
			if (operator_stack.empty()) {
				if ((flags & expression_parse_flags_suppress_errors) == 0) {
					boxmon_error_printf("Expression parse failed (internal error, popping op with no more ops left) at: \"%s\"\n", look);
				}
				return false;
			}
			const expression_type op = operator_stack.top();
			operator_stack.pop();

			if (expression_stack.empty()) {
				if ((flags & expression_parse_flags_suppress_errors) == 0) {
					boxmon_error_printf("Expression parse failed (operand expected) at: \"%s\"\n", look);
				}
				return false;
			}
			const expression_base *rhs = expression_stack.top();
			expression_stack.pop();

			switch (op) {
				case expression_type::dereference: [[fallthrough]];
				case expression_type::negate: [[fallthrough]];
				case expression_type::bit_not: [[fallthrough]];
				case expression_type::logical_not:
					expression_stack.push(new unary_expression(op, rhs));
					break;
				default:
					if (expression_stack.empty()) {
						if ((flags & expression_parse_flags_suppress_errors) == 0) {
							boxmon_error_printf("Expression parse failed (operand expected) at: \"%s\"\n", look);
						}
						return false;
					} else {
						const expression_base *lhs = expression_stack.top();
						expression_stack.pop();
						expression_stack.push(new binary_expression(op, lhs, rhs));
					}
					break;
			}

			return true;
		};

		auto read_token = [&]() -> expression_type {
			switch (*look) {
				case '@': {
					++look;
					return expression_type::dereference;
				} break;

				case '~': {
					++look;
					return expression_type::bit_not;
				} break;

				case '(': {
					++look;
					return expression_type::parenthesis;
				} break;

				case ')': {
					++look;
					return expression_type::parenthesis_end;
				} break;

				case '^': {
					++look;
					if (*look == '^') {
						++look;
						return expression_type::pow;
					} else {
						return expression_type::bit_xor;
					}
				} break;

				case '%': {
					++look;
					return expression_type::modulo;
				} break;

				case '*': {
					++look;
					return expression_type::multiply;
				} break;

				case '/': {
					++look;
					return expression_type::divide;
				} break;

				case '+': {
					++look;
					return expression_type::addition;
				} break;

				case '-': {
					++look;
					return expression_type::subtraction;
				} break;

				case '&': {
					++look;
					if (*look == '&') {
						++look;
						return expression_type::logical_and;
					} else {
						return expression_type::bit_and;
					}
				} break;
				case '|': {
					++look;
					if (*look == '|') {
						++look;
						return expression_type::logical_or;
					} else {
						return expression_type::bit_or;
					}
				} break;
				case '=': {
					++look;
					if (*look == '=') {
						++look;
						return expression_type::equal;
					} else {
						// return expression_type::assign;
						return expression_type::invalid;
					}
				} break;

				case '!': {
					++look;
					if (*look == '=') {
						++look;
						return expression_type::not_equal;
					} else {
						return expression_type::logical_not;
					}
				} break;

				case '<': {
					++look;
					if (*look == '=') {
						++look;
						return expression_type::lte;
					} else {
						return expression_type::lt;
					}
				} break;

				case '>': {
					++look;
					if (*look == '=') {
						++look;
						return expression_type::gte;
					} else {
						return expression_type::gt;
					}
				} break;

				default: {
					expression_base *subexpression = nullptr;
					if (int num; parse_number(num, look)) {
						expression_stack.push(new value_expression(num));
						return expression_stack.top()->get_type();
					} else if (std::string symbol; parse_word(symbol, look)) {
						expression_stack.push(new symbol_expression(symbol));
						return expression_stack.top()->get_type();
					} else {
						return expression_type::invalid;
					}
				} break;
			}
			return expression_type::invalid;
		};

		expression_type last_parse_type = expression_type::invalid;
		while (*look) {
			expression_type parse_type = read_token();
			switch (parse_type) {
				case expression_type::invalid:
					if ((flags & expression_parse_flags_suppress_errors) == 0) {
						boxmon_error_printf("Expression parse failed (invalid token) at: \"%s\"\n", look);
					}
					clear_stacks();
					return false;
				case expression_type::value:
					[[fallthrough]];
				case expression_type::symbol:
					break;
				case expression_type::parenthesis:
					operator_stack.push(parse_type);
					break;
				case expression_type::parenthesis_end:
					while (!operator_stack.empty() && operator_stack.top() != expression_type::parenthesis) {
						if (!pop_op()) {
							clear_stacks();
							return false;
						}
					}
					if (operator_stack.empty()) {
						if ((flags & expression_parse_flags_suppress_errors) == 0) {
							boxmon_error_printf("Expression parse failed (mismatched parenthesis) at: \"%s\"\n", look);
						}
					} else {
						operator_stack.pop();
					}
					break;
				case expression_type::subtraction:
					if (expression_stack.empty() || last_parse_type == expression_type::subtraction) {
						operator_stack.push(expression_type::negate);
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
			if ((flags & expression_parse_flags_suppress_errors) == 0) {
				boxmon_error_printf("Expression parse failed (internal error, no final expression) at: \"%s\"\n", look);
			}
			return false;
		}

		const auto *expression_result = expression_stack.top();
		expression_stack.pop();

		if (!expression_stack.empty()) {
			if ((flags & expression_parse_flags_suppress_errors) == 0) {
				boxmon_error_printf("Expression parse failed (too many expressions) at: \"%s\"\n", look);
			}
			return false;
		}

		if ((flags & expression_parse_flags_must_consume_all) != 0 && *look != '\0') {
			if ((flags & expression_parse_flags_suppress_errors) == 0) {
				boxmon_error_printf("Expression parse failed (invalid token) at: \"%s\"\n", look);
			}
			return false;
		}

		expression = new boxmon_expression_internal(std::string(input, look), expression_result);

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