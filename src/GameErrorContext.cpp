#include "GameErrorContext.hpp"
#include "FileSystem.hpp"
#include "GamePaths.hpp"
#ifndef __PS3__
#include <SDL2/SDL_messagebox.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#else
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#endif

GameErrorContext g_GameErrorContext;

const char *GameErrorContext::Log(const char *fmt, ...)
{
    char tmpBuffer[1024];
    int tmpBufferSize;
    va_list args;

    va_start(args, fmt);
#ifndef __PS3__
    tmpBufferSize = std::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#else
    tmpBufferSize = vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#endif
    va_end(args);

    if (tmpBufferSize > 0)
    {
        if (tmpBufferSize >= (int)sizeof(tmpBuffer))
        {
            tmpBufferSize = sizeof(tmpBuffer) - 1;
        }

        if (this->m_BufferEnd + tmpBufferSize < &this->m_Buffer[sizeof(this->m_Buffer) - 1])
        {
            std::memcpy(this->m_BufferEnd, tmpBuffer, tmpBufferSize);
            this->m_BufferEnd += tmpBufferSize;
            *this->m_BufferEnd = '\0';
        }
    }

    this->Flush();

    return fmt;
}

const char *GameErrorContext::Fatal(const char *fmt, ...)
{
    char tmpBuffer[1024];
    int tmpBufferSize;
    va_list args;

    va_start(args, fmt);
#ifndef __PS3__
    tmpBufferSize = std::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#else
    tmpBufferSize = vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);
#endif
    va_end(args);

    this->m_ShowMessageBox = true;

    if (tmpBufferSize > 0)
    {
        if (tmpBufferSize >= (int)sizeof(tmpBuffer))
        {
            tmpBufferSize = sizeof(tmpBuffer) - 1;
        }

        if (this->m_BufferEnd + tmpBufferSize < &this->m_Buffer[sizeof(this->m_Buffer) - 1])
        {
            std::memcpy(this->m_BufferEnd, tmpBuffer, tmpBufferSize);
            this->m_BufferEnd += tmpBufferSize;
            *this->m_BufferEnd = '\0';
        }
    }

    this->Flush();

    return fmt;
}

void GameErrorContext::Flush()
{
    if (!GamePaths::IsInitialized())
    {
        return;
    }

    if (m_BufferEnd != m_Buffer)
    {
        if (m_ShowMessageBox)
        {
#ifndef __PS3__
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "log", m_Buffer, NULL);
#endif
#ifndef DDEBUG
            m_ShowMessageBox = false;
        }
#else
        FILE *f = FileSystem::FopenUTF8("log.txt", "ab");
        if (f != NULL)
        {
            std::fwrite(m_Buffer, 1, (size_t)(m_BufferEnd - m_Buffer), f);
            std::fclose(f);
        }
#endif
        m_BufferEnd = m_Buffer;
        m_Buffer[0] = '\0';
    }
}
