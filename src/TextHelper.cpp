#include "TextHelper.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
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
    //    this->format = (D3DFORMAT)-1;
    //    this->width = 0;
    //    this->height = 0;
    //    this->hdc = 0;
    //    this->gdiObj2 = 0;
    //    this->gdiObj = 0;
    //    this->buffer = NULL;
}

TextHelper::~TextHelper()
{
#ifndef __PS3__
    TTF_Quit();
#endif
}

#define TEXT_BUFFER_HEIGHT 64

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

    g_TextBufferSurface = std::malloc(GAME_WINDOW_WIDTH * TEXT_BUFFER_HEIGHT * 4);
#endif

    return ZUN_SUCCESS;
}

bool TextHelper::InvertAlpha(i32 x, i32 y, i32 spriteWidth, i32 fontHeight)
{
    u8 *bufferCursor;
    i32 gradientArea;
    i32 i = 0;

    gradientArea = spriteWidth * fontHeight;

#ifndef __PS3__
    SDL_LockSurface(g_TextBufferSurface);
    bufferCursor = (u8 *)g_TextBufferSurface->pixels;
#else
    bufferCursor = (u8 *)g_TextBufferSurface;
#endif

    // In D3D EoSD this function mostly inverts the alpha, but on A1R5G5B5 surfaces specifically it also
    //   creates a gradient. D3D EoSD will always attempt to create an A1R5G5B5 surface for the text buffer,
    //   will only attempt use other formats as a fallback, and in those cases the text will be bugged anyway.
    //   As part of the port from GDI to SDL_ttf, we've converted the text buffer surface to always be RGBA32
    //   and no longer need the alpha inversion, but we still want that gradient to be applied

    for (; i < gradientArea; i++, bufferCursor += 4)
    {
        if (bufferCursor[3]) // A
        {
            bufferCursor[0] = bufferCursor[0] - bufferCursor[0] * i / gradientArea / 2; // R
            bufferCursor[1] = bufferCursor[1] - bufferCursor[1] * i / gradientArea / 2; // G
            bufferCursor[2] = bufferCursor[2] - bufferCursor[2] * i / gradientArea / 4; // B
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

static void PS3_RenderText(const char *text, int fontHeight, u32 color, int xPos, int yPos)
{
    float scale = stbtt_ScaleForPixelHeight(&g_Font, (float)fontHeight);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_Font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);

    u8 r = color & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = (color >> 16) & 0xFF;

    const char *p = text;
    int x = xPos;
    while (*p)
    {
        u32 codepoint = DecodeUTF8(&p);
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&g_Font, codepoint, &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&g_Font, codepoint, scale, scale, &x0, &y0, &x1, &y1);

        int out_x = x + (int)(lsb * scale);
        int out_y = yPos + baseline + y0;
        int width = x1 - x0;
        int height = y1 - y0;

        if (width > 0 && height > 0)
        {
            u8 *bitmap = (u8 *)std::malloc(width * height);
            stbtt_MakeCodepointBitmap(&g_Font, bitmap, width, height, width, scale, scale, codepoint);

            for (int j = 0; j < height; j++)
            {
                if (out_y + j < 0 || out_y + j >= TEXT_BUFFER_HEIGHT)
                    continue;
                for (int i = 0; i < width; i++)
                {
                    if (out_x + i < 0 || out_x + i >= GAME_WINDOW_WIDTH)
                        continue;
                    u8 alpha = bitmap[j * width + i];
                    if (alpha > 0)
                    {
                        u8 *dst = ((u8 *)g_TextBufferSurface) + ((out_y + j) * GAME_WINDOW_WIDTH + (out_x + i)) * 4;
                        dst[0] = r;
                        dst[1] = g;
                        dst[2] = b;
                        dst[3] = alpha;
                    }
                }
            }
            std::free(bitmap);
        }
        x += (int)(advance * scale);
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
    std::memset(g_TextBufferSurface, 0, GAME_WINDOW_WIDTH * TEXT_BUFFER_HEIGHT * 4);

    if (shadowColor != COLOR_WHITE)
    {
        // Render shadow.
        PS3_RenderText(convertedText, fontHeight * 2, shadowColor, xPos * 2 + 3, 2);
    }

    PS3_RenderText(convertedText, fontHeight * 2, textColor, xPos * 2, 0);
#endif

    // Once we get an API abstraction layer for surface operations, this needs to change
    //   We really shouldn't be clobbering the texture format
    if (!outTexture->textureData || outTexture->format != TEX_FMT_A8R8G8B8)
    {
        free(outTexture->textureData);
        outTexture->textureData = (u8 *)malloc(outTexture->width * outTexture->height * 4);
        memset(outTexture->textureData, 0, outTexture->width * outTexture->height * 4);
    }

    outTexture->format = TEX_FMT_A8R8G8B8;
#ifndef __PS3__
    SDL_Surface *textureSurface = SDL_CreateRGBSurfaceWithFormatFrom(
        outTexture->textureData, outTexture->width, outTexture->height, SDL_BITSPERPIXEL(SDL_PIXELFORMAT_RGBA32),
        outTexture->width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32), SDL_PIXELFORMAT_RGBA32);
#endif

    InvertAlpha(0, 0, spriteWidth * 2, fontHeight * 2 + 6);

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
    // TODO: implement PS3 surface scaling/copying
    // For now just copy the top-left of g_TextBufferSurface to outTexture->textureData at yPos
    u8 *src = (u8 *)g_TextBufferSurface;
    u8 *dst = outTexture->textureData + yPos * outTexture->width * 4;
    for (int i = 0; i < 16; i++)
    {
        std::memcpy(dst + i * outTexture->width * 4, src + i * GAME_WINDOW_WIDTH * 4, spriteWidth * 4);
    }
#endif

    g_AnmManager->SetCurrentTexture(outTexture->handle);

    g_GfxBackend->SetTextureImage(outTexture->width, outTexture->height, PIXEL_RGBA, PIXEL_UNSIGNED_BYTE,
                                  outTexture->textureData);

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
