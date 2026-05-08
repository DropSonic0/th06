#ifndef __PS3__
#include <SDL.h>
#else
#include <sys/process.h>
#include <PSGL/psgl.h>
#include <sysutil/sysutil_common.h>
#endif
#include <stdio.h>

#include "AnmManager.hpp"
#include "Chain.hpp"
#include "Controller.hpp"
#include "FileSystem.hpp"
#include "GameErrorContext.hpp"
#include "GamePaths.hpp"
#include "GameWindow.hpp"
#include "MidiOutput.hpp"
#include "SoundPlayer.hpp"
#include "Stage.hpp"
#include "Supervisor.hpp"
#include "TextHelper.hpp"
#include "ZunResult.hpp"
#include "i18n.hpp"
#include "utils.hpp"
#define dlog utils::Log

#ifdef __PS3__
SYS_PROCESS_PARAM(1001, 0x100000)
#endif

int main(int argc, char *argv[])
{
    GamePaths::Init();

    dlog("Starting");
    i32 renderResult = 0;

#ifdef __ANDROID__
    // On Android, SDL must be initialized before GamePaths::Init()
    // because SDL_AndroidGetInternalStoragePath() requires SDL_Init.
    if (SDL_Init(0) < 0)
    {
        return 1;
    }
#endif

    dlog("Init Gamepath done");

    // if (utils::CheckForRunningGameInstance())
    // {
    //     g_GameErrorContext.Flush();

    //     return 1;
    // }

    dlog("Load CONF File");
    if (g_Supervisor.LoadConfig(TH_CONFIG_FILE) != ZUN_SUCCESS)
    {
#ifdef __ANDROID__
        // On Android, config file may not exist on first run.
        // LoadConfig sets defaults and tries to write — if write fails,
        // continue anyway with defaults.
        SDL_Log("LoadConfig failed (first run?), continuing with defaults");
#else
        g_GameErrorContext.Flush();
        return -1;
#endif
    }

    // if (GameWindow::InitD3dInterface())
    // {
    //     g_GameErrorContext.Flush();
    //     return 1;
    // }
    dlog("Start the game");

restart:
    dlog("Create game window");
    GameWindow::CreateGameWindow();
#ifdef __PS3__
    if (g_GameWindow.device == NULL || g_GameWindow.glContext == NULL)
    {
        dlog("GameWindow creation failed. Exiting...");
        return 1;
    }
#endif

    dlog("new AnmManager");
    g_AnmManager = new AnmManager();

    dlog("InitD3dRendering");
    if (GameWindow::InitD3dRendering())
    {
        g_GameErrorContext.Flush();
        return 1;
    }

    dlog("InitializeDSound");
    g_SoundPlayer.InitializeDSound();
    dlog("GetJoystickCaps");
    Controller::GetJoystickCaps();
    dlog("ResetKeyboard");
    Controller::ResetKeyboard();

    dlog("Supervisor::RegisterChain");
    if (Supervisor::RegisterChain() != ZUN_SUCCESS)
    {
        goto stop;
    }
#ifndef __PS3__
    if (!g_Supervisor.cfg.windowed)
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
#endif

    g_GameWindow.curFrame = 0;

    dlog("Into loop game event");
    bool firstRender = true;
    while (true)
    {
#ifndef __PS3__
        SDL_Event e;

        //dlog("Into poolevent loop");
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                goto stop;
            }
        }
#else
        cellSysutilCheckCallback();
#endif

        if (firstRender)
        {
            dlog("g_GameWindow.Render (first time)...");
            firstRender = false;
        }
        renderResult = g_GameWindow.Render();
        if (renderResult != 0)
        {
            break;
        }

        //        SDL_Delay(1000.0f / 60.0f);

        //        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        //        {
        //            TranslateMessage(&msg);
        //            DispatchMessage(&msg);
        //        }
        //        else
        //        {
        //            testCoopLevelRes = g_Supervisor.d3dDevice->TestCooperativeLevel();
        //            if (testCoopLevelRes == D3D_OK)
        //            {
        //                renderResult = g_GameWindow.Render();
        //                if (renderResult != 0)
        //                {
        //                    goto stop;
        //                }
        //            }
        //            else if (testCoopLevelRes == D3DERR_DEVICENOTRESET)
        //            {
        //                g_AnmManager->ReleaseSurfaces();
        //                testResetRes = g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);
        //                if (testResetRes != 0)
        //                {
        //                    goto stop;
        //                }
        //                GameWindow::InitD3dDevice();
        //                g_Supervisor.unk198 = 3;
        //            }
        //        }
    }


stop:
    dlog("stop the game");
    g_Chain.Release();
    g_SoundPlayer.Release();

    delete g_AnmManager;
    g_AnmManager = NULL;

    // Clean up GL resources while the context is still valid.
    // THPrac::THPracGuiShutdown();
    // {
    //     SDL_GLContext ctx = g_Renderer ? g_Renderer->glContext : nullptr;
    //     if (g_Renderer)
    //         g_Renderer->Release();
    //     if (ctx)
    //         SDL_GL_DeleteContext(ctx);
    // }

#ifndef __PS3__
    SDL_DestroyWindow(g_GameWindow.window);
    SDL_GL_DeleteContext(g_GameWindow.glContext);
#else
    psglDestroyContext(g_GameWindow.glContext);
    psglDestroyDevice(g_GameWindow.device);
    psglExit();
#endif

    if (renderResult == 2)
    {
        // Clean up resources that leak across restart cycles.
        // We cannot call Supervisor::DeletedCallback() here because
        // ReleasePbg3() has a built-in double-free (calls Release() then
        // delete which calls Release() again) that crashes on modern heaps.
        // PBG3 archives are re-released internally by LoadPbg3() on reload,
        // so only these three resources actually leak:
        if (g_Supervisor.midiOutput != NULL)
        {
            g_Supervisor.midiOutput->StopPlayback();
            delete g_Supervisor.midiOutput;
            g_Supervisor.midiOutput = NULL;
        }
        TextHelper::ReleaseTextBuffer();
        // Controller::CloseSDLController();

        g_GameErrorContext.ResetContext();

        GameErrorContext::Log(&g_GameErrorContext, TH_ERR_OPTION_CHANGED_RESTART);

#ifndef __PS3__
        if (!g_Supervisor.cfg.windowed)
        {
            SDL_ShowCursor(SDL_ENABLE);
        }
#endif
        goto restart;
    }

#ifdef __PS3__
    GameConfiguration swappedCfgFinal = g_Supervisor.cfg;
    swappedCfgFinal.version = utils::Swap32(swappedCfgFinal.version);
    swappedCfgFinal.opts = utils::Swap32(swappedCfgFinal.opts);
    swappedCfgFinal.padXAxis = (i16)utils::Swap16((u16)swappedCfgFinal.padXAxis);
    swappedCfgFinal.padYAxis = (i16)utils::Swap16((u16)swappedCfgFinal.padYAxis);
    
    i16* swappedMappingFinal = (i16*)&swappedCfgFinal.controllerMapping;
    for (int i = 0; i < sizeof(ControllerMapping) / 2; ++i) {
        swappedMappingFinal[i] = (i16)utils::Swap16((u16)swappedMappingFinal[i]);
    }
    FileSystem::WriteDataToFile(TH_CONFIG_FILE, &swappedCfgFinal, sizeof(g_Supervisor.cfg));
#else
    FileSystem::WriteDataToFile(TH_CONFIG_FILE, &g_Supervisor.cfg, sizeof(g_Supervisor.cfg));
#endif

#ifndef __PS3__
    SDL_ShowCursor(SDL_ENABLE);
#endif
    // GameErrorContext::Log(&g_GameErrorContext, TH_ERR_LOGGER_END);
    g_GameErrorContext.Flush();
#ifndef __PS3__
    SDL_Quit();
#endif
    return 0;
}
