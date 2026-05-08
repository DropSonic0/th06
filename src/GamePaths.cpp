#include "GamePaths.hpp"

#ifndef __PS3__
#include <SDL.h>
#else
#include <cell/cell_fs.h>
#endif
#ifdef __PS3__
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#else
#ifndef __PS3__
#include <cstdio>
#include <cstring>
#else
#include <stdio.h>
#include <string.h>
#endif
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace GamePaths
{

static char s_userPath[512] = "";

void Init()
{
#ifdef __ANDROID__
    const char *internalPath = SDL_AndroidGetExternalStoragePath();
    if (internalPath)
    {
        snprintf(s_userPath, sizeof(s_userPath), "%s/", internalPath);
        SDL_Log("GamePaths: user data path = %s", s_userPath);
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "GamePaths: SDL_AndroidGetExternalStoragePath() returned NULL, using cwd");
        s_userPath[0] = '\0';
    }
#elif defined(__PS3__)
    snprintf(s_userPath, sizeof(s_userPath), "/dev_hdd0/game/TH06PORT0/USRDIR/");
#else
    // Desktop: all files relative to the working directory.
    s_userPath[0] = '\0';
#endif
}

const char *GetUserPath()
{
    return s_userPath;
}

bool IsAssetPath(const char *path)
{
    if (!path || !*path)
        return false;

    // Paths starting with these prefixes are read-only game assets:
#ifdef __PS3__
    if (::strncmp(path, "data/", 5) == 0 || ::strncmp(path, "data\\", 5) == 0)
#else
    if (strncmp(path, "data/", 5) == 0 || strncmp(path, "data\\", 5) == 0)
#endif
        return true;
    if (strncmp(path, "bgm/", 4) == 0 || strncmp(path, "bgm\\", 4) == 0)
        return true;
    if (strncmp(path, "font/", 5) == 0 || strncmp(path, "font\\", 5) == 0)
        return true;

    // Any .dat file (pbg3 archives like紅魔郷IN.dat)
#ifdef __PS3__
    const char *dot = ::strrchr(path, '.');
#else
    const char *dot = strrchr(path, '.');
#endif
    if (dot)
    {
#ifdef _WIN32
        if (_stricmp(dot, ".dat") == 0)
            return true;
#else
#ifdef __PS3__
        if (::strlen(dot) == 4 && ::tolower(dot[1]) == 'd' && ::tolower(dot[2]) == 'a' && ::tolower(dot[3]) == 't')
            return true;
#else
        if (strcasecmp(dot, ".dat") == 0)
            return true;
#endif
#endif
    }

    return false;
}

void Resolve(char *outBuf, size_t outBufSize, const char *path)
{
    if (!path || !*path)
    {
        outBuf[0] = '\0';
        return;
    }

    // Strip leading "./" or ".\\"
    if (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))
        path += 2;

#ifdef __PS3__
    // On PS3, we always want to prepend the absolute base path to ensure 
    // files are found regardless of whether they are "assets" or "user data".
    ::snprintf(outBuf, outBufSize, "%s%s", s_userPath, path);
#else
    if (IsAssetPath(path))
    {
        // Asset: keep the relative path as-is.
        // SDL_RWFromFile on Android reads from APK assets/ automatically.
        snprintf(outBuf, outBufSize, "%s", path);
    }
    else
    {
        // User data: prepend the writable user-data directory.
        snprintf(outBuf, outBufSize, "%s%s", s_userPath, path);
    }
#endif
}

void EnsureParentDir(const char *resolvedPath)
{
    // Find the last directory separator and create the directory.
    char dirBuf[512];
#ifdef __PS3__
    ::snprintf(dirBuf, sizeof(dirBuf), "%s", resolvedPath);

    char *lastSep = ::strrchr(dirBuf, '/');
#else
    snprintf(dirBuf, sizeof(dirBuf), "%s", resolvedPath);

    char *lastSep = strrchr(dirBuf, '/');
#endif
#ifdef _WIN32
    {
        char *lastBs = strrchr(dirBuf, '\\');
        if (lastBs && (!lastSep || lastBs > lastSep))
            lastSep = lastBs;
    }
#endif

    if (lastSep && lastSep != dirBuf)
    {
        *lastSep = '\0';
#ifdef _WIN32
        _mkdir(dirBuf);
#elif defined(__PS3__)
        cellFsMkdir(dirBuf, 0777);
#else
        mkdir(dirBuf, 0755);
#endif
    }
}

} // namespace GamePaths
