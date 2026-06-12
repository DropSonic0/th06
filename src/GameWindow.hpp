#pragma once

#ifndef __PS3__
#include <SDL2/SDL_video.h>
#else
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#endif

#include "ZunResult.hpp"
#include "graphics/GfxInterface.hpp"
#include "inttypes.hpp"

// The internal resolution EoSD uses. 640x480. I can't think of any reason anyone sane
//   would want to change this
#define GAME_WINDOW_WIDTH (640)
#define GAME_WINDOW_HEIGHT (480)

// The actual resolution used for the output window and viewport scaling
//   At some point there should be a method to change this without recompiling but for now
//   this'll do
extern i32 g_GameWindowWidthReal;
extern i32 g_GameWindowHeightReal;

extern u32 g_ViewportWidth;
extern i32 g_ViewportOffX;
extern u32 g_ViewportHeight;
extern i32 g_ViewportOffY;

#define GAME_WINDOW_WIDTH_REAL g_GameWindowWidthReal
#define GAME_WINDOW_HEIGHT_REAL g_GameWindowHeightReal

#define VIEWPORT_WIDTH g_ViewportWidth
#define VIEWPORT_OFF_X g_ViewportOffX
#define VIEWPORT_HEIGHT g_ViewportHeight
#define VIEWPORT_OFF_Y g_ViewportOffY

#define WIDTH_RESOLUTION_SCALE (((f32)VIEWPORT_WIDTH) / GAME_WINDOW_WIDTH)
#define HEIGHT_RESOLUTION_SCALE (((f32)VIEWPORT_HEIGHT) / GAME_WINDOW_HEIGHT)

enum RenderResult
{
    RENDER_RESULT_KEEP_RUNNING,
    RENDER_RESULT_EXIT_SUCCESS,
    RENDER_RESULT_EXIT_ERROR,
};

struct GameWindow
{
    RenderResult Render();
    static void Present();

    static void CreateGameWindow();
    static ZunResult InitD3dRendering();
    static void InitD3dDevice();

    i32 isAppClosing;
    i32 lastActiveAppValue;
    i32 isAppActive;
    u8 curFrame;
    i32 screenSaveActive;
    i32 lowPowerActive;
    i32 powerOffActive;
    u32 renderBackendIndex;
};

extern GameWindow g_GameWindow;
extern i32 g_TickCountToEffectiveFramerate;
extern double g_LastFrameTime;
extern GfxInterface *g_GfxBackend;