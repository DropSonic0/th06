#include "FixedFunctionGL.hpp"
#include "GLFunc.hpp"
#include "utils.hpp"

#include "Supervisor.hpp"

#ifdef __PS3__
extern "C" void FixedFunctionGL_SetContextFlags_Helper()
{
    FixedFunctionGL::SetContextFlags();
}

extern "C" GfxInterface *FixedFunctionGL_Init_Helper()
{
#ifdef __PS3__
    utils::Log("FixedFunctionGL_Init_Helper called.");
#endif
    return FixedFunctionGL::Init();
}
#endif
#ifndef __PS3__
#include <SDL.h>
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
#ifdef __PS3__
    utils::Log("FixedFunctionGL: Init start...");
    utils::Log("FixedFunctionGL: glEnable(GL_TEXTURE_2D)...");
    g_glFuncTable.glEnable(GL_TEXTURE_2D);
    utils::Log("FixedFunctionGL: glEnableClientState(GL_VERTEX_ARRAY)...");
    g_glFuncTable.glEnableClientState(GL_VERTEX_ARRAY);

    utils::Log("FixedFunctionGL: glEnable(GL_ALPHA_TEST)...");
    g_glFuncTable.glEnable(GL_ALPHA_TEST);
    utils::Log("FixedFunctionGL: glAlphaFunc...");
    g_glFuncTable.glAlphaFunc(GL_GEQUAL, 4 / 255.0f);

    if (((g_Supervisor.cfg.opts >> GCOS_SUPPRESS_USE_OF_GOROUD_SHADING) & 1) == 1)
    {
        utils::Log("FixedFunctionGL: glShadeModel(GL_FLAT)...");
        g_glFuncTable.glShadeModel(GL_FLAT);
    }

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_FOG) & 1) == 0)
    {
        utils::Log("FixedFunctionGL: glEnable(GL_FOG)...");
        g_glFuncTable.glEnable(GL_FOG);
    }

    utils::Log("FixedFunctionGL: glFogf(GL_FOG_DENSITY, 1.0f)...");
    g_glFuncTable.glFogf(GL_FOG_DENSITY, 1.0f);
    utils::Log("FixedFunctionGL: glFogf(GL_FOG_MODE, GL_LINEAR)...");
    g_glFuncTable.glFogf(GL_FOG_MODE, GL_LINEAR);

    utils::Log("FixedFunctionGL: glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE)...");
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
    {
        utils::Log("FixedFunctionGL: glTexEnvi(COMBINE_ALPHA, MODULATE)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    }
    else
    {
        utils::Log("FixedFunctionGL: glTexEnvi(COMBINE_ALPHA, REPLACE)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    }

    utils::Log("FixedFunctionGL: glTexEnvi(OPERAND0_ALPHA, SRC_ALPHA)...");
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        utils::Log("FixedFunctionGL: glTexEnvi(SRC1_ALPHA, CONSTANT)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_CONSTANT);
    }
    else
    {
        utils::Log("FixedFunctionGL: glTexEnvi(SRC1_ALPHA, PRIMARY_COLOR)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
    }

    utils::Log("FixedFunctionGL: glTexEnvi(OPERAND1_ALPHA, SRC_ALPHA)...");
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
    {
        utils::Log("FixedFunctionGL: glTexEnvi(COMBINE_RGB, MODULATE)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    }
    else
    {
        utils::Log("FixedFunctionGL: glTexEnvi(COMBINE_RGB, REPLACE)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    }

    utils::Log("FixedFunctionGL: glTexEnvi(OPERAND0_RGB, SRC_COLOR)...");
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        utils::Log("FixedFunctionGL: glTexEnvi(SRC1_RGB, CONSTANT)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
    }
    else
    {
        utils::Log("FixedFunctionGL: glTexEnvi(SRC1_RGB, PRIMARY_COLOR)...");
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
    }

    utils::Log("FixedFunctionGL: glTexEnvi(OPERAND1_RGB, SRC_COLOR)...");
    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

    utils::Log("FixedFunctionGL: Creating new FixedFunctionGL...");
#else
    g_glFuncTable.glEnable(GL_TEXTURE_2D);
    g_glFuncTable.glEnableClientState(GL_VERTEX_ARRAY);

    g_glFuncTable.glEnable(GL_ALPHA_TEST);
    g_glFuncTable.glAlphaFunc(GL_GEQUAL, 4 / 255.0f);

    if (((g_Supervisor.cfg.opts >> GCOS_SUPPRESS_USE_OF_GOROUD_SHADING) & 1) == 1)
    {
        g_glFuncTable.glShadeModel(GL_FLAT);
    }

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_FOG) & 1) == 0)
    {
        g_glFuncTable.glEnable(GL_FOG);
    }

    g_glFuncTable.glFogf(GL_FOG_DENSITY, 1.0f);
    g_glFuncTable.glFogf(GL_FOG_MODE, GL_LINEAR);

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    }
    else
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    }

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_CONSTANT);
    }
    else
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
    }

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    }
    else
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    }

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT);
    }
    else
    {
        g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
    }

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
#endif
    return new FixedFunctionGL();
}

void FixedFunctionGL::SetFogRange(f32 nearPlane, f32 farPlane)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glFogf(START/END, %f, %f)...", nearPlane, farPlane);
#endif
    g_glFuncTable.glFogf(GL_FOG_START, nearPlane);
    g_glFuncTable.glFogf(GL_FOG_END, farPlane);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glFogf done.");
#endif
}

void FixedFunctionGL::SetFogColor(ZunColor color)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glFogfv(COLOR, 0x%x)...", color);
#endif
    GLfloat normalizedFogColor[4] = {((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f,
                                     (color & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f};

    g_glFuncTable.glFogfv(GL_FOG_COLOR, normalizedFogColor);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glFogfv done.");
#endif
}

void FixedFunctionGL::ToggleVertexAttribute(u8 attr, bool enable)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: ToggleVertexAttribute(attr=0x%x, enable=%d)...", attr, enable);
#endif
    if (attr & VERTEX_ATTR_TEX_COORD)
    {
        // Arg 0 will be the texture is it's used, and diffuse otherwise. Arg 1 will always be diffuse
        if (enable)
        {
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
            g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
            g_glFuncTable.glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        }
        else
        {
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
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: ToggleVertexAttribute done.");
#endif
}

void FixedFunctionGL::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetAttributePointer(attr=%d, stride=%d, ptr=%p)...", attr, stride, ptr);
#endif
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
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetAttributePointer done.");
#endif
}

void FixedFunctionGL::SetColorOp(TextureOpComponent component, ColorOp op)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetColorOp(component=%d, op=%d)...", component, op);
#endif
    const GLenum opEnums[3] = {GL_MODULATE, GL_ADD, GL_REPLACE};

    if (component > COMPONENT_ALPHA || op > COLOR_OP_REPLACE)
    {
        return;
    }

    GLenum componentEnum = component == COMPONENT_ALPHA ? GL_COMBINE_ALPHA : GL_COMBINE_RGB;

    g_glFuncTable.glTexEnvi(GL_TEXTURE_ENV, componentEnum, opEnums[op]);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetColorOp done.");
#endif
}

void FixedFunctionGL::SetTextureFactor(ZunColor factor)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glTexEnvfv(TFAC, 0x%x)...", factor);
#endif
    GLfloat tfactorColor[4] = {((factor >> 16) & 0xFF) / 255.0f, ((factor >> 8) & 0xFF) / 255.0f,
                               (factor & 0xFF) / 255.0f, ((factor >> 24) & 0xFF) / 255.0f};

    g_glFuncTable.glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, tfactorColor);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetTextureFactor done.");
#endif
}

void FixedFunctionGL::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetTransformMatrix(type=%d)...", type);
#endif
    // This is not going to work for modelview
    GLenum matrixEnum[4] = {GL_MODELVIEW, GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE};

    g_glFuncTable.glMatrixMode(matrixEnum[type]);
    
#ifdef __PS3__
    // Rule out alignment issues
    float alignedMatrix[16];
    memcpy(alignedMatrix, &matrix, sizeof(alignedMatrix));
    g_glFuncTable.glLoadMatrixf((const GLfloat *)alignedMatrix);
#else
    g_glFuncTable.glLoadMatrixf((const GLfloat *)&matrix);
#endif

#ifdef __PS3__
    // utils::Log("FixedFunctionGL: SetTransformMatrix done.");
#endif
}

void FixedFunctionGL::SetDepthMask(bool enable)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glDepthMask(%d)...", enable);
#endif
    g_glFuncTable.glDepthMask(enable);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glDepthMask done.");
#endif
}

void FixedFunctionGL::SetDepthFunc(DepthFunc func)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glDepthFunc(%d)...", func);
#endif
    if (func == DEPTH_FUNC_ALWAYS)
    {
        g_glFuncTable.glDepthFunc(GL_ALWAYS);
    }
    else
    {
        g_glFuncTable.glDepthFunc(GL_LEQUAL);
    }
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glDepthFunc done.");
#endif
}

void FixedFunctionGL::Clear(ZunColor color)
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glClear(0x%x)...", color);
#endif
    g_glFuncTable.glClearColor(((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f,
                               (color & 0xFF) / 255.0f, ((color >> 24) & 0xFF) / 255.0f);
    g_glFuncTable.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glClear done.");
#endif
}

void FixedFunctionGL::Draw()
{
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glDrawArrays(TRIANGLE_STRIP, 0, 4)...");
#endif
    g_glFuncTable.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#ifdef __PS3__
    // utils::Log("FixedFunctionGL: glDrawArrays done.");
#endif
}
