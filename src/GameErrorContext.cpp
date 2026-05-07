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
    std::vsprintf(tmpBuffer, fmt, args);
    tmpBufferSize = std::strlen(tmpBuffer);
#else
    ::vsprintf(tmpBuffer, fmt, args);
    tmpBufferSize = ::strlen(tmpBuffer);
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
    std::vsprintf(tmpBuffer, fmt, args);
    tmpBufferSize = std::strlen(tmpBuffer);
#else
    ::vsprintf(tmpBuffer, fmt, args);
    tmpBufferSize = ::strlen(tmpBuffer);
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
#ifdef __PS3__
    ::FILE *logFile;
#else
    FILE *logFile;
#endif

    if (m_BufferEnd != m_Buffer)
    {
        GameErrorContext::Log(this, TH_ERR_LOGGER_END);

        if (m_ShowMessageBox)
        {
#ifndef __PS3__
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "log", m_Buffer, NULL);
#endif
        }

        logFile = FileSystem::FopenUTF8("./log.txt", "w");

#ifndef __PS3__
        std::fprintf(logFile, "%s", m_Buffer);
        std::fclose(logFile);
#else
        ::fprintf(logFile, "%s", m_Buffer);
        ::fclose(logFile);
#endif
    }
}
