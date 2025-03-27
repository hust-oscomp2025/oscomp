#pragma once
//#define sprint(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define sprint(fmt, ...)
//#define panic(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define panic(fmt, ...)
//#define assert(x) if (!(x)) panic("Assertion failed: %s\n", #x)
#define assert(x)


