#pragma once

#include <string>

namespace boxmon
{
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

	struct expression_type_info {
		int  precedence;
		bool left_associative;
	};

	const expression_type_info &get_expression_type_info(expression_type type);

	//
	// Expression
	//

	class expression_base
	{
	public:
		expression_base(expression_type type);
		virtual ~expression_base();
		virtual int     evaluate() const = 0;
		expression_type get_type() const;

	private:

		expression_type m_type;
	};

	class value_expression final : public expression_base
	{
	public:
		value_expression(const int &value);
		virtual ~value_expression() override;
		virtual int evaluate() const override;

	private:
		int m_value;
	};

	class symbol_expression final : public expression_base
	{
	public:
		symbol_expression(const std::string &symbol);
		virtual ~symbol_expression() override;
		virtual int evaluate() const override;

		bool is_valid() const;

	private:
		std::string m_symbol;
	};

	class unary_expression final : public expression_base
	{
	public:
		unary_expression(expression_type type, const expression_base *param);
		virtual ~unary_expression() override;
		virtual int evaluate() const override;

	private:
		const expression_base *m_param;
	};

	class binary_expression final : public expression_base
	{
	public:
		binary_expression(expression_type type, const expression_base *lhs, const expression_base *rhs);
		virtual ~binary_expression() override;
		virtual int evaluate() const override;

	private:
		const expression_base *m_lhs;
		const expression_base *m_rhs;
	};

} // namespace boxmon