#pragma once

#include <cstring>
#include "inttypes.hpp"

#ifndef __PS3__
#include <SDL_endian.h>
#else
#ifndef SDL_LIL_ENDIAN
#define SDL_LIL_ENDIAN 1234
#endif
#ifndef SDL_BIG_ENDIAN
#define SDL_BIG_ENDIAN 4321
#endif
#ifndef SDL_BYTEORDER
#define SDL_BYTEORDER SDL_BIG_ENDIAN
#endif
#ifndef SDL_FLOATWORDORDER
#define SDL_FLOATWORDORDER SDL_BIG_ENDIAN
#endif

static inline u16 SDL_Swap16(u16 x) {
    return (u16)((x << 8) | (x >> 8));
}
static inline u32 SDL_Swap32(u32 x) {
    return ((x << 24) & 0xff000000) |
           ((x <<  8) & 0x00ff0000) |
           ((x >>  8) & 0x0000ff00) |
           ((x >> 24) & 0x000000ff);
}
static inline u64 SDL_Swap64(u64 x) {
    return ((x << 56) & 0xff00000000000000ULL) |
           ((x << 40) & 0x00ff000000000000ULL) |
           ((x << 24) & 0x0000ff0000000000ULL) |
           ((x <<  8) & 0x000000ff00000000ULL) |
           ((x >>  8) & 0x00000000ff000000ULL) |
           ((x >> 24) & 0x0000000000ff0000ULL) |
           ((x >> 40) & 0x000000000000ff00ULL) |
           ((x >> 56) & 0x00000000000000ffULL);
}
#endif

template <int S> struct UIForSize_t;
template <> struct UIForSize_t<1> { typedef u8 type; };
template <> struct UIForSize_t<2> { typedef u16 type; };
template <> struct UIForSize_t<4> { typedef u32 type; };
template <> struct UIForSize_t<8> { typedef u64 type; };

#if defined(__GNUC__) || defined(__clang__)
#define UNALIGNED_ATTR __attribute__((packed))
#elif defined _MSC_VER
#define UNALIGNED_ATTR __declspec(align(1))
#else
#define UNALIGNED_ATTR
#define NEEDS_FALLBACK
#endif

template <typename T> struct UNALIGNED_ATTR Unaligned
{
    T data;

    inline operator T() const
    {
#ifdef NEEDS_FALLBACK
        T ret;
        std::memcpy(&ret, &data, sizeof(T));
        return ret;
#else
        return data;
#endif
    }
};

#undef UNALIGNED_ATTR
#undef NEEDS_FALLBACK

template <typename T>
static inline typename UIForSize_t<sizeof(T)>::type bit_cast_from_size(void *value) {
    return ((Unaligned<typename UIForSize_t<sizeof(T)>::type> *)value)->data;
}

template <typename T>
static inline T bit_cast_to_size(typename UIForSize_t<sizeof(T)>::type value) {
    T ret;
    std::memcpy(&ret, &value, sizeof(value));
    return ret;
}

static inline u8  ZunByteswap(u8 in)  { return in; }
static inline u16 ZunByteswap(u16 in) { return SDL_Swap16(in); }
static inline u32 ZunByteswap(u32 in) { return SDL_Swap32(in); }
static inline u64 ZunByteswap(u64 in) { return SDL_Swap64(in); }

template <typename T>
struct LE {
    T raw;

    inline operator T() const {
        typename UIForSize_t<sizeof(T)>::type ui = bit_cast_from_size<T>((void *)&raw);

        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            ui = ZunByteswap(ui);
        }

        return bit_cast_to_size<T>(ui);
    }

    inline LE &operator=(const T &a) {
        typename UIForSize_t<sizeof(T)>::type ui = bit_cast_from_size<T>((void *)&a);

        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            ui = ZunByteswap(ui);
        }

        raw = bit_cast_to_size<T>(ui);
        return *this;
    }
};

template <>
struct LE<float> {
    float raw;

    inline operator float() const {
        typename UIForSize_t<sizeof(float)>::type ui = bit_cast_from_size<float>((void*) &raw);

        if (SDL_FLOATWORDORDER == SDL_BIG_ENDIAN) {
            ui = ZunByteswap(ui);
        }

        return bit_cast_to_size<float>(ui);
    }

    inline LE &operator=(const float &a) {
        typename UIForSize_t<sizeof(float)>::type ui = bit_cast_from_size<float>((void *)&a);

        if (SDL_FLOATWORDORDER == SDL_BIG_ENDIAN) {
            ui = ZunByteswap(ui);
        }

        raw = bit_cast_to_size<float>(ui);
        return *this;
    }
};
