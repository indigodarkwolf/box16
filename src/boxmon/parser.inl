#pragma once

#include "parser.h"

namespace boxmon
{
	template <typename T>
	bool parser::parse_hex_number(T &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '9') {
				return true;
			}
			if (c >= 'a' && c <= 'f') {
				return true;
			}
			if (c >= 'A' && c <= 'F') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}
			if (c >= 'a' && c <= 'f') {
				return 10 + c - 'a';
			}
			if (c >= 'A' && c <= 'F') {
				return 10 + c - 'A';
			}
			return -1;
		};

		char const *look = input;
		while (isalnum(*look)) {
			if (!isnum(*look)) {
				break;
			}
			result <<= 4;
			result |= tonum(*look);
			++look;
		}

		if (input == look) {
			return false;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	template <typename T>
	bool parser::parse_dec_number(T &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '9') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}

			return -1;
		};

		char const *look = input;
		while (isalnum(*look)) {
			if (!isnum(*look)) {
				break;
			}
			result *= 10;
			result += tonum(*look);
			++look;
		}

		if (input == look) {
			return false;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	template <typename T>
	bool parser::parse_oct_number(T &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '7') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '7') {
				return c - '0';
			}

			return -1;
		};

		char const *look = input;
		while (isalnum(*look)) {
			if (!isnum(*look)) {
				break;
			}
			result <<= 3;
			result |= tonum(*look);
			++look;
		}

		if (input == look) {
			return false;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	template <typename T>
	bool parser::parse_bin_number(T &result, char const *&input)
	{
		result = 0;

		const auto isnum = [](const char c) -> bool {
			if (c >= '0' && c <= '1') {
				return true;
			}

			return false;
		};

		const auto tonum = [](const char c) -> int {
			if (c >= '0' && c <= '1') {
				return c - '0';
			}

			return -1;
		};

		char const *look = input;
		while (isalnum(*look)) {
			if (!isnum(*look)) {
				break;
			}
			result <<= 1;
			result |= tonum(*look);
			++look;
		}

		if (input == look) {
			return false;
		}

		input = look;

		skip_whitespace(input);
		return true;
	}

	/*
	    A number can be interpreted in several different radices.

	    If the number is preceded with an explicit radix type (see "parse_radix_type"),
	    then it is expected to be the sequence of alphanumeric characters following
	    after the space separating the radix type from the number. If any alphanumeric
	    characters are encountered that are not within the specified radix, parsing
	    fails because the radix was explicitly specified.

	    Otherwise, the number may be preceded by an explicit radix prefix (see
	    "parse_radix_prefix"), then it is expected to be the sequence of alphanumeric
	    characters following immediately after the prefix. If any alphanumeric
	    characters are encountered that are not within the specified radix, parsing
	    fails because the radix was explicitly specified.

	    Otherwise, the number is a best-guess based on the parser's default radix and
	    the alphanumeric characters provided, interpreting the value as the smallest
	    radix greater than or equal to the default radix, but not greater than a radix
	    of 16. If any alphanumeric characters are encountered that are not within a
	    radix of 16 (i.e. hexadecimal digits), parsing fails because the largest radix
	    understood by the parser is 16.

	    Some canonical examples include:

	    b 101001001010 ; explicitly binary, parses to decimal 2634, or hexadecimal A4A
	    o 555          ; explicitly octal, parses to decimal 365, or hexadecimal 16D
	    h d00d         ; explicitly hexadecimal, parses to decimal 53261
	    d 1234         ; explicitly decimal, parses to decimal 1234, or hexadecimal 4D2

	    %101001001010  ; explicitly binary, parses to decimal 2634, or hexadecimal A4A
	    o555           ; explicitly octal, parses to decimal 365, or hexadecimal 16D
	    $d00d          ; explicitly hexadecimal, parses to decimal 53261
	    hd00d          ; explicitly hexadecimal, parses to decimal 53261
	    0xd00d         ; explicitly hexadecimal, parses to decimal 53261

	    1234           ; Will depend greatly on the default radix of the parser:
	                    ; parses to decimal 668 if the default radix is binary or octal
	                    ; parses to decimal 1234 if the default radix is decimal
	                    ; parses to decimal 4660 if the default radix is hexadecimal

	    0555           ; Will depend greatly on the default radix of the parser:
	                    ; parses to decimal 365 if the default radix is binary or octal
	                    ; parses to decimal 555 if the default radix is decimal
	                    ; parses to decimal 1365 if the default radix is hexadecimal

	    b 102          ; explicitly binary, but does not parse because '2' is not binary
	    o 678          ; explicitly octal, but does not parse because '8' is not octal
	    d 89a          ; explicitly decimal, but does not parse because 'a' is not decimal
	    h efg          ; explicitly hexadecimal, but does not parse because 'g' is not
	                    ; hexadecimal

	    %102           ; explicitly binary, but does not parse because '2' is not binary
	    o678           ; explicitly octal, but does not parse because '8' is not octal
	    $efg           ; explicitly hexadecimal, but does not parse because 'g' is not
	                    ; hexadecimal
	    hefg           ; explicitly hexadecimal, but does not parse because 'g' is not
	                    ; hexadecimal

	    123g           ; will never parse as a number because 'g' is not hexadecimal

	    The reason a prefix of `0` does not count as an explicit "octal", in spite of
	    the commonality of that convention and the parser's acceptance of "0x", is because
	    ca65 and other compilers output hexadecimal numbers with one or more leading zeroes
	    but without an explicit radix. Consequently, cases that are ambiguous need to be
	    resolved assuming these are not indicating a radix. This also excludes the common
	    prefix "0b" for binary.
	*/
	template <typename T>
	bool parser::parse_number(T &result, char const *&input)
	{
		char const *look = input;

		radix_type radix = m_default_radix;

		bool explicit_radix = parse_radix_type(radix, look) == true;
		if (!explicit_radix) {
			explicit_radix = parse_radix_prefix(radix, look) == true;
		}

		if (explicit_radix) {
			switch (radix) {
				case radix_type::bin:
					if (parse_bin_number(result, look) == false) {
						return false;
					}
					break;
				case radix_type::oct:
					if (parse_oct_number(result, look) == false) {
						return false;
					}
					break;
				case radix_type::dec:
					if (parse_dec_number(result, look) == false) {
						return false;
					}
					break;
				case radix_type::hex:
					if (parse_hex_number(result, look) == false) {
						return false;
					}
					break;
				default:
					return false;
			}
		} else {
			switch (m_default_radix) {
				case radix_type::bin:
					if (parse_bin_number(result, look) == true) {
						break;
					}
					[[fallthrough]];
				case radix_type::oct:
					if (parse_oct_number(result, look) == true) {
						break;
					}
					[[fallthrough]];
				case radix_type::dec:
					if (parse_dec_number(result, look) == true) {
						break;
					}
					[[fallthrough]];
				case radix_type::hex:
					if (parse_hex_number(result, look) == true) {
						break;
					}
					[[fallthrough]];
				default:
					return false;
			}
		}
		input = look;
		return true;
	}
} // namespace boxmon