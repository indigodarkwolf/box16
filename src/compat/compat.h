#pragma once

#if defined(_MSC_VER)
#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif
#define __attribute__(...)

#ifdef _MSC_VER
#	ifndef PATH_MAX
#		include <windows.h>
#		define PATH_MAX MAX_PATH
#       undef max
#		undef min
#		endif
#endif

#include <stdio.h>
#include <cstdint>

void usleep(__int64 usec);
uint8_t
    __builtin_parity(uint8_t);
#elif defined(__linux__)
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#endif