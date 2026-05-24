#include "GameErrorContext.hpp"
#include "FileSystem.hpp"
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
    char tmpBuffer[512];
    size_t tmpBufferSize;
    va_list args;

    va_start(args, fmt);
    std::vsprintf(tmpBuffer, fmt, args);

    tmpBufferSize = std::strlen(tmpBuffer);

    if (this->m_BufferEnd + tmpBufferSize < &this->m_Buffer[sizeof(this->m_Buffer) - 1])
    {
        std::strcpy(this->m_BufferEnd, tmpBuffer);

        this->m_BufferEnd += tmpBufferSize;
        *this->m_BufferEnd = '\0';
    }

    va_end(args);

#ifdef __PS3__
    if (this->m_BufferEnd != this->m_Buffer)
    {
        FileSystem::WriteDataToFile("log.txt", this->m_Buffer, (size_t)(this->m_BufferEnd - this->m_Buffer));
    }
#endif

    return fmt;
}

const char *GameErrorContext::Fatal(const char *fmt, ...)
{
    char tmpBuffer[512];
    size_t tmpBufferSize;
    va_list args;

    va_start(args, fmt);
    std::vsprintf(tmpBuffer, fmt, args);

    tmpBufferSize = std::strlen(tmpBuffer);

    if (this->m_BufferEnd + tmpBufferSize < &this->m_Buffer[sizeof(this->m_Buffer) - 1])
    {
        std::strcpy(this->m_BufferEnd, tmpBuffer);

        this->m_BufferEnd += tmpBufferSize;
        *this->m_BufferEnd = '\0';
    }

    va_end(args);

    this->m_ShowMessageBox = true;

#ifdef __PS3__
    if (this->m_BufferEnd != this->m_Buffer)
    {
        FileSystem::WriteDataToFile("log.txt", this->m_Buffer, (size_t)(this->m_BufferEnd - this->m_Buffer));
    }
#endif

    return fmt;
}

void GameErrorContext::Flush()
{
    FILE *logFile;

    if (m_BufferEnd != m_Buffer)
    {
        g_GameErrorContext.Log(TH_ERR_LOGGER_END);

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
