// Smoke test for the Linux/SDL2 target header: verifies the fundamental type
// sizes the engine assumes. Compiled as the `platform_check` CMake target.

#include "Platform.h"
#include <cstdio>

static_assert(sizeof(int8) == 1, "int8");
static_assert(sizeof(uint8) == 1, "uint8");
static_assert(sizeof(int16) == 2, "int16");
static_assert(sizeof(uint16) == 2, "uint16");
static_assert(sizeof(int32) == 4, "int32");
static_assert(sizeof(uint32) == 4, "uint32");
static_assert(sizeof(int64) == 8, "int64");
static_assert(sizeof(uint64) == 8, "uint64");
static_assert(sizeof(fp32) == 4, "fp32");
static_assert(sizeof(fp64) == 8, "fp64");
static_assert(sizeof(mint) == sizeof(void*), "mint must be pointer-sized");
static_assert(sizeof(aint) == sizeof(void*), "aint must be pointer-sized");
static_assert(sizeof(vec128) == 16, "vec128");
static_assert(sizeof(wchar) == 2, "wchar");

int main()
{
	printf("platform_check: TARGET_LINUX_SDL2 OK (%zu-bit pointers)\n", sizeof(void*) * 8);
	return 0;
}
