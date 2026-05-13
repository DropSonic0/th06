#include "TextHelper.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"

#ifndef __PS3__
#include <SDL_ttf.h>
#else
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../3rdparty/imgui/imstb_truetype.h"
#include "../th06.ttc.h"
#endif
#include <algorithm>
#include <cstring>
#include "ShittyIconv.hpp"
#include "GamePaths.hpp"


#ifndef __PS3__
static TTF_Font *g_Font, *g_Font2;
#else
static stbtt_fontinfo g_StbFont;
#endif

bool textNotExist;

TextHelper::TextHelper()
{
}

TextHelper::~TextHelper()
{
#ifndef __PS3__
    TTF_Quit();
#endif
}

#define TEXT_BUFFER_HEIGHT 64

#ifdef __PS3__
// Helper to decode UTF-8 to UTF-32
static const char* utf8_to_codepoint(const char* p, u32* out_cp) {
    u8 c = (u8)*p;
    if (c < 0x80) {
        *out_cp = (u32)c;
        return p + 1;
    } else if (c < 0xe0) {
        *out_cp = (u32)(((c & 0x1f) << 6) | ((p[1] & 0x3f)));
        return p + 2;
    } else if (c < 0xf0) {
        *out_cp = (u32)(((c & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f));
        return p + 3;
    } else {
        *out_cp = (u32)(((c & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f));
        return p + 4;
    }
}
#endif

// Extended to initialize all globals for text helper
ZunResult TextHelper::CreateTextBuffer()
{
#ifndef __PS3__
    bool usePath2;
    const char* path;
    const char* path2;
    const PixelFormatSDL1* fmt;
#ifdef __ANDROID__
    string resolvedPath;
    string resolvedPath2;
#endif

    TTF_Init();

    // Primary font is MSゴシック, which is nonfree and has to be taken from a Windows install
    // Fallback is Noto Sans Regular (JP) which is redistributable
    usePath2 = false;
    #ifdef __ANDROID__
    resolvedPath = string(GamePaths::GetUserPath()) + string("th06.ttc");
    resolvedPath2 = string(GamePaths::GetUserPath()) + string("th06.ttf");
    path=resolvedPath.c_str();
    path2=resolvedPath2.c_str();
    #else
    path="th06.ttc";
    path2="th06.ttf";
    #endif    
    if (g_Font = TTF_OpenFont(path, 30), g_Font == NULL)
    {
        if (g_Font = TTF_OpenFont(path2, 30), g_Font == NULL)
        {
            textNotExist = true;
            return ZUN_SUCCESS;
        }else{
            usePath2 = true;
        }
    }
    if(!textNotExist){
        if(usePath2){
            g_Font2 = TTF_OpenFont(path2, 30);
        }else{
            g_Font2 = TTF_OpenFont(path, 32);
        }
    }
    fmt = &SDL1_PIXELFORMAT_RGBA32;

    g_TextBufferSurface = SDL_CreateRGBSurface(
        0,
        GAME_WINDOW_WIDTH, TEXT_BUFFER_HEIGHT,
        fmt->bpp,
        fmt->rmask,
        fmt->gmask,
        fmt->bmask,
        fmt->amask
    );

    return ZUN_SUCCESS;
#else
    if (!stbtt_InitFont(&g_StbFont, th06_ttc, 0)) {
        textNotExist = true;
        return ZUN_SUCCESS;
    }
    g_TextBufferSurface = (void*)(new u8[GAME_WINDOW_WIDTH * TEXT_BUFFER_HEIGHT * 4]);
    memset(g_TextBufferSurface, 0, GAME_WINDOW_WIDTH * TEXT_BUFFER_HEIGHT * 4);
    return ZUN_SUCCESS;
#endif
}

bool TextHelper::InvertAlpha(i32 x, i32 y, i32 spriteWidth, i32 fontHeight)
{
    i32 i, j;
    u8* pixels;

    if(textNotExist) return true;

#ifndef __PS3__
    SDL_LockSurface(g_TextBufferSurface);
    pixels = (u8*)g_TextBufferSurface->pixels;
#else
    pixels = (u8*)g_TextBufferSurface;
#endif

    // In D3D EoSD this function mostly inverts the alpha, but on A1R5G5B5 surfaces specifically it also
    //   creates a gradient. D3D EoSD will always attempt to create an A1R5G5B5 surface for the text buffer,
    //   will only attempt use other formats as a fallback, and in those cases the text will be bugged anyway.
    //   As part of the port from GDI to SDL_ttf, we've converted the text buffer surface to always be RGBA32
    //   and no longer need the alpha inversion, but we still want that gradient to be applied

    for (j = 0; j < fontHeight; j++)
    {
        for (i = 0; i < spriteWidth; i++)
        {
            u8* p = &pixels[((y + j) * GAME_WINDOW_WIDTH + (x + i)) * 4];
            if (p[3]) // A
            {
                // Apply vertical gradient. j / fontHeight goes from 0 to 1.
                p[0] = p[0] - (u8)((i32)p[0] * j / fontHeight / 2); // R
                p[1] = p[1] - (u8)((i32)p[1] * j / fontHeight / 2); // G
                p[2] = p[2] - (u8)((i32)p[2] * j / fontHeight / 4); // B
            }
        }
    }

#ifndef __PS3__
    SDL_UnlockSurface(g_TextBufferSurface);
#endif

    return true;
}

// Text strings in asset files are encoded using Shift_JIS. This allows RenderTextToTexture to handle both UTF-8 and
// Shift_JIS. This also does not check for overlong encoding, but that shouldn't matter
bool isUTF8Encoded(const char *string)
{
    bool isMultiByteParse;
    int codepointLen;
    if(textNotExist) return true;
#define UTF8_1BYTE_MASK 0x80
#define UTF8_2BYTE_MASK 0xE0
#define UTF8_3BYTE_MASK 0xF0
#define UTF8_4BYTE_MASK 0xF8

#define UTF8_2NDBYTE_MASK 0xC0

// 0xxx xxxx
#define UTF8_1BYTE_PREFIX 0x00
// 110x xxxx
#define UTF8_2BYTE_PREFIX 0xC0
// 1110 xxxx
#define UTF8_3BYTE_PREFIX 0xE0
// 1111 0xxx
#define UTF8_4BYTE_PREFIX 0xF0

// 10xx xxxx
#define UTF8_2NDBYTE_PREFIX 0x80

    isMultiByteParse = false;
    codepointLen = 0;

    while (*string != '\0')
    {
        unsigned char c = *(unsigned char *)string;

        if (!isMultiByteParse)
        {
            if ((c & UTF8_1BYTE_MASK) != UTF8_1BYTE_PREFIX)
            {
                isMultiByteParse = true;

                if ((c & UTF8_2BYTE_MASK) == UTF8_2BYTE_PREFIX)
                    codepointLen = 1;
                else if ((c & UTF8_3BYTE_MASK) == UTF8_3BYTE_PREFIX)
                    codepointLen = 2;
                else if ((c & UTF8_4BYTE_MASK) == UTF8_4BYTE_PREFIX)
                    codepointLen = 3;
                else
                    return false;
            }
        }
        else
        {
            if ((c & UTF8_2NDBYTE_MASK) != UTF8_2NDBYTE_PREFIX)
                return false;

            if (--codepointLen == 0)
                isMultiByteParse = false;
        }

        string++;
    }

    return true;

#undef UTF8_1BYTE_MASK
#undef UTF8_2BYTE_MASK
#undef UTF8_3BYTE_MASK
#undef UTF8_4BYTE_MASK

#undef UTF8_2NDBYTE_MASK

#undef UTF8_1BYTE_PREFIX
#undef UTF8_2BYTE_PREFIX
#undef UTF8_3BYTE_PREFIX
#undef UTF8_4BYTE_PREFIX

#undef UTF8_2NDBYTE_PREFIX
}

#ifndef __PS3__
void SurfaceOverwriteBlend(SDL_Surface *srcSurface, SDL_Surface *dstSurface, u32 x)
{
    u32 *srcData;
    u8 *dstData;
    if(textNotExist) return;

    SDL_LockSurface(srcSurface);
    SDL_LockSurface(dstSurface);

    srcData = (u32 *)srcSurface->pixels;
    dstData = (u8 *)dstSurface->pixels;

    for (int i = 0; i < srcSurface->h; i++)
    {
        for (int j = 0; j < srcSurface->w; j++)
        {
            if ((srcData[j] & 0xFF000000) != 0)
            {
                dstData[i * dstSurface->pitch + (x + j) * 4] = (srcData[j] >> 16) & 0xFF;
                dstData[i * dstSurface->pitch + (x + j) * 4 + 1] = (srcData[j] >> 8) & 0xFF;
                dstData[i * dstSurface->pitch + (x + j) * 4 + 2] = srcData[j] & 0xFF;
                dstData[i * dstSurface->pitch + (x + j) * 4 + 3] = (srcData[j] >> 24) & 0xFF;
            }
        }

        srcData += srcSurface->pitch / 4;
    }

    SDL_UnlockSurface(dstSurface);
    SDL_UnlockSurface(srcSurface);
}
#endif

void TextHelper::RenderTextToTexture(i32 xPos, i32 yPos, i32 spriteWidth, i32 spriteHeight, i32 fontHeight,
                                     i32 fontWidth, ZunColor textColor, ZunColor shadowColor, const char *string,
                                     TextureData *outTexture)
{
    char convertedText[1024];
#ifndef __PS3__
    SDL_Rect finalCopyDst;
    SDL_Rect finalCopySrc;
    SDL_Rect shadowRect;
    SDL_Rect textRect;
    const PixelFormatSDL1* fmt;
    SDL_Surface *textureSurface;
#else
    u8* pixels;
    int pass, curX, w, h, x0, y0, x1, y1, i, j, dstX, dstY, srcX, srcY, advance, lsb;
    int srcW, srcH, dstW, dstH;
    float scale;
    int ascent, descent, lineGap, baseline;
    const char* p_text;
    ZunColor color;
    int xOff, yOff;
    u8 a_col, r_col, g_col, b_col;
    u32 cp;
    u8 temp_bitmap[64 * 64];
    u8 alpha_val;
    u8* dst_px_ptr;
    u8* src_px_ptr;
#endif

    if(textNotExist) return;
    
    memset(convertedText, 0, sizeof(convertedText));

    if (!isUTF8Encoded(string))
    {
        char outputUtf[1024];
        memset(outputUtf, 0, sizeof(outputUtf));
    	sjis_to_utf8(string, (size_t)strlen(string), outputUtf);
        strncpy(convertedText, outputUtf, sizeof(convertedText));
    }
    else
    {
        strncpy(convertedText, string, sizeof(convertedText));
    }

#ifndef __PS3__
    // TTF_SetFontSize(g_Font, fontHeight * 2);

    finalCopySrc.x = 0;
    finalCopySrc.y = 0;
    finalCopySrc.w = spriteWidth * 2 - 2;
    finalCopySrc.h = fontHeight * 2 - 2;

    SDL_FillRect(g_TextBufferSurface, &finalCopySrc, 0);

    if (shadowColor != COLOR_WHITE)
    {
        SDL_Surface *shadowText;

        // Render shadow.
        SDL_Color sdlShadowColor;
        sdlShadowColor.b = (shadowColor >> 16) & 0xFF;
        sdlShadowColor.g = (shadowColor >> 8) & 0xFF;
        sdlShadowColor.r = shadowColor & 0xFF;

        shadowText = TTF_RenderUTF8_Blended(fontHeight==15?g_Font:g_Font2, convertedText, sdlShadowColor);

        if (shadowText != NULL)
        {
            shadowRect.x = xPos * 2 + 3;
            shadowRect.y = 2;
            shadowRect.w = shadowText->w;
            shadowRect.h = shadowText->h;

            // SDL_SetSurfaceBlendMode(shadowText, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(shadowText, NULL, g_TextBufferSurface, &shadowRect);

            SDL_FreeSurface(shadowText);
        }
    }

    SDL_Color sdlTextColor;
    sdlTextColor.b = (textColor >> 16) & 0xFF;
    sdlTextColor.g = (textColor >> 8) & 0xFF;
    sdlTextColor.r = textColor & 0xFF;

    SDL_Surface *regularText = TTF_RenderUTF8_Blended(fontHeight==15?g_Font:g_Font2, convertedText, sdlTextColor);

    if (regularText != NULL)
    {
        textRect.x = xPos * 2;
        textRect.y = 0;
        textRect.w = regularText->w;
        textRect.h = regularText->h;

        SurfaceOverwriteBlend(regularText, g_TextBufferSurface, xPos * 2);

        SDL_FreeSurface(regularText);
    }

    // Once we get an API abstraction layer for surface operations, this needs to change
    //   We really shouldn't be clobbering the texture format
    if (!outTexture->textureData || outTexture->format != TEX_FMT_A8R8G8B8)
    {
        free(outTexture->textureData);
        outTexture->textureData = (u8 *)malloc(outTexture->width * outTexture->height * 4);
        memset(outTexture->textureData, 0, outTexture->width * outTexture->height * 4);
    }

    outTexture->format = TEX_FMT_A8R8G8B8;
    fmt = &SDL1_PIXELFORMAT_RGBA32;

    textureSurface = SDL_CreateRGBSurfaceFrom(
        outTexture->textureData,
        outTexture->width,
        outTexture->height,
        fmt->bpp,
        outTexture->width * 4,
        fmt->rmask,
        fmt->gmask,
        fmt->bmask,
        fmt->amask
    );
    
    InvertAlpha(0, 0, spriteWidth * 2, fontHeight * 2 + 6);

    finalCopyDst.x = 0;
    finalCopyDst.y = yPos;
    finalCopyDst.w = spriteWidth;
    finalCopyDst.h = 16;

    if (SDL_SoftStretch(g_TextBufferSurface, &finalCopySrc, textureSurface, &finalCopyDst) < 0)
    {
    }

    g_AnmManager->SetCurrentTexture(outTexture->handle);

    g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outTexture->width, outTexture->height, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, outTexture->textureData);

    SDL_FreeSurface(textureSurface);
#else
    pixels = (u8*)g_TextBufferSurface;
    memset(pixels, 0, GAME_WINDOW_WIDTH * TEXT_BUFFER_HEIGHT * 4);

    scale = stbtt_ScaleForPixelHeight(&g_StbFont, (float)fontHeight * 2.0f);
    stbtt_GetFontVMetrics(&g_StbFont, &ascent, &descent, &lineGap);
    baseline = (int)((float)ascent * scale);

    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            if (shadowColor == COLOR_WHITE) continue;
            color = shadowColor;
            xOff = 3;
            yOff = 2;
        } else {
            color = textColor;
            xOff = 0;
            yOff = 0;
        }

        a_col = (u8)((color >> 24) & 0xFF);
        if (a_col == 0) a_col = 255; // Handle colors with missing alpha
        r_col = (u8)((color >> 16) & 0xFF);
        g_col = (u8)((color >> 8) & 0xFF);
        b_col = (u8)(color & 0xFF);
        p_text = convertedText;
        curX = xOff;
        while (*p_text) {
            p_text = utf8_to_codepoint(p_text, &cp);
            stbtt_GetCodepointBitmapBox(&g_StbFont, (int)cp, scale, scale, &x0, &y0, &x1, &y1);
            w = x1 - x0;
            h = y1 - y0;
            if (w > 0 && h > 0) {
                if (w * h <= (int)sizeof(temp_bitmap)) {
                    stbtt_MakeCodepointBitmap(&g_StbFont, temp_bitmap, w, h, w, scale, scale, (int)cp);
                    for (j = 0; j < h; j++) {
                        dstY = baseline + y0 + j + yOff;
                        if (dstY < 0 || dstY >= TEXT_BUFFER_HEIGHT) continue;
                        for (i = 0; i < w; i++) {
                            dstX = curX + x0 + i;
                            if (dstX < 0 || dstX >= GAME_WINDOW_WIDTH) continue;
                            alpha_val = (u8)((u32)temp_bitmap[j * w + i] * a_col / 255);
                            if (alpha_val > 0) {
                                dst_px_ptr = &pixels[(dstY * GAME_WINDOW_WIDTH + dstX) * 4];
                                // Store as RGBA for consistency with InvertAlpha
                                dst_px_ptr[0] = r_col;
                                dst_px_ptr[1] = g_col;
                                dst_px_ptr[2] = b_col;
                                dst_px_ptr[3] = alpha_val;
                            }
                        }
                    }
                }
            }
            stbtt_GetCodepointHMetrics(&g_StbFont, (int)cp, &advance, &lsb);
            curX += (int)((float)advance * scale);
        }
    }

    if (!outTexture->textureData || outTexture->format != TEX_FMT_A8R8G8B8) {
        delete[] outTexture->textureData;
        outTexture->textureData = new u8[outTexture->width * outTexture->height * 4];
        memset(outTexture->textureData, 0, outTexture->width * outTexture->height * 4);
    }
    outTexture->format = TEX_FMT_A8R8G8B8;

    InvertAlpha(0, 0, spriteWidth * 2, fontHeight * 2 + 6);

    // Simple nearest-neighbor scale from g_TextBufferSurface to outTexture->textureData
    srcW = spriteWidth * 2;
    srcH = fontHeight * 2;
    dstW = spriteWidth;
    dstH = 16;
    if (srcW > GAME_WINDOW_WIDTH) srcW = GAME_WINDOW_WIDTH;
    for (j = 0; j < dstH; j++) {
        srcY = j * srcH / dstH;
        if (srcY >= TEXT_BUFFER_HEIGHT) continue;
        if (yPos + j >= (int)outTexture->height) continue;
        for (i = 0; i < dstW; i++) {
            if (xPos + i >= (int)outTexture->width) continue;
            srcX = i * srcW / dstW;
            if (srcX >= GAME_WINDOW_WIDTH) continue;
            src_px_ptr = &pixels[(srcY * GAME_WINDOW_WIDTH + srcX) * 4];
            dst_px_ptr = &outTexture->textureData[((yPos + j) * outTexture->width + (xPos + i)) * 4];

            // Source is now RGBA, and we want to upload as GL_RGBA
            memcpy(dst_px_ptr, src_px_ptr, 4);
        }
    }

    g_AnmManager->SetCurrentTexture(outTexture->handle);
    g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outTexture->width, outTexture->height, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, outTexture->textureData);
#endif

    return;
}

// Extended to free all globals for text helper
void TextHelper::ReleaseTextBuffer()
{
    if(textNotExist) return;
#ifndef __PS3__
    if (g_Font != NULL)
    {
        TTF_CloseFont(g_Font);
        g_Font = NULL;
    }
    if (g_Font2 != NULL)
    {
        TTF_CloseFont(g_Font2);
        g_Font2 = NULL;
    }

    if (g_TextBufferSurface != NULL)
    {
        SDL_FreeSurface(g_TextBufferSurface);
        g_TextBufferSurface = NULL;
    }
#else
    if (g_TextBufferSurface != NULL) {
        delete[] (u8*)g_TextBufferSurface;
        g_TextBufferSurface = NULL;
    }
#endif
    
    return;
}
