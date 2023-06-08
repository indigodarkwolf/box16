#pragma once

#include <string>

namespace boxmon
{
	//
	// Expression
	//

	class boxmon_expression
	{
	public:
		enum class expression_type {
			invalid = 0,

			parenthesis,
			parenthesis_end, // Special-case: This is only used as a token type, not an expression type.

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

			logical_and,
			logical_or,
			logical_not,

			equal,
			not_equal,
			lt,
			gt,
			lte,
			gte,

			// TODO: Handle these someday. Somehow.
			// assign,
			// assign_add,
			// assign_subtract,
			// assign_multiply,
			// assign_divide,
			// assign_bit_and,
			// assign_bit_or,
			// assign_bit_xor,
		};

		static constexpr int expression_type_precedence[] = {
			-1, // invalid,

			0, // parenthesis,
			0, // parenthesis_end, // Special-case: This is only used as a token type, not an expression type.

			1, // value,  // 1234
			1, // symbol, // .@local
			1, // dereference,

			4, // negate,
			4, // addition,
			4, // subtraction,
			3, // multiply,
			3, // divide,
			3, // modulo,
			2, // pow,

			5, // bit_not,
			6, // bit_and,
			6, // bit_or,
			6, // bit_xor,
			7, // left_shift,
			7, // right_shift,

			8, // logical_and,
			8, // logical_or,
			8, // logical_not,

			9, // equal,
			9, // not_equal,
			9, // lt,
			9, // gt,
			9, // lte,
			9, // gte,
		};

		static constexpr bool expression_type_left_associative[] = {
			true, // invalid,

			true, // parenthesis,
			true, // parenthesis_end, // Special-case: This is only used as a token type, not an expression type.

			true,  // value,  // 1234
			true,  // symbol, // .@local
			false, // dereference,

			true,  // negate,
			true,  // addition,
			true,  // subtraction,
			true,  // multiply,
			true,  // divide,
			true,  // modulo,
			false, // pow,

			true, // bit_not,
			true, // bit_and,
			true, // bit_or,
			true, // bit_xor,
			true, // left_shift,
			true, // right_shift,

			true, // logical_and,
			true, // logical_or,
			true, // logical_not,

			true, // equal,
			true, // not_equal,
			true, // lt,
			true, // gt,
			true, // lte,
			true, // gte,
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

} // namespace boxmon