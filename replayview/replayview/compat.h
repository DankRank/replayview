#pragma once

// MSVC 6 replacements
#if defined(_MSC_VER) && _MSC_VER <= 1200
#define GetWindowLongPtr GetWindowLong
#define SetWindowLongPtr SetWindowLong
#define GWLP_USERDATA GWL_USERDATA
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed long int32_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef int intptr_t;
typedef long LONG_PTR;
#else
#include <stdint.h>
#endif

// C++11 replacements
#if __cplusplus < 201103L
#define nullptr NULL
#define constexpr const
//TODO: implement this as an actual function
#define memcpy_s(a,b,c,d) (memcpy(a,c,d),0)
#endif
