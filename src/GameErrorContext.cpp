#include "GameErrorContext.hpp"
#include "FileSystem.hpp"
#ifndef __PS3__
#include <SDL_messagebox.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#else
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#endif

GameErrorContext g_GameErrorContext;

const char *GameErrorContext::Log(GameErrorContext *ctx, const char *fmt, ...)
{
    char tmpBuffer[512];
    size_t tmpBufferSize;
    va_list args;

    va_start(args, fmt);
#ifndef __PS3__
    tmpBufferSize = std::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#else
    tmpBufferSize = ::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#endif

    if (ctx->m_BufferEnd + tmpBufferSize < &ctx->m_Buffer[sizeof(ctx->m_Buffer) - 1])
    {
#ifndef __PS3__
        std::strcpy(ctx->m_BufferEnd, tmpBuffer);
#else
        ::strcpy(ctx->m_BufferEnd, tmpBuffer);
#endif

        ctx->m_BufferEnd += tmpBufferSize;
        *ctx->m_BufferEnd = '\0';
    }

    va_end(args);

    return fmt;
}

const char *GameErrorContext::Fatal(GameErrorContext *ctx, const char *fmt, ...)
{
    char tmpBuffer[512];
    size_t tmpBufferSize;
    va_list args;

    va_start(args, fmt);
#ifndef __PS3__
    tmpBufferSize = std::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#else
    tmpBufferSize = ::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#endif

    if (ctx->m_BufferEnd + tmpBufferSize < &ctx->m_Buffer[sizeof(ctx->m_Buffer) - 1])
    {
#ifndef __PS3__
        std::strcpy(ctx->m_BufferEnd, tmpBuffer);
#else
        ::strcpy(ctx->m_BufferEnd, tmpBuffer);
#endif

        ctx->m_BufferEnd += tmpBufferSize;
        *ctx->m_BufferEnd = '\0';
    }

    va_end(args);

    ctx->m_ShowMessageBox = true;

    return fmt;
}

void GameErrorContext::Flush()
{
    if (m_BufferEnd != m_Buffer)
    {
        if (m_ShowMessageBox)
        {
#ifndef __PS3__
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "log", m_Buffer, NULL);
#endif
            m_ShowMessageBox = false;
        }

        if (FileSystem::WriteDataToFile("log.txt", m_Buffer, (size_t)(m_BufferEnd - m_Buffer)) != 0)
        {
#ifdef __PS3__
            ::printf("Error: Could not write log.txt\n");
#endif
        }
    }
}
