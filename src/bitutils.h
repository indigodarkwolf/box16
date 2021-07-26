#pragma once
#if !defined(BITUTILS_H)
#	define BITUTILS_H

template <uint8_t msb, uint8_t lsb = msb>
uint8_t get_bit_field(const uint8_t value)
{
	static_assert(msb >= lsb);
	constexpr const uint8_t mask = (2 << (msb - lsb)) - 1;
	return (value >> lsb) & mask;
}

template <uint8_t msb, uint8_t lsb = msb>
uint8_t set_bit_field(const uint8_t src, const uint8_t value)
{
	static_assert(msb >= lsb);
	constexpr const uint8_t mask = (2 << (msb - lsb)) - 1;
	return (src & ~(mask << lsb)) | (value << lsb);
}

template <typename T>
constexpr T bit_set_or_res(const T val, const T mask, bool cond)
{
	return cond ? (val | mask) : (val & ~mask);
}

#endif