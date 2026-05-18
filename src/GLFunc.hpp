#pragma once

#if defined(__PS3__) || defined(__CELLOS_LV2__) || defined(__PPU__) || defined(__ppu__) || defined(__SNC__) || defined(SN_TARGET_PS3)
#ifndef __PS3__
#define __PS3__
#endif
#endif

#ifndef __PS3__
#include <SDL_opengl.h>
#else
#include <PSGL/psgl.h>
#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif
#endif

// Function pointers for OpenGL functions used in EoSD. This is necessary because Windows
//   opengl32 only goes up to OpenGL 1.1 and some of the blending parameters we need are
//   from 1.3. Resolving function addresses at runtime using SDL_GL_GetProcAddress gets
//   around that restriction. Plus not directly linking the GL library is good for flexibility
//   in general, even on UNIX
struct GLFuncTable
{
    void ResolveFunctions(bool glesContext);

    // Functions where arguments use doubles in OpenGL and floats in GLES and therefore need manual dispatch
    void glClearDepthf(GLclampf depth);
    void glDepthRangef(GLclampf near_val, GLclampf far_val);

    // Function pointers for functions shared between GL and GLES
    void (GLAPIENTRY *glAlphaFunc)(GLenum func, GLclampf ref);
    void (GLAPIENTRY *glBindTexture)(GLenum target, GLuint texture);
    void (GLAPIENTRY *glBindBuffer)(GLenum target, GLuint buffer);
    void (GLAPIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
    void (GLAPIENTRY *glBufferData)(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
    void (GLAPIENTRY *glClear)(GLbitfield mask);
    void (GLAPIENTRY *glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    void (GLAPIENTRY *glColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
    void (GLAPIENTRY *glDeleteTextures)(GLsizei n, const GLuint *textures);
    void (GLAPIENTRY *glDeleteBuffers)(GLsizei n, const GLuint *buffers);
    void (GLAPIENTRY *glDepthFunc)(GLenum func);
    void (GLAPIENTRY *glDepthMask)(GLboolean flag);
    void (GLAPIENTRY *glDisable)(GLenum cap);
    void (GLAPIENTRY *glDisableClientState)(GLenum cap);
    void (GLAPIENTRY *glDrawArrays)(GLenum mode, GLint first, GLsizei count);
    void (GLAPIENTRY *glEnable)(GLenum cap);
    void (GLAPIENTRY *glEnableClientState)(GLenum cap);
    void (GLAPIENTRY *glFinish)(void);
    void (GLAPIENTRY *glFlush)(void);
    void (GLAPIENTRY *glFogf)(GLenum pname, GLfloat param);
    void (GLAPIENTRY *glFogfv)(GLenum pname, const GLfloat *params);
    void (GLAPIENTRY *glFogi)(GLenum pname, GLint param);
    void (GLAPIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
    void (GLAPIENTRY *glGenBuffers)(GLsizei n, GLuint *buffers);
    GLenum (GLAPIENTRY *glGetError)(void);
    void (GLAPIENTRY *glGetFloatv)(GLenum pname, GLfloat *params);
    void (GLAPIENTRY *glGetIntegerv)(GLenum pname, GLint *params);
    void (GLAPIENTRY *glHint)(GLenum target, GLenum mode);
    void (GLAPIENTRY *glLoadIdentity)(void);
    void (GLAPIENTRY *glLoadMatrixf)(const GLfloat *m);
    void (GLAPIENTRY *glMatrixMode)(GLenum mode);
    void (GLAPIENTRY *glMultMatrixf)(const GLfloat *m);
    void (GLAPIENTRY *glPopMatrix)(void);
    void (GLAPIENTRY *glPushMatrix)(void);
    void (GLAPIENTRY *glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                  GLvoid *pixels);
    void (GLAPIENTRY *glShadeModel)(GLenum mode);
    void (GLAPIENTRY *glTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
    void (GLAPIENTRY *glTexEnvfv)(GLenum target, GLenum pname, const GLfloat *params);
    void (GLAPIENTRY *glTexEnvi)(GLenum target, GLenum pname, GLint param);
    void (GLAPIENTRY *glTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
                                  GLint border, GLenum format, GLenum type, const GLvoid *pixels);
    void (GLAPIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);
    void (GLAPIENTRY *glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                      GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
    void (GLAPIENTRY *glVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
    void (GLAPIENTRY *glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    // GL(ES) 2.X / WebGL
#ifndef __PS3__
    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLDELETEPROGRAMPROC glDeleteProgram;
    PFNGLDELETESHADERPROC glDeleteShader;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLUNIFORM1FPROC glUniform1f;
    PFNGLUNIFORM1IPROC glUniform1i;
    PFNGLUNIFORM4FPROC glUniform4f;
    PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
    PFNGLUSEPROGRAMPROC glUseProgram;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
#endif

  private:
    // GLES forms for cases where they're different
    void (GLAPIENTRY *glClearDepthf_ptr)(GLclampf depth);
    void (GLAPIENTRY *glDepthRangef_ptr)(GLclampf near_val, GLclampf far_val);

    // GL forms for cases where they're different
#ifndef __PS3__
    void (GLAPIENTRY *glClearDepth)(GLclampd depth);
    void (GLAPIENTRY *glDepthRange)(GLclampd near_val, GLclampd far_val);
#else
    void (GLAPIENTRY *glClearDepth)(GLclampf depth);
    void (GLAPIENTRY *glDepthRange)(GLclampf near_val, GLclampf far_val);
#endif

    bool isGlesContext;
};

extern GLFuncTable g_glFuncTable;
