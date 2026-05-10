#pragma once

#if defined(__PS3__) || defined(__CELLOS_LV2__) || defined(__PPU__) || defined(__ppu__) || defined(__SNC__) || defined(SN_TARGET_PS3)
#ifndef __PS3__
#define __PS3__
#endif
#endif

#ifndef __PS3__
#include <cstdint>

typedef std::int8_t i8;
typedef std::uint8_t u8;
typedef std::int16_t i16;
typedef std::uint16_t u16;
typedef std::int32_t i32;
typedef std::uint32_t u32;
typedef std::int64_t i64;
typedef std::uint64_t u64;
typedef std::intptr_t iptr;
typedef std::uintptr_t uptr;
#else
#include <stdint.h>

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef intptr_t iptr;
typedef uintptr_t uptr;
#endif
typedef float f32;
typedef double f64;

#ifdef __ANDROID__
inline f32 uf32(const f32* ptr) {
    u32 temp = *(const u32*)ptr;
    __asm__ volatile ("":"+r"(temp));
    return __builtin_bit_cast(float, temp);
}
#else
inline f32 uf32(const f32* ptr) {
    return *ptr;
}
#endif