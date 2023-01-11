#pragma once

#include <string>

#include "debugger.h"

namespace boxmon
{
	enum class device_type {
		device_cpu,
		device_8,
		device_9,
		device_10,
		device_11
	};

	enum class radix_type {
		hex,
		dec,
		oct,
		bin
	};

	//
	// Expression
	//

	class boxmon_expression
	{
	public:
		enum class expression_type {
			value,  // 1234
			symbol, // .@local
			dereference,

			negate,
			addition,
			subtraction,
			multiply,
			divide,
			modulo,
			pow,

			bit_not,
			bit_and,
			bit_or,
			bit_xor,
			left_shift,
			right_shift,

			equal,
			not_equal,
			lt,
			gt,
			lte,
			gte,

			logical_and,
			logical_or,
			logical_not,

			parenthesis
		};

		boxmon_expression(expression_type type);
		virtual ~boxmon_expression();
		virtual int     evaluate() const = 0;
		expression_type get_type() const;

	private:
		expression_type m_type;
	};

	class boxmon_value_expression : public boxmon_expression
	{
	public:
		boxmon_value_expression(const int &value);
		virtual ~boxmon_value_expression() override final;
		virtual int evaluate() const override final;

	private:
		int m_value;
	};

	class boxmon_symbol_expression : public boxmon_expression
	{
	public:
		boxmon_symbol_expression(const std::string &symbol);
		virtual ~boxmon_symbol_expression() override final;
		virtual int evaluate() const override final;

	private:
		std::string m_symbol;
	};

	class boxmon_unary_expression : public boxmon_expression
	{
	public:
		boxmon_unary_expression(expression_type type, const boxmon_expression *param);
		virtual ~boxmon_unary_expression() override final;
		virtual int evaluate() const override final;

	private:
		const boxmon_expression *m_param;
	};

	class boxmon_binary_expression : public boxmon_expression
	{
	public:
		boxmon_binary_expression(expression_type type, const boxmon_expression *lhs, const boxmon_expression *rhs);
		virtual ~boxmon_binary_expression() override final;
		virtual int evaluate() const override final;

	private:
		const boxmon_expression *m_lhs;
		const boxmon_expression *m_rhs;
	};

	//
	// Parser
	//

	class parser
	{
	public:
		void skip_whitespace(char const *&input);
		bool parse_separator(char const *&input);
		bool parse_radix_type(radix_type &radix, char const *&input);
		bool parse_radix_prefix(radix_type &radix, char const *&input);
		bool parse_device(device_type &result, char const *&input);
		bool parse_word(std::string &result, char const *&input);
		bool parse_string(std::string &result, char const *&input);
		bool parse_option(int &result, std::initializer_list<char const *> options, char const *&input);
		bool parse_label(std::string &result, char const *&input);
		bool parse_comment(std::string &result, char const *&input);

		template <typename T>
		bool parse_hex_number(T &result, char const *&input);
		template <typename T>
		bool parse_dec_number(T &result, char const *&input);
		template <typename T>
		bool parse_oct_number(T &result, char const *&input);
		template <typename T>
		bool parse_bin_number(T &result, char const *&input);
		template <typename T>
		bool parse_number(T &result, char const *&input);

		bool parse_address(breakpoint_type &result, char const *&input);
		bool parse_address_range(breakpoint_type &result0, breakpoint_type &result1, char const *&input);
		bool parse_bankname(uint8_t &bank, char const *&input);
		bool parse_expression(boxmon_expression *&expression, char const *&input);

		void set_default_radix(radix_type radix);
		void set_default_bank(uint8_t bank);

	private:
		radix_type m_default_radix = radix_type::hex;
		uint8_t    m_default_bank  = 0;
	};
} // namespace boxmon

#include "parser.inl"
