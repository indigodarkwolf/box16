#include "expression.h"

#include "memory.h"
#include "symbols.h"

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
		auto namelist = symbols_find(m_symbol);
		if (namelist.empty()) {
			return 0;
		}
		return namelist.front();
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
} // namespace boxmon