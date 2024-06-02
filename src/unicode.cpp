#include "unicode.h"

#include "utf8.h"
#include "utf8_encode.h"

uint8_t iso8859_15_from_unicode(const uint32_t c)
{
	// line feed -> carriage return
	if (c == '\n') {
		return '\r';
	}

	// translate Unicode characters not part of Latin-1 but part of Latin-15
	switch (c) {
		case 0x20ac: // '�'
			return 0xa4;
		case 0x160: // '�'
			return 0xa6;
		case 0x161: // '�'
			return 0xa8;
		case 0x17d: // '�'
			return 0xb4;
		case 0x17e: // '�'
			return 0xb8;
		case 0x152: // '�'
			return 0xbc;
		case 0x153: // '�'
			return 0xbd;
		case 0x178: // '�'
			return 0xbe;
	}

	// remove Unicode characters part of Latin-1 but not part of Latin-15
	switch (c) {
		case 0xa4: // '�'
		case 0xa6: // '�'
		case 0xa8: // '�'
		case 0xb4: // '�'
		case 0xb8: // '�'
		case 0xbc: // '�'
		case 0xbd: // '�'
		case 0xbe: // '�'
			return '?';
	}

	// all other Unicode characters are also unsupported
	if (c >= 256) {
		return '?';
	}

	// everything else is Latin-15 already
	return c;
}

uint32_t unicode_from_iso8859_15(const uint8_t c)
{
	// translate Latin-15 characters not part of Latin-1
	switch (c) {
		case 0xa4:
			return 0x20ac; // '�'
		case 0xa6:
			return 0x160; // '�'
		case 0xa8:
			return 0x161; // '�'
		case 0xb4:
			return 0x17d; // '�'
		case 0xb8:
			return 0x17e; // '�'
		case 0xbc:
			return 0x152; // '�'
		case 0xbd:
			return 0x153; // '�'
		case 0xbe:
			return 0x178; // '�'
		default:
			return c;
	}
}

// converts the character to UTF-8 and prints it
void print_iso8859_15_char(const char c)
{
	char utf8[5];
	utf8_encode(utf8, unicode_from_iso8859_15(c));
	fmt::print("{}", utf8);
}
