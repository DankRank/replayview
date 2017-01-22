#pragma once
#include <stdint.h>

// ARRAY_SIZE and endianness check are from GerbilSoft/rom-properties

#define ARRAY_SIZE(x) \
	((int)(((sizeof(x) / sizeof(x[0]))) / \
		(size_t)(!(sizeof(x) % sizeof(x[0])))))

enum {
	ENDIAN_LITTLE,
	ENDIAN_BIG
};

#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
    defined(__hppa__) || defined(__HPPA__) || \
    defined(__m68k__) || defined(__MC68K__) || defined(_M_M68K) || \
    defined(mc68000) || defined(M68000) || \
    ((defined(__mips) || defined(__mips__) || defined(__mips) || defined(__MIPS__)) && \
     (defined(__mipseb) || defined(__mipseb__) || defined(__MIPSEB) || defined(__MIPSEB__))) || \
    defined(__powerpc__) || defined(__POWERPC__) || \
    defined(__ppc__) || defined(__PPC__) || defined(_M_PPC) || \
    defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || \
    defined(__sparc) || defined(__sparc__)

/* System is big-endian. */
constexpr int ENDIANNESS = ENDIAN_BIG;

#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_PDP_ENDIAN__)

/* System is PDP-endian. */
#error Why are you trying to run this on a PDP?

#else

/* System is little-endian. */
constexpr int ENDIANNESS = ENDIAN_LITTLE;

#endif

constexpr uint32_t TO_MAGIC(char a, char b, char c, char d){
	return
		ENDIANNESS == ENDIAN_LITTLE
		? (a) | ((b) << 8) | ((c) << 16) | ((d) << 24)
		: ((a) << 24) | ((b) << 16) | ((c) << 8) | (d);
}