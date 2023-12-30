#include "expression.h"

#include "memory.h"
#include "symbols.h"

namespace boxmon
{
	static const expression_type_info Expression_type_infos[] = {
		{ -1, true }, // invalid

		{ 0, true }, // parenthesis,
		{ 0, true }, // parenthesis_end, // Special-case: This is only used as a token type, not an expression type.

		{ 1, true },  // value,  // 1234
		{ 1, true },  // symbol, // .@local
		{ 1, false }, // dereference,

		{ 4, true },  // negate,
		{ 4, true },  // addition,
		{ 4, true },  // subtraction,
		{ 3, true },  // multiply,
		{ 3, true },  // divide,
		{ 3, true },  // modulo,
		{ 2, false }, // pow,

		{ 5, true }, // bit_not,
		{ 6, true }, // bit_and,
		{ 6, true }, // bit_or,
		{ 6, true }, // bit_xor,
		{ 7, true }, // left_shift,
		{ 7, true }, // right_shift,

		{ 8, true }, // logical_and,
		{ 8, true }, // logical_or,
		{ 8, true }, // logical_not,

		{ 9, true }, // equal,
		{ 9, true }, // not_equal,
		{ 9, true }, // lt,
		{ 9, true }, // gt,
		{ 9, true }, // lte,
		{ 9, true }, // gte,
	};

	const expression_type_info &get_expression_type_info(expression_type type)
	{
		return Expression_type_infos[static_cast<int>(type)];
	}

	//
	// Expression
	//

	expression_base::expression_base(expression_type type)
	    : m_type(type)
	{
	}

	expression_base::~expression_base()
	{
	}

	expression_type expression_base::get_type() const
	{
		return m_type;
	}

	value_expression::value_expression(const int &value)
	    : expression_base(expression_type::value),
	      m_value(value)
	{
	}

	value_expression::~value_expression()
	{
	}

	int value_expression::evaluate() const
	{
		return m_value;
	}

	symbol_expression::symbol_expression(const std::string &symbol)
	    : expression_base(expression_type::symbol),
	      m_symbol(symbol)
	{
	}

	symbol_expression::~symbol_expression()
	{
	}

	int symbol_expression::evaluate() const
	{
		auto namelist = symbols_find(m_symbol);
		if (namelist.empty()) {
			return 0;
		}
		return namelist.front();
	}

	bool symbol_expression::is_valid() const
	{
		auto namelist = symbols_find(m_symbol);
		return !namelist.empty();
	}


	unary_expression::unary_expression(expression_type type, const expression_base *param)
	    : expression_base(type),
	      m_param(param)
	{
	}

	unary_expression::~unary_expression()
	{
		delete m_param;
	}

	int unary_expression::evaluate() const
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

	binary_expression::binary_expression(expression_type type, const expression_base *lhs, const expression_base *rhs)
	    : expression_base(type),
	      m_lhs(lhs),
	      m_rhs(rhs)
	{
	}

	binary_expression::~binary_expression()
	{
		delete m_lhs;
		delete m_rhs;
	}

	int binary_expression::evaluate() const
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
} // namespace boxmon