#include "TextHelper.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "graphics/GLFunc.hpp"
#include "i18n.hpp"

#include "thirdparty/sjis_converter.h"

#ifndef __PS3__
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#else
#include <cstdlib>
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/imstb_truetype.h"
#include "font/msgothic.ttc.h"
#endif
#include <algorithm>
#include <cstring>

#ifndef __PS3__
static TTF_Font *g_Font;
#else
static stbtt_fontinfo g_Font;
#endif

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
#define PS3_TEXT_BUFFER_WIDTH 1280
#define PS3_TEXT_BUFFER_HEIGHT 128
#endif

// Extended to initialize all globals for text helper
ZunResult TextHelper::CreateTextBuffer()
{
    g_GameErrorContext.Log("TextHelper::CreateTextBuffer started.\n");
#ifndef __PS3__
    TTF_Init();

    // Primary font is MSゴシック, which is nonfree and has to be taken from a Windows install
    // Fallback is Noto Sans Regular (JP) which is redistributable
    if ((g_Font = TTF_OpenFont(TH_PRIMARY_FONT_FILENAME, 10), g_Font == NULL) &&
        (std::printf("%s\n", TTF_GetError()), g_Font = TTF_OpenFont(TH_FALLBACK_FONT_FILENAME, 10), g_Font == NULL))
    {
        std::printf("%s\n", TTF_GetError());

        g_GameErrorContext.Fatal(TH_ERR_FONTS_NOT_FOUND);
        return ZUN_ERROR;
    }

    g_TextBufferSurface =
        SDL_CreateRGBSurfaceWithFormat(0, GAME_WINDOW_WIDTH, TEXT_BUFFER_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);

    SDL_SetSurfaceBlendMode(g_TextBufferSurface, SDL_BLENDMODE_NONE);
#else
    int offset = stbtt_GetFontOffsetForIndex(msgothic_ttc, 0);
    g_GameErrorContext.Log("stbtt_GetFontOffsetForIndex returned %d\n", offset);
    if (offset < 0 || stbtt_InitFont(&g_Font, msgothic_ttc, offset) == 0)
    {
        g_GameErrorContext.Fatal(TH_ERR_FONTS_NOT_FOUND);
        return ZUN_ERROR;
    }
    g_GameErrorContext.Log("stbtt_InitFont finished.\n");

    g_TextBufferSurface = std::malloc(PS3_TEXT_BUFFER_WIDTH * PS3_TEXT_BUFFER_HEIGHT * 4);
#endif

    return ZUN_SUCCESS;
}

bool TextHelper::InvertAlpha(i32 x, i32 y, i32 spriteWidth, i32 fontHeight)
{
    u8 *bufferStart;
    i32 pitch;
    i32 limitHeight;

#ifndef __PS3__
    SDL_LockSurface(g_TextBufferSurface);
    bufferStart = (u8 *)g_TextBufferSurface->pixels;
    pitch = g_TextBufferSurface->pitch;
    limitHeight = TEXT_BUFFER_HEIGHT;
#else
    bufferStart = (u8 *)g_TextBufferSurface;
    pitch = PS3_TEXT_BUFFER_WIDTH * 4;
    limitHeight = PS3_TEXT_BUFFER_HEIGHT;
#endif

    // In D3D EoSD this function mostly inverts the alpha, but on A1R5G5B5 surfaces specifically it also
    //   creates a gradient. D3D EoSD will always attempt to create an A1R5G5B5 surface for the text buffer,
    //   will only attempt use other formats as a fallback, and in those cases the text will be bugged anyway.
    //   As part of the port from GDI to SDL_ttf, we've converted the text buffer surface to always be RGBA32
    //   and no longer need the alpha inversion, but we still want that gradient to be applied

    for (i32 row = 0; row < fontHeight; row++)
    {
        if (y + row < 0 || y + row >= limitHeight)
            continue;
        u8 *rowPtr = bufferStart + (y + row) * pitch;
        for (i32 col = 0; col < spriteWidth; col++)
        {
            if (x + col < 0 || (x + col) * 4 >= pitch)
                continue;
            u8 *pixel = rowPtr + (x + col) * 4;
#ifndef __PS3__
            if (pixel[3]) // A (RGBA)
            {
                // Purely vertical gradient to match original game look
                pixel[0] = pixel[0] - pixel[0] * row / fontHeight / 2; // R
                pixel[1] = pixel[1] - pixel[1] * row / fontHeight / 2; // G
                pixel[2] = pixel[2] - pixel[2] * row / fontHeight / 4; // B
            }
#else
            if (pixel[0]) // A (ARGB)
            {
                // Purely vertical gradient to match original game look
                pixel[1] = pixel[1] - pixel[1] * row / fontHeight / 2; // R
                pixel[2] = pixel[2] - pixel[2] * row / fontHeight / 2; // G
                pixel[3] = pixel[3] - pixel[3] * row / fontHeight / 4; // B
            }
#endif
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

    bool isMultiByteParse = false;
    int codepointLen = 0;

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

#ifdef __PS3__
static u32 DecodeUTF8(const char **s)
{
    u32 c = (unsigned char)**s;
    if (c < 0x80)
    {
        (*s)++;
        return c;
    }
    if ((c & 0xE0) == 0xC0)
    {
        c = ((c & 0x1F) << 6) | ((unsigned char)(*s)[1] & 0x3F);
        *s += 2;
        return c;
    }
    if ((c & 0xF0) == 0xE0)
    {
        c = ((c & 0x0F) << 12) | (((unsigned char)(*s)[1] & 0x3F) << 6) | ((unsigned char)(*s)[2] & 0x3F);
        *s += 3;
        return c;
    }
    if ((c & 0xF8) == 0xF0)
    {
        c = ((c & 0x07) << 18) | (((unsigned char)(*s)[1] & 0x3F) << 12) | (((unsigned char)(*s)[2] & 0x3F) << 6) |
            ((unsigned char)(*s)[3] & 0x3F);
        *s += 4;
        return c;
    }
    (*s)++;
    return 0;
}

struct CachedGlyph {
    u8* bitmap;
    int w, h;
    int xoff, yoff;
    int advance;
};

static void PS3_RenderText(const char *text, int fontHeight, u32 color, int xPos, int yPos)
{
    float scale = stbtt_ScaleForPixelHeight(&g_Font, (float)fontHeight);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_Font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);

    u8 red = (color >> 16) & 0xFF;
    u8 green = (color >> 8) & 0xFF;
    u8 blue = color & 0xFF;

    const char *p = text;
    int x = xPos;
    
    // Static cache for the current string to avoid re-rendering glyphs in multi-pass
    static CachedGlyph cache[256];
    static int cacheCount = 0;
    static char lastText[1024] = {0};
    static int lastFontHeight = 0;

    if (std::strcmp(text, lastText) != 0 || fontHeight != lastFontHeight) {
        // Clear old cache
        for (int i = 0; i < cacheCount; i++) {
            std::free(cache[i].bitmap);
        }
        cacheCount = 0;
        std::strncpy(lastText, text, 1023);
        lastFontHeight = fontHeight;

        const char* p2 = text;
        while (*p2 && cacheCount < 256) {
            u32 codepoint = DecodeUTF8(&p2);
            if (codepoint == 0) break;

            int advance, lsb;
            stbtt_GetCodepointHMetrics(&g_Font, codepoint, &advance, &lsb);
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&g_Font, codepoint, scale, scale, &x0, &y0, &x1, &y1);

            int w = x1 - x0;
            int h = y1 - y0;
            cache[cacheCount].advance = (int)(advance * scale);
            cache[cacheCount].xoff = (int)(lsb * scale);
            cache[cacheCount].yoff = y0;
            cache[cacheCount].w = w;
            cache[cacheCount].h = h;
            if (w > 0 && h > 0) {
                cache[cacheCount].bitmap = (u8*)std::malloc(w * h);
                stbtt_MakeCodepointBitmap(&g_Font, cache[cacheCount].bitmap, w, h, w, scale, scale, codepoint);
            } else {
                cache[cacheCount].bitmap = nullptr;
            }
            cacheCount++;
        }
    }

    for (int i = 0; i < cacheCount; i++) {
        CachedGlyph& g = cache[i];
        int out_x = x + g.xoff;
        int out_y = yPos + baseline + g.yoff;

        if (g.bitmap) {
            for (int j = 0; j < g.h; j++) {
                int py = out_y + j;
                if (py < 0 || py >= PS3_TEXT_BUFFER_HEIGHT) continue;
                u8 *rowDst = ((u8 *)g_TextBufferSurface) + py * PS3_TEXT_BUFFER_WIDTH * 4;
                for (int k = 0; k < g.w; k++) {
                    int px = out_x + k;
                    if (px < 0 || px >= PS3_TEXT_BUFFER_WIDTH) continue;
                    u8 alpha = g.bitmap[j * g.w + k];
                    if (alpha > 0) {
                        u8 *dst = rowDst + px * 4;
                        u8 oldA = dst[0];
                        u32 newA = alpha + (u32)oldA * (255 - alpha) / 255;
                        if (newA > 0) {
                            dst[1] = (u8)((red * alpha + (u32)dst[1] * oldA * (255 - alpha) / 255) / newA);
                            dst[2] = (u8)((green * alpha + (u32)dst[2] * oldA * (255 - alpha) / 255) / newA);
                            dst[3] = (u8)((blue * alpha + (u32)dst[3] * oldA * (255 - alpha) / 255) / newA);
                            dst[0] = (u8)newA;
                        }
                    }
                }
            }
        }
        x += g.advance;
    }
}

static void PS3_ScaleSurface(u8 *src, int srcW, int srcH, int srcPitch, u8 *dst, int dstW, int dstH, int dstPitch)
{
    for (int y = 0; y < dstH; y++)
    {
        for (int x = 0; x < dstW; x++)
        {
            int srcX = x * 2;
            int srcY = y * 2;

            u32 r = 0, g = 0, b = 0, a = 0;
            int count = 0;
            for (int dy = 0; dy < 2; dy++)
            {
                for (int dx = 0; dx < 2; dx++)
                {
                    int sx = srcX + dx;
                    int sy = srcY + dy;
                    if (sx < srcW && sy < srcH)
                    {
                        u8 *p = src + sy * srcPitch + sx * 4;
                        u32 alpha = p[0];
                        a += alpha;
                        r += (u32)p[1] * alpha;
                        g += (u32)p[2] * alpha;
                        b += (u32)p[3] * alpha;
                        count++;
                    }
                }
            }
            u8 *dp = dst + y * dstPitch + x * 4;
            if (count > 0)
            {
                u32 avgA = a / 4;
                dp[0] = (u8)avgA;
                if (avgA > 0) {
                    dp[1] = (u8)(r / a);
                    dp[2] = (u8)(g / a);
                    dp[3] = (u8)(b / a);
                } else {
                    dp[1] = dp[2] = dp[3] = 0;
                }
            }
        }
    }
}
#endif

#ifndef __PS3__
void SurfaceOverwriteBlend(SDL_Surface *srcSurface, SDL_Surface *dstSurface, u32 x)
{
    // Source surface is A8R8G8B8
    // Dest surface is RGBA32
    // We want to overwrite dest unless source has alpha 0

    SDL_LockSurface(srcSurface);
    SDL_LockSurface(dstSurface);

    u32 *srcData = (u32 *)srcSurface->pixels;
    u8 *dstData = (u8 *)dstSurface->pixels;

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
#ifdef __PS3__
    static bool logged = false;
    if (!logged) {
        std::printf("DEBUG: TextHelper::RenderTextToTexture PS3 path active\n");
        logged = true;
    }
#endif
    char convertedText[1024];
#ifndef __PS3__
    SDL_Rect finalCopyDst;
    SDL_Rect finalCopySrc;
    SDL_Rect shadowRect;
    SDL_Rect textRect;
#endif

    if (!isUTF8Encoded(string))
    {
        char *utf8 = sjis2utf8(string);
        strcpy(convertedText, utf8);
        free(utf8);
    }
    else
    {
        strcpy(convertedText, string);
    }

#ifndef __PS3__
    TTF_SetFontSize(g_Font, fontHeight * 2);

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
        sdlShadowColor.a = 0xFF;
        sdlShadowColor.b = (shadowColor >> 16) & 0xFF;
        sdlShadowColor.g = (shadowColor >> 8) & 0xFF;
        sdlShadowColor.r = shadowColor & 0xFF;

        shadowText = TTF_RenderUTF8_Blended(g_Font, convertedText, sdlShadowColor);

        if (shadowText != NULL)
        {
            shadowRect.x = xPos * 2 + 3;
            shadowRect.y = 2;
            shadowRect.w = shadowText->w;
            shadowRect.h = shadowText->h;

            SDL_SetSurfaceBlendMode(shadowText, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(shadowText, NULL, g_TextBufferSurface, &shadowRect);

            SDL_FreeSurface(shadowText);
        }
    }

    SDL_Color sdlTextColor;
    sdlTextColor.a = 0xFF;
    sdlTextColor.b = (textColor >> 16) & 0xFF;
    sdlTextColor.g = (textColor >> 8) & 0xFF;
    sdlTextColor.r = textColor & 0xFF;

    SDL_Surface *regularText = TTF_RenderUTF8_Blended(g_Font, convertedText, sdlTextColor);

    if (regularText != NULL)
    {
        textRect.x = xPos * 2;
        textRect.y = 0;
        textRect.w = regularText->w;
        textRect.h = regularText->h;

        SurfaceOverwriteBlend(regularText, g_TextBufferSurface, xPos * 2);

        SDL_FreeSurface(regularText);
    }
#else
    std::memset(g_TextBufferSurface, 0, PS3_TEXT_BUFFER_WIDTH * PS3_TEXT_BUFFER_HEIGHT * 4);

    // Thick outline: diamond pattern, 2px distance at 2x scale = 1px at 1x scale
    for (int dy = -2; dy <= 2; dy++)
    {
        for (int dx = -2; dx <= 2; dx++)
        {
            if (dx * dx + dy * dy <= 5 && (dx != 0 || dy != 0))
            {
                PS3_RenderText(convertedText, fontHeight * 2, COLOR_BLACK, xPos * 2 + dx, dy + 2);
            }
        }
    }

    if (shadowColor != COLOR_WHITE)
    {
        // Render shadow. Original is roughly (1, 1) offset. At 2x scale, it's (2, 2).
        PS3_RenderText(convertedText, fontHeight * 2, shadowColor, xPos * 2 + 2, 2 + 2);
    }

    PS3_RenderText(convertedText, fontHeight * 2, textColor, xPos * 2, 2);
#endif

    // Once we get an API abstraction layer for surface operations, this needs to change
    //   We really shouldn't be clobbering the texture format
    if (!outTexture->textureData || outTexture->format != TEX_FMT_A8R8G8B8)
    {
        free(outTexture->textureData);
        outTexture->textureData = (u8 *)malloc(outTexture->width * outTexture->height * 4);
        memset(outTexture->textureData, 0, outTexture->width * outTexture->height * 4);
        outTexture->format = TEX_FMT_A8R8G8B8;
    }

#ifndef __PS3__
    SDL_Surface *textureSurface = SDL_CreateRGBSurfaceWithFormatFrom(
        outTexture->textureData, outTexture->width, outTexture->height, SDL_BITSPERPIXEL(SDL_PIXELFORMAT_RGBA32),
        outTexture->width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32), SDL_PIXELFORMAT_RGBA32);
#endif

    // Apply vertical gradient only to the text area (offset by 2 pixels vertically)
    InvertAlpha(std::max(0, xPos * 2 - 2), 2, spriteWidth * 2 + 4, fontHeight * 2);

#ifndef __PS3__
    finalCopyDst.x = 0;
    finalCopyDst.y = yPos;
    finalCopyDst.w = spriteWidth;
    finalCopyDst.h = 16;

    if (SDL_SoftStretchLinear(g_TextBufferSurface, &finalCopySrc, textureSurface, &finalCopyDst) < 0)
    {
        SDL_Log("SDL_BlitScaled failed! Error: %s", SDL_GetError());
    }
#else
    // Adjust scaling to include the outlines (+-2px) and the 2px vertical shift
    // We scale from 2x back to 1x.
    int srcX = std::max(0, xPos * 2 - 2);
    int dstX = srcX / 2;
    PS3_ScaleSurface((u8 *)g_TextBufferSurface + (srcX * 4), spriteWidth * 2 + 4, fontHeight * 2 + 6,
                     PS3_TEXT_BUFFER_WIDTH * 4, outTexture->textureData + (yPos * outTexture->width + dstX) * 4,
                     spriteWidth + 2, (fontHeight * 2 + 6) / 2, outTexture->width * 4);
#endif

    g_AnmManager->SetCurrentTexture(outTexture->handle);

    g_GfxBackend->SetTextureImage(outTexture->width, outTexture->height, PIXEL_RGBA, PIXEL_UNSIGNED_BYTE,
                                  outTexture->textureData);

#ifdef __PS3__
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#endif

#ifndef __PS3__
    SDL_FreeSurface(textureSurface);
#endif

    return;
}

// Extended to free all globals for text helper
void TextHelper::ReleaseTextBuffer()
{
#ifndef __PS3__
    if (g_Font != NULL)
    {
        TTF_CloseFont(g_Font);
        g_Font = NULL;
    }

    if (g_TextBufferSurface != NULL)
    {
        SDL_FreeSurface(g_TextBufferSurface);
        g_TextBufferSurface = NULL;
    }
#else
    if (g_TextBufferSurface != NULL)
    {
        std::free(g_TextBufferSurface);
        g_TextBufferSurface = NULL;
    }
#endif

    return;
}
