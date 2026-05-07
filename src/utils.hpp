#pragma once
#include "GameErrorContext.hpp"
#include "ZunMath.hpp"
#include "ZunResult.hpp"
#include "inttypes.hpp"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ARRAY_SIZE_SIGNED(x) ((i32)sizeof(x) / (i32)sizeof(x[0]))

#define ZUN_BIT(a) (1 << (a))
#define ZUN_MASK(a) (ZUN_BIT(a) - 1)
#define ZUN_RANGE(a, count) (ZUN_MASK((a) + (count)) & ~ZUN_MASK(a))
#define ZUN_CLEAR_BITS(a, keep_mask) (a & ~keep_mask)

#define IS_PRESSED(key) (g_CurFrameInput & (key))
#define WAS_PRESSED(key) (((g_CurFrameInput & (key)) != 0) && (g_CurFrameInput & (key)) != (g_LastFrameInput & (key)))
#define WAS_PRESSED_PERIODIC(key)                                                                                      \
    (WAS_PRESSED(key) || (((g_CurFrameInput & (key)) != 0) && (g_IsEigthFrameOfHeldInput != 0)))

namespace utils
{
#ifdef __PS3__
inline uint32_t Swap32(uint32_t x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8) |
           ((x & 0x00FF0000) >> 8) |
           ((x & 0xFF000000) >> 24);
}
inline uint16_t Swap16(uint16_t x) {
    return ((x & 0x00FF) << 8) |
           ((x & 0xFF00) >> 8);
}
#define VEC3_SET(v, _x, _y, _z) do { (v).x = (_x); (v).y = (_y); (v).z = (_z); } while(0)
#define VEC2_SET(v, _x, _y) do { (v).x = (_x); (v).y = (_y); } while(0)
#else
#define VEC3_SET(v, _x, _y, _z) (v) = ZunVec3(_x, _y, _z)
#define VEC2_SET(v, _x, _y) (v) = ZunVec2(_x, _y)
#endif

void DebugPrint(const char *fmt, ...);
void DebugPrint2(const char *fmt, ...);

f32 AddNormalizeAngle(f32 a, f32 b);
void Rotate(ZunVec3 *outVector, const ZunVec3 *point, f32 angle);
}; // namespace utils
