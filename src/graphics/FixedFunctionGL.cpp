#include "FixedFunctionGL.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"
#ifndef __PS3__
#include <SDL2/SDL.h>
#endif

void FixedFunctionGL::SetContextFlags()
{
#ifndef __PS3__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
}

GfxInterface *FixedFunctionGL::Init()
{
    g_GameErrorContext.Log("FixedFunctionGL::Init started.\n");
    SetContextFlags();

    FixedFunctionGL *self = nullptr;

#ifndef __PS3__
    SDL_Init(SDL_INIT_VIDEO);

    u32 flags = SDL_WINDOW_OPENGL;
    i32 height = GAME_WINDOW_HEIGHT_REAL;
    i32 width = GAME_WINDOW_WIDTH_REAL;
    i32 x = SDL_WINDOWPOS_UNDEFINED;
    i32 y = SDL_WINDOWPOS_UNDEFINED;

    if (g_Supervisor.cfg.windowed == 0)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    self = new FixedFunctionGL();

    SDL_Window *window = SDL_CreateWindow(TH_WINDOW_TITLE, x, y, width, height, flags);
    self->window = window;
    if (window == NULL)
    {
        delete self;
        return NULL;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    self->glContext = glContext;
    if (glContext == NULL)
    {
        delete self;
        return NULL;
    }

    if (SDL_GL_MakeCurrent(window, glContext) != 0)
    {
        delete self;
        return NULL;
    }

    SDL_GL_SetSwapInterval(1);
#else
    g_GameErrorContext.Log("FixedFunctionGL::Init (PS3 path) initializing PSGL...\n");
    PSGLinitOptions options;
    options.enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS | PSGL_INIT_HOST_MEMORY_SIZE;
    options.maxSPUs = 1;
    options.initializeSPUs = true;
    options.persistentMemorySize = 0;
    options.transientMemorySize = 0;
    options.errorConsole = 0;
    options.fifoSize = 0;
    options.hostMemorySize = 64 * 1024 * 1024;
    
    psglInit(&options);
    g_GameErrorContext.Log("PSGL initialized.\n");

    g_GameErrorContext.Log("Creating PSGL device...\n");
    PSGLdeviceParameters params;
    params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT | PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT | PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE | PSGL_DEVICE_PARAMETERS_BUFFERING_MODE | PSGL_DEVICE_PARAMETERS_RESC_ADJUST_ASPECT_RATIO;
    params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
    params.colorFormat = GL_ARGB_SCE;
    params.depthFormat = GL_DEPTH_COMPONENT16;
    params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;
    params.enable |= PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE;
    params.rescRatioMode = RESC_RATIO_MODE_FULLSCREEN;

    PSGLdevice* device = psglCreateDeviceExtended(&params);
    if (device) {
        GLuint w, h;
        psglGetDeviceDimensions(device, &w, &h);
        g_GameWindowWidthReal = w;
        g_GameWindowHeightReal = h;
        g_GameErrorContext.Log("PS3 Device Size: %ux%u\n", w, h);

        if ((g_GameWindowWidthReal * 3) > (g_GameWindowHeightReal * 4))
        {
            g_ViewportWidth = ((u32)((g_GameWindowHeightReal / 3.0f) * 4.0f));
            g_ViewportOffX = ((g_GameWindowWidthReal - g_ViewportWidth) / 2);
            g_ViewportHeight = g_GameWindowHeightReal;
            g_ViewportOffY = 0;
        }
        else if ((g_GameWindowWidthReal * 3) < (g_GameWindowHeightReal * 4))
        {
            g_ViewportWidth = g_GameWindowWidthReal;
            g_ViewportOffX = 0;
            g_ViewportHeight = ((u32)((g_GameWindowWidthReal / 4.0f) * 3.0f));
            g_ViewportOffY = ((g_GameWindowHeightReal - g_ViewportHeight) / 2);
        }
        else
        {
            g_ViewportWidth = g_GameWindowWidthReal;
            g_ViewportOffX = 0;
            g_ViewportHeight = g_GameWindowHeightReal;
            g_ViewportOffY = 0;
        }
        g_GameErrorContext.Log("PS3 Viewport: %ux%u at (%d, %d)\n", g_ViewportWidth, g_ViewportHeight, g_ViewportOffX, g_ViewportOffY);
    }
    if (!device) {
        g_GameErrorContext.Log("CRITICAL: psglCreateDeviceExtended failed!\n");
        return NULL;
    }
    g_GameErrorContext.Log("PSGL device created.\n");

    g_GameErrorContext.Log("Creating PSGL context...\n");
    PSGLcontext* glContext = psglCreateContext();
    if (!glContext) {
        g_GameErrorContext.Log("CRITICAL: psglCreateContext failed!\n");
        return NULL;
    }
    g_GameErrorContext.Log("PSGL context created.\n");

    g_GameErrorContext.Log("Making PSGL context current...\n");
    psglMakeCurrent(glContext, device);
    psglResetCurrentContext();
    g_GameErrorContext.Log("PSGL context made current and reset. Actual current context: %p\n", (void *)psglGetCurrentContext());

    g_GameErrorContext.Log("FixedFunctionGL::Init (PS3 path) creating instance...\n");
    self = new FixedFunctionGL();
    self->device = device;
    self->glContext = glContext;
    g_GameErrorContext.Log("FixedFunctionGL instance created at %p\n", (void *)self);
#endif

    g_GameErrorContext.Log("Resolving GL functions...\n");
    g_glFuncTable.ResolveFunctions(false);
    g_GameErrorContext.Log("GL functions resolved.\n");

#ifdef __PS3__
    g_GameErrorContext.Log("glEnable function pointer: %p\n", (void *)g_glFuncTable.glEnable);
    g_GameErrorContext.Log("Current PSGL context: %p\n", (void *)psglGetCurrentContext());
    if (g_glFuncTable.glEnable == NULL) {
        g_GameErrorContext.Log("CRITICAL: glEnable is NULL!\n");
    }
#endif

    g_GameErrorContext.Log("Init trace start\n");
    g_glFuncTable.glEnable(GL_TEXTURE_2D);
    self->m_textureEnabled = true;
    g_glFuncTable.glEnableClientState(GL_VERTEX_ARRAY);

#ifndef __PS3__
    g_glFuncTable.glEnable(GL_ALPHA_TEST);
    g_glFuncTable.glAlphaFunc(GL_GEQUAL, 4 / 255.0f);
#endif

    if (((g_Supervisor.cfg.opts >> GCOS_SUPPRESS_USE_OF_GOROUD_SHADING) & 1) == 1)
    {
        g_glFuncTable.glShadeModel(GL_FLAT);
    }

#ifndef __PS3__
    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_FOG) & 1) == 0)
    {
        g_glFuncTable.glEnable(GL_FOG);
    }
#endif

#ifndef __PS3__
    g_glFuncTable.glFogf(GL_FOG_DENSITY, 1.0f);
    g_glFuncTable.glFogf(GL_FOG_MODE, GL_LINEAR);
#endif

#ifdef __PS3__
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
#else
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
    g_GameErrorContext.Log("Init trace end\n");

    // Some basic state to ensure VP can be read
    self->SetViewport(0, 0, 640, 480);
    self->SetDepthRange(0.0f, 1.0f);

    g_GameErrorContext.Log("FixedFunctionGL::Init finished successfully.\n");
    return self;
}

void FixedFunctionGL::Exit()
{
#ifndef __PS3__
    if (this->glContext)
    {
        SDL_GL_DeleteContext(this->glContext);
        this->glContext = NULL;
    }
    if (this->window)
    {
        SDL_DestroyWindow(this->window);
        this->window = NULL;
    }
#else
    if (this->glContext)
    {
        psglDestroyContext(this->glContext);
        this->glContext = NULL;
    }
    if (this->device)
    {
        psglDestroyDevice(this->device);
        this->device = NULL;
    }
    psglExit();
#endif
}

void FixedFunctionGL::SetFogRange(f32 nearPlane, f32 farPlane)
{
#ifndef __PS3__
    g_glFuncTable.glFogf(GL_FOG_START, nearPlane);
    g_glFuncTable.glFogf(GL_FOG_END, farPlane);
#endif
}

void FixedFunctionGL::SetFogColor(ZunColor color)
{
#ifndef __PS3__
    GLfloat normalizedFogColor[4] = {((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f,
                                     (color & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f};

    g_glFuncTable.glFogfv(GL_FOG_COLOR, normalizedFogColor);
#endif
}

void FixedFunctionGL::ToggleVertexAttribute(u8 attr, bool enable)
{
    if (attr & VERTEX_ATTR_TEX_COORD)
    {
        m_textureEnabled = enable;
        // Arg 0 will be the texture is it's used, and diffuse otherwise. Arg 1 will always be diffuse
        if (enable)
        {
            g_glFuncTable.glEnable(GL_TEXTURE_2D);
#ifdef __PS3__
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
#endif
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
            g_glFuncTable.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        else
        {
            g_glFuncTable.glDisable(GL_TEXTURE_2D);
#ifdef __PS3__
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PRIMARY_COLOR);
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR);
            g_glFuncTable.glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        }
    }

    if (attr & VERTEX_ATTR_DIFFUSE)
    {
        if (enable)
        {
            g_glFuncTable.glEnableClientState(GL_COLOR_ARRAY);
        }
        else
        {
            g_glFuncTable.glDisableClientState(GL_COLOR_ARRAY);
        }
    }
}

void FixedFunctionGL::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr)
{
    switch (attr)
    {
    case VERTEX_ARRAY_POSITION:
        g_glFuncTable.glVertexPointer(3, GL_FLOAT, stride, ptr);
        break;
    case VERTEX_ARRAY_TEX_COORD:
        g_glFuncTable.glTexCoordPointer(2, GL_FLOAT, stride, ptr);
        break;
    case VERTEX_ARRAY_DIFFUSE:
        g_glFuncTable.glColorPointer(4, GL_UNSIGNED_BYTE, stride, ptr);
        break;
    }
}

void FixedFunctionGL::SetColorOp(TextureOpComponent component, ColorOp op)
{
    static const GLenum opEnums[3] = {GL_MODULATE, GL_ADD, GL_REPLACE};

    if (component > COMPONENT_ALPHA || op > COLOR_OP_REPLACE)
    {
        return;
    }

    GLenum componentEnum = component == COMPONENT_ALPHA ? GL_COMBINE_ALPHA : GL_COMBINE_RGB;

#ifdef __PS3__
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, componentEnum, opEnums[op]);

    if (component == COMPONENT_RGB)
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, m_textureEnabled ? GL_TEXTURE : GL_PRIMARY_COLOR);
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    }
    else
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, m_textureEnabled ? GL_TEXTURE : GL_PRIMARY_COLOR);
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    }
#else
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, componentEnum, opEnums[op]);
#endif
}

void FixedFunctionGL::SetTextureFactor(ZunColor factor)
{
    GLfloat tfactorColor[4] = {((factor >> 16) & 0xFF) / 255.0f, ((factor >> 8) & 0xFF) / 255.0f,
                               (factor & 0xFF) / 255.0f, ((factor >> 24) & 0xFF) / 255.0f};

    g_glFuncTable.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, tfactorColor);
}

void FixedFunctionGL::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix)
{
    // This is not going to work for modelview
    GLenum matrixEnum[4] = {GL_MODELVIEW, GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE};

    g_glFuncTable.glMatrixMode(matrixEnum[type]);
    g_glFuncTable.glLoadMatrixf((const GLfloat *)&matrix);
}

void FixedFunctionGL::Enable(Capabilities cap)
{
    switch (cap)
    {
    case CAPS_BLEND:
        g_glFuncTable.glEnable(GL_BLEND);
        break;
    case CAPS_DEPTH_TEST:
        g_glFuncTable.glEnable(GL_DEPTH_TEST);
        break;
    }
}

bool FixedFunctionGL::HasError()
{
    return g_glFuncTable.glGetError() != GL_NO_ERROR;
}

void FixedFunctionGL::SetBlendMode(BlendMode mode)
{
    if (mode == BLEND_INV_SRC_ALPHA)
    {
        g_glFuncTable.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        g_glFuncTable.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
}

void FixedFunctionGL::SetViewport(i32 x, i32 y, i32 width, i32 height)
{
    if (width <= 0 || height <= 0) {
        g_GameErrorContext.Log("WARNING: Invalid viewport size!\n");
        return;
    }
    m_viewport[0] = x;
    m_viewport[1] = y;
    m_viewport[2] = width;
    m_viewport[3] = height;
    g_glFuncTable.glViewport(x, y, width, height);
}

void FixedFunctionGL::GetViewport(u32 *viewport)
{
    g_glFuncTable.glGetIntegerv(GL_VIEWPORT, (GLint *)viewport);
#ifdef __PS3__
    if (viewport[2] == 0 || viewport[3] == 0) {
        std::memcpy(viewport, m_viewport, sizeof(m_viewport));
    }
#endif
}

void FixedFunctionGL::GetDepthRange(f32 *depthRange)
{
    g_glFuncTable.glGetFloatv(GL_DEPTH_RANGE, depthRange);
#ifdef __PS3__
    if (depthRange[0] == 0.0f && depthRange[1] == 0.0f) {
        std::memcpy(depthRange, m_depthRange, sizeof(m_depthRange));
    }
#endif
}

void FixedFunctionGL::SetClearColor(f32 r, f32 g, f32 b, f32 a)
{
    g_glFuncTable.glClearColor(r, g, b, a);
}

void FixedFunctionGL::SetTextureFilter()
{
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void FixedFunctionGL::SetClearDepth(f32 depth)
{
    g_glFuncTable.glClearDepthf(depth);
}

void FixedFunctionGL::Clear(u32 clearBits)
{
    GLbitfield mask = 0;

    if (clearBits & CLEAR_COLOR_BUFFER)
        mask |= GL_COLOR_BUFFER_BIT;
    if (clearBits & CLEAR_DEPTH_BUFFER)
        mask |= GL_DEPTH_BUFFER_BIT;

    g_glFuncTable.glClear(mask);
}

void FixedFunctionGL::SetDepthRange(f32 nearPlane, f32 farPlane)
{
    m_depthRange[0] = nearPlane;
    m_depthRange[1] = farPlane;
    g_glFuncTable.glDepthRangef(nearPlane, farPlane);
}

void FixedFunctionGL::SetDepthMask(bool enable)
{
    g_glFuncTable.glDepthMask(enable);
}

void FixedFunctionGL::SetDepthFunc(DepthFunc func)
{
    if (func == DEPTH_FUNC_ALWAYS)
    {
        g_glFuncTable.glDepthFunc(GL_ALWAYS);
    }
    else
    {
        g_glFuncTable.glDepthFunc(GL_LEQUAL);
    }
}

GfxTextureHandle FixedFunctionGL::CreateTexture()
{
    GLuint texture = 0;
    g_glFuncTable.glGenTextures(1, &texture);
    return texture;
}

void FixedFunctionGL::BindTexture(GfxTextureHandle handle)
{
    g_glFuncTable.glBindTexture(GL_TEXTURE_2D, handle);
}

void FixedFunctionGL::DeleteTexture(GfxTextureHandle handle)
{
    g_glFuncTable.glDeleteTextures(1, (GLuint *)&handle);
}

void FixedFunctionGL::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data)
{
    GLenum glFmt = GL_RGBA;
    GLenum glType = GL_UNSIGNED_BYTE;

    switch (fmt)
    {
    case PIXEL_RGBA:
#ifndef __PS3__
        glFmt = GL_RGBA;
#else
        glFmt = GL_ARGB_SCE;
#endif
        break;
    case PIXEL_RGB:
        glFmt = GL_RGB;
        break;
    }

    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        glType = GL_UNSIGNED_BYTE;
        break;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        glType = GL_UNSIGNED_SHORT_5_5_5_1;
        break;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        glType = GL_UNSIGNED_SHORT_5_6_5;
        break;
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        glType = GL_UNSIGNED_SHORT_4_4_4_4;
        break;
    }

    g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, glFmt, width, height, 0, glFmt, glType, data);
}

void FixedFunctionGL::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data)
{
#ifndef __PS3__
    g_glFuncTable.glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
#else
    g_glFuncTable.glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, GL_ARGB_SCE, GL_UNSIGNED_BYTE, data);
#endif
}

void FixedFunctionGL::ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels)
{
#ifndef __PS3__
    g_glFuncTable.glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (void *)pixels);
#else
    g_glFuncTable.glReadPixels(x, y, width, height, GL_ARGB_SCE, GL_UNSIGNED_BYTE, (void *)pixels);
#endif
}

void FixedFunctionGL::CopyTextureFromBackbuffer(i32 x, i32 y, i32 width, i32 height)
{
#ifndef __PS3__
    g_glFuncTable.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, width, height, 0);
#else
    g_glFuncTable.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB_SCE, x, y, width, height, 0);
#endif
}

void FixedFunctionGL::Draw(PrimitiveType type, i32 start, i32 count)
{
    GLenum glPrim = GL_TRIANGLES;

    switch (type)
    {
    case PRIM_TRIANGLE_STRIP:
        glPrim = GL_TRIANGLE_STRIP;
        break;
    case PRIM_TRIANGLES:
        glPrim = GL_TRIANGLES;
        break;
    }

    g_glFuncTable.glDrawArrays(glPrim, start, count);
#ifdef __PS3__
    g_glFuncTable.glFlush();
#else
    g_glFuncTable.glFlush();
    g_glFuncTable.glFinish();
#endif
}

void FixedFunctionGL::SwapBuffers()
{
    // g_GameErrorContext.Log("FixedFunctionGL::SwapBuffers started.\n");
#ifndef __PS3__
    SDL_GL_SwapWindow(window);
#else
    psglSwap();
#endif
}
