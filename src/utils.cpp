#ifdef DEBUG
#ifndef __PS3__
#include <cstdarg>
#include <cstdio>
#else
#include <stdarg.h>
#include <stdio.h>
#endif
#endif

#include "ZunMath.hpp"
#include "i18n.hpp"
#include "utils.hpp"
#include "GameErrorContext.hpp"
#ifndef __PS3__
#include <cstdio>
#include <cstdarg>
#else
#include <stdio.h>
#include <stdarg.h>
#endif

namespace utils
{
void DebugPrint(const char *fmt, ...)
{
#ifdef DEBUG
    char tmpBuffer[512];
#ifndef __PS3__
    std::va_list args;
#else
    va_list args;
#endif

    va_start(args, fmt);
#ifndef __PS3__
    std::vsnprintf(tmpBuffer, 511, fmt, args);
#else
    ::vsnprintf(tmpBuffer, 511, fmt, args);
#endif
    va_end(args);

#ifndef __PS3__
    std::printf("DEBUG2: %s\n", tmpBuffer);
#else
    ::printf("DEBUG2: %s\n", tmpBuffer);
#endif
#endif
}

void Log(const char *fmt, ...)
{
    char tmpBuffer[512];
#ifndef __PS3__
    std::va_list args;
#else
    va_list args;
#endif

    va_start(args, fmt);
#ifndef __PS3__
    std::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#else
    ::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#endif
    va_end(args);

#ifndef __PS3__
    std::printf("%s\n", tmpBuffer);
    std::fflush(stdout);
#else
    ::printf("%s\n", tmpBuffer);
    ::fflush(stdout);
#endif
    GameErrorContext::Log(&g_GameErrorContext, "%s\n", tmpBuffer);
    g_GameErrorContext.Flush();
}

f32 AddNormalizeAngle(f32 a, f32 b)
{
    i32 i;

    i = 0;
    a += b;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 16)
            break;
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 16)
            break;
    }
    return a;
}

void Rotate(ZunVec3 *outVector, const ZunVec3 *point, f32 angle)
{
    f32 sinOut;
    f32 cosOut;

    sinOut = ZUN_SINF(angle);
    cosOut = ZUN_COSF(angle);
    outVector->x = cosOut * point->x + sinOut * point->y;
    outVector->y = cosOut * point->y - sinOut * point->x;
}

void DebugPrint2(const char *fmt, ...)
{
#ifdef DEBUG
    char tmpBuffer[512];
#ifndef __PS3__
    std::va_list args;
#else
    va_list args;
#endif

    va_start(args, fmt);
#ifndef __PS3__
    std::vsnprintf(tmpBuffer, 511, fmt, args);
#else
    ::vsnprintf(tmpBuffer, 511, fmt, args);
#endif
    va_end(args);

#ifndef __PS3__
    std::printf("DEBUG2: %s\n", tmpBuffer);
#else
    ::printf("DEBUG2: %s\n", tmpBuffer);
#endif
#endif
}
}; // namespace utils
