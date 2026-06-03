#include "GamePaths.hpp"
#include "GameErrorContext.hpp"

#ifndef __PS3__
#include <SDL.h>
#else
#include <cell/cell_fs.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_gamecontent.h>
#include <sysutil/sysutil_common.h>
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

#ifdef __PS3__
extern "C" int cellSysmoduleLoadModule(uint16_t id);
#endif

namespace GamePaths
{

static char s_userPath[512] = "";
#ifdef __PS3__
static char s_savePath[512] = "";
static bool s_isJapanese = false;
#elif defined(THJP)
static bool s_isJapanese = true;
#else
static bool s_isJapanese = false;
#endif

void Init()
{
#ifdef __PS3__
    cellSysmoduleLoadModule(0x003e); // CELL_SYSMODULE_SYSUTIL_GAME

    CellFsStat st;
    if (cellFsStat("/dev_bdvd/kouma/", &st) == CELL_FS_SUCCEEDED)
    {
        // Assets are in USRDIR
        ::snprintf(s_userPath, sizeof(s_userPath), "/dev_bdvd/kouma/");
        s_isJapanese = true;
    }
    else
    {
        ::snprintf(s_userPath, sizeof(s_userPath), "/dev_hdd0/game/TH06PORT0/USRDIR/");
        s_isJapanese = false;
    }

    // Default save path to asset path
    ::snprintf(s_savePath, sizeof(s_savePath), "%s", s_userPath);
#endif

#ifdef __ANDROID__
    const char *internalPath = SDL_AndroidGetExternalStoragePath();
    if (internalPath)
    {
        ::snprintf(s_userPath, sizeof(s_userPath), "%s/", internalPath);
        SDL_Log("GamePaths: user data path = %s", s_userPath);
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "GamePaths: SDL_AndroidGetExternalStoragePath() returned NULL, using cwd");
        s_userPath[0] = '\0';
    }
#elif defined(__PS3__)
    // Initialize Game Utility for saves
    {
        CellGameSetInitParams init;
        ::memset(&init, 0, sizeof(CellGameSetInitParams));
        ::strncpy(init.title, "Touhou 06 Saves", CELL_GAME_SYSP_TITLE_SIZE);
        ::strncpy(init.titleId, "TH06SAVES", CELL_GAME_SYSP_TITLEID_SIZE);
        ::strncpy(init.version, "01.00", CELL_GAME_SYSP_VERSION_SIZE);

        char contentInfoPath[128];
        char usrdirPath[128];
        ::memset(contentInfoPath, 0, 128);
        ::memset(usrdirPath, 0, 128);
        CellGameContentSize size;

        cellSysutilCheckCallback();
        // GAMETYPE_GAMEDATA is 3
        int ret = cellGameDataCheck(3, init.titleId, &size);
        
        bool shouldPermit = (ret == 0);

        if (ret != 0)
        {
            cellSysutilCheckCallback();
            ret = cellGameCreateGameData(&init, contentInfoPath, usrdirPath);
            if (ret == 0)
            {
                // Copy ICON0.PNG
                char srcIcon[256], dstIcon[256];
                ::snprintf(srcIcon, sizeof(srcIcon), "%sICON0.PNG", s_userPath);
                ::snprintf(dstIcon, sizeof(dstIcon), "%s/ICON0.PNG", contentInfoPath);
                int fsrc, fdst;
                if (cellFsOpen(srcIcon, CELL_FS_O_RDONLY, &fsrc, NULL, 0) == CELL_FS_SUCCEEDED)
                {
                    if (cellFsOpen(dstIcon, CELL_FS_O_WRONLY | CELL_FS_O_CREAT | CELL_FS_O_TRUNC, &fdst, NULL, 0) ==
                        CELL_FS_SUCCEEDED)
                    {
                        static char buffer[4096];
                        uint64_t bytes;
                        while (cellFsRead(fsrc, buffer, sizeof(buffer), &bytes) == CELL_FS_SUCCEEDED && bytes > 0)
                        {
                            uint64_t written;
                            cellFsWrite(fdst, buffer, bytes, &written);
                        }
                        cellFsClose(fdst);
                    }
                    cellFsClose(fsrc);
                }
                shouldPermit = true;
            }
        }
        else
        {
            ::snprintf(contentInfoPath, sizeof(contentInfoPath), "/dev_hdd0/game/%s", init.titleId);
            shouldPermit = true;
        }

        if (shouldPermit)
        {
            cellSysutilCheckCallback();
            ret = cellGameContentPermit(contentInfoPath, usrdirPath);
            if (ret == 0)
            {
                ::strncpy(s_savePath, usrdirPath, sizeof(s_savePath));
                s_savePath[sizeof(s_savePath) - 1] = '\0';
                ::strcat(s_savePath, "/");
            }
        }
    }
#else
    // Desktop: all files relative to the working directory.
    s_userPath[0] = '\0';
#endif
}

const char *GetUserPath()
{
    return s_userPath;
}

bool IsJapanese()
{
    return s_isJapanese;
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
    if (strcmp(path, "log.txt") == 0)
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
#if defined(_WIN32) && !defined(__PS3__)
        if (_stricmp(dot, ".dat") == 0)
        {
            const char *p = path;
            const char *lastSep = strrchr(path, '/');
            if (strrchr(path, '\\') > lastSep) lastSep = strrchr(path, '\\');
            if (lastSep) p = lastSep + 1;
            if (_stricmp(p, "score.dat") == 0) return false;
            return true;
        }
#elif defined(__PS3__)
        if (::strlen(dot) == 4 && ::tolower(dot[1]) == 'd' && ::tolower(dot[2]) == 'a' && ::tolower(dot[3]) == 't')
        {
            const char *p = path;
            const char *lastSep = ::strrchr(path, '/');
            if (::strrchr(path, '\\') > lastSep) lastSep = ::strrchr(path, '\\');
            if (lastSep) p = lastSep + 1;
            if (::strcasecmp(p, "score.dat") == 0) return false;
            return true;
        }
#else
        if (strcasecmp(dot, ".dat") == 0)
        {
            const char *p = path;
            const char *lastSep = strrchr(path, '/');
            if (lastSep) p = lastSep + 1;
            if (strcasecmp(p, "score.dat") == 0) return false;
            return true;
        }
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
    if (IsAssetPath(path))
    {
        ::snprintf(outBuf, outBufSize, "%s%s", s_userPath, path);
    }
    else
    {
        ::snprintf(outBuf, outBufSize, "%s%s", s_savePath, path);
    }
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
