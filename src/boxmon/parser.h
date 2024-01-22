#pragma once

#include <string>

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

	using address_type = std::tuple<uint16_t, uint8_t>;

	class expression
	{
	public:
		virtual ~expression()
		{
		}
		virtual const std::string &get_string() const = 0;
		virtual int                evaluate() const   = 0;
	};

	enum expression_parse_flags_ {
		expression_parse_flags_none             = 0,
		expression_parse_flags_must_consume_all = 1 << 0,
		expression_parse_flags_suppress_errors  = 1 << 1
	};

	typedef int expression_parse_flags; //

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

		bool parse_address(address_type &, char const *&input);
		bool parse_address_range(address_type &result0, address_type &result1, char const *&input);
		bool parse_bankname(uint8_t &bank, char const *&input);
		bool parse_expression(const expression *&expression, char const *&input, expression_parse_flags flags = expression_parse_flags_none);

		void set_default_radix(radix_type radix);
		radix_type get_default_radix();
		void set_default_bank(uint8_t bank);
		uint8_t get_default_bank();

	private:
		radix_type m_default_radix = radix_type::hex;
		uint8_t    m_default_bank  = 0;
	};
} // namespace boxmon

#include "parser.inl"
