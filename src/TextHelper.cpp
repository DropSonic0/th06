#include "TextHelper.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"

#ifndef __PS3__
#include <SDL_ttf.h>
#else
#include <cstdlib>
#define STB_TRUETYPE_IMPLEMENTATION
#include "../3rdparty/imgui/imstb_truetype.h"
#include "font/msgothic.ttc.h"
#endif
#include <algorithm>
#include <cstring>
#include "ShittyIconv.hpp"
#include "GamePaths.hpp"


#ifndef __PS3__
TTF_Font *g_Font;
#else
static stbtt_fontinfo g_Font;
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
    this->ReleaseBuffer();
}

bool TextHelper::ReleaseBuffer()
{
    return true;
}

#define TEXT_BUFFER_HEIGHT 64

// Extended to initialize all globals for text helper
ZunResult TextHelper::CreateTextBuffer()
{
#ifndef __PS3__
    TTF_Init();

    // Primary font is MSゴシック, which is nonfree and has to be taken from a Windows install
    // Fallback is Noto Sans Regular (JP) which is redistributable
    #ifdef __ANDROID__
    std::string resolvedPath = std::string(GamePaths::GetUserPath()) + std::string("th06.ttc");
    if (g_Font = TTF_OpenFont(resolvedPath.c_str(), 10), g_Font == NULL)
    {
        std::printf("%s\n", TTF_GetError());
        // GameErrorContext::Fatal(&g_GameErrorContext, TH_ERR_FONTS_NOT_FOUND);
        textNotExist = true;
        return ZUN_SUCCESS;
    }
    #else
    if (g_Font = TTF_OpenFont("msgothic.ttc.h", 10), g_Font == NULL)
    {
        std::printf("%s\n", TTF_GetError());

        // GameErrorContext::Fatal(&g_GameErrorContext, TH_ERR_FONTS_NOT_FOUND);
        textNotExist = true;
        return ZUN_SUCCESS;
    }
    #endif

    g_TextBufferSurface =
        SDL_CreateRGBSurfaceWithFormat(0, GAME_WINDOW_WIDTH, TEXT_BUFFER_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);

    SDL_SetSurfaceBlendMode(g_TextBufferSurface, SDL_BLENDMODE_NONE);
#else
    int font_offset = stbtt_GetFontOffsetForIndex(msgothic_ttc, 0);
    if (font_offset < 0) font_offset = 0;
    if (!stbtt_InitFont(&g_Font, msgothic_ttc, font_offset))
    {
        textNotExist = true;
        return ZUN_SUCCESS;
    }
    // PS3: Allocate 2x buffer for supersampling (1280x128)
    g_TextBufferSurface = malloc(1280 * TEXT_BUFFER_HEIGHT * 2 * 4);
    if (g_TextBufferSurface) {
        memset(g_TextBufferSurface, 0, 1280 * TEXT_BUFFER_HEIGHT * 2 * 4);
    }
#endif

    return ZUN_SUCCESS;
}

bool TextHelper::InvertAlpha(i32 x, i32 y, i32 spriteWidth, i32 fontHeight)
{
    if(textNotExist) return true;
    u8 *bufferCursor;
    i32 gradientArea;

    gradientArea = spriteWidth * fontHeight;

#ifndef __PS3__
    SDL_LockSurface(g_TextBufferSurface);
    bufferCursor = (u8 *)g_TextBufferSurface->pixels;
#else
    bufferCursor = (u8 *)g_TextBufferSurface;
#endif

    if (!bufferCursor) return true;

    // In D3D EoSD this function mostly inverts the alpha, but on A1R5G5B5 surfaces specifically it also
    //   creates a gradient. D3D EoSD will always attempt to create an A1R5G5B5 surface for the text buffer,
    //   will only attempt use other formats as a fallback, and in those cases the text will be bugged anyway.
    //   As part of the port from GDI to SDL_ttf, we've converted the text buffer surface to always be RGBA32
    //   and no longer need the alpha inversion, but we still want that gradient to be applied

    for (int y_grad = 0; y_grad < fontHeight; y_grad++)
    {
        float f = (float)y_grad / (float)fontHeight;
        u8* row = bufferCursor + (y + y_grad) * (
#ifndef __PS3__
            g_TextBufferSurface->pitch
#else
            1280 * 4
#endif
        );
        for (int x_grad = 0; x_grad < spriteWidth; x_grad++)
        {
            u8* px = row + (x + x_grad) * 4;
#ifndef __PS3__
            if (px[3]) // A
            {
                px[0] = (u8)(px[0] * (1.0f - f * 0.5f)); // R
                px[1] = (u8)(px[1] * (1.0f - f * 0.5f)); // G
                px[2] = (u8)(px[2] * (1.0f - f * 0.25f)); // B
            }
#else
            // PS3 Big Endian ARGB: 0=A, 1=R, 2=G, 3=B
            if (px[0]) // A
            {
                px[1] = (u8)(px[1] * (1.0f - f * 0.5f)); // R
                px[2] = (u8)(px[2] * (1.0f - f * 0.5f)); // G
                px[3] = (u8)(px[3] * (1.0f - f * 0.25f)); // B
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
bool isUTF8Encoded(char *string)
{
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

#ifndef __PS3__
void SurfaceOverwriteBlend(SDL_Surface *srcSurface, SDL_Surface *dstSurface, u32 x)
{
    if(textNotExist) return;
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

void TextHelper::RenderTextToTexture(i32 xPos, i32 yPos, i32 spriteWidth, i32 spriteHeight, i32 fontWidth,
                                     i32 fontHeight, ZunColor textColor, ZunColor shadowColor, char *string,
                                     TextureData *outTexture)
{
    if(textNotExist) return;
    
    char convertedText[1024];

    if (!isUTF8Encoded(string))
    {
        std::string outputUtf;
    	sjis_to_utf8(string, (i32)strlen(string), outputUtf);
        std::strcpy(convertedText, outputUtf.c_str());
    }
    else
    {
        std::strcpy(convertedText, string);
    }

#ifndef __PS3__
    SDL_Rect finalCopyDst;
    SDL_Rect finalCopySrc;
    SDL_Rect shadowRect;
    SDL_Rect textRect;

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
            SDL_BlitSurface(shadowText, NULL, (SDL_Surface*)g_TextBufferSurface, &shadowRect);

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

        SurfaceOverwriteBlend(regularText, (SDL_Surface*)g_TextBufferSurface, xPos * 2);

        SDL_FreeSurface(regularText);
    }
#else
    u8 *pixels = (u8 *)g_TextBufferSurface;
    memset(pixels, 0, 1280 * TEXT_BUFFER_HEIGHT * 2 * 4);

    for (int pass = 0; pass < 2; pass++) {
        ZunColor color;
        int xOff, yOff;
        if (pass == 0) {
            if (shadowColor == COLOR_WHITE) continue;
            color = shadowColor;
            // Scale offsets for 2x buffer. PC EoSD is 1px shadow, so 2px at 2x scale.
            xOff = 2; yOff = 2;
        } else {
            color = textColor; xOff = 0; yOff = 0;
        }
        u8 r = (u8)((color >> 16) & 0xff), g = (u8)((color >> 8) & 0xff), b = (u8)(color & 0xff);
        u8 a = (u8)((color >> 24) & 0xff);
        if (a == 0 && (r != 0 || g != 0 || b != 0)) a = 0xff; // Fallback for 24-bit color constants
        // Render at 2x scale for supersampling
        float scale = stbtt_ScaleForPixelHeight(&g_Font, (float)fontHeight * 2.0f);
        int ascent; stbtt_GetFontVMetrics(&g_Font, &ascent, 0, 0);
        ascent = (int)((float)ascent * scale);
        int curX = xOff;
        const unsigned char* p = (const unsigned char*)convertedText;
        while (*p) {
            int cp;
            unsigned char cu = *p;
            if (cu < 0x80) cp = *p++;
            else if (cu < 0xE0) { cp = ((*p++ & 0x1F) << 6); cp |= (*p++ & 0x3F); }
            else if (cu < 0xF0) { cp = ((*p++ & 0x0F) << 12); cp |= ((*p++ & 0x3F) << 6); cp |= (*p++ & 0x3F); }
            else { cp = ((*p++ & 0x07) << 18); cp |= ((*p++ & 0x3F) << 12); cp |= ((*p++ & 0x3F) << 6); cp |= (*p++ & 0x3F); }
            int ax, lsb, c_x1, c_y1, c_x2, c_y2;
            stbtt_GetCodepointHMetrics(&g_Font, cp, &ax, &lsb);
            stbtt_GetCodepointBitmapBox(&g_Font, cp, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);
            int out_x = curX + (int)((float)lsb * scale), out_y = yOff + ascent + c_y1, b_w = c_x2 - c_x1, b_h = c_y2 - c_y1;
            if (b_w > 0 && b_h > 0) {
                u8* bmp = (u8*)malloc(b_w * b_h);
                stbtt_MakeCodepointBitmap(&g_Font, bmp, b_w, b_h, b_w, scale, scale, cp);
                for (int py = 0; py < b_h; py++) {
                    if (out_y + py < 0 || out_y + py >= TEXT_BUFFER_HEIGHT * 2) continue;
                    for (int px = 0; px < b_w; px++) {
                        if (out_x + px < 0 || out_x + px >= 1280) continue;
                        u8 alpha_pixel = bmp[py * b_w + px];
                        if (alpha_pixel == 0) continue;

                        // Fake bold: render multiple times with slight offsets in 2x space
                        // 3x3 stamp (+2px) for a strong bold effect matching PC version
                        for (int boldY = 0; boldY < 3; boldY++) {
                            for (int boldX = 0; boldX < 3; boldX++) {
                                int bx = out_x + px + boldX;
                                int by = out_y + py + boldY;
                                if (bx >= 1280 || by >= TEXT_BUFFER_HEIGHT * 2) continue;

                                u8* dst_px = &pixels[by * 1280 * 4 + bx * 4];
                                u8 glyph_a = (u8)((a * alpha_pixel) / 255);
                                
                                if (pass == 0) {
                                    // Shadow pass: simple max alpha
                                    if (glyph_a > dst_px[0]) {
                                        dst_px[0] = glyph_a;
                                        dst_px[1] = r; dst_px[2] = g; dst_px[3] = b;
                                    }
                                } else {
                                    // Text pass: alpha blend over shadow
                                    if (glyph_a > 0) {
                                        u32 old_a = dst_px[0];
                                        if (glyph_a >= 250 || old_a == 0) {
                                            dst_px[0] = glyph_a;
                                            dst_px[1] = r; dst_px[2] = g; dst_px[3] = b;
                                        } else {
                                            u32 out_a = glyph_a + (old_a * (255 - glyph_a)) / 255;
                                            if (out_a > 0) {
                                                dst_px[1] = (u8)((r * glyph_a + (u32)dst_px[1] * old_a * (255 - glyph_a) / 255) / out_a);
                                                dst_px[2] = (u8)((g * glyph_a + (u32)dst_px[2] * old_a * (255 - glyph_a) / 255) / out_a);
                                                dst_px[3] = (u8)((b * glyph_a + (u32)dst_px[3] * old_a * (255 - glyph_a) / 255) / out_a);
                                                dst_px[0] = (u8)out_a;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                free(bmp);
            }
            curX += (int)((float)ax * scale);
        }
    }
#endif

    // Once we get an API abstraction layer for surface operations, this needs to change
    //   We really shouldn't be clobbering the texture format
    if (!outTexture->textureData || outTexture->format != TEX_FMT_A8R8G8B8)
    {
        delete[] outTexture->textureData;
        outTexture->textureData = new u8[outTexture->width * outTexture->height * 4];
        if (outTexture->textureData) {
            memset(outTexture->textureData, 0, outTexture->width * outTexture->height * 4);
        }
    }

    outTexture->format = TEX_FMT_A8R8G8B8;
#ifndef __PS3__
    SDL_Surface *textureSurface = SDL_CreateRGBSurfaceWithFormatFrom(
        outTexture->textureData, outTexture->width, outTexture->height, (i32)SDL_BITSPERPIXEL(SDL_PIXELFORMAT_RGBA32),
        (i32)(outTexture->width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32)), SDL_PIXELFORMAT_RGBA32);

    InvertAlpha(0, 0, 640, fontHeight + 2);

    finalCopyDst.x = 0;
    finalCopyDst.y = yPos;
    finalCopyDst.w = spriteWidth;
    finalCopyDst.h = 16;

    if (SDL_SoftStretchLinear((SDL_Surface*)g_TextBufferSurface, &finalCopySrc, textureSurface, &finalCopyDst) < 0)
    {
        SDL_Log("SDL_BlitScaled failed! Error: %s", SDL_GetError());
    }

    g_AnmManager->SetCurrentTexture(outTexture->handle);

    g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outTexture->width, outTexture->height, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, outTexture->textureData);

    SDL_FreeSurface(textureSurface);
#else
    // Apply gradient on 2x buffer
    InvertAlpha(0, 0, 1280, fontHeight * 2 + 4);

    // Downsample 2x2 to 1x1 using premultiplied alpha for better quality AA
    for (int dy = 0; dy < fontHeight + 2; dy++) {
        if (yPos + dy >= (int)outTexture->height) break;
        if (dy >= TEXT_BUFFER_HEIGHT) break;
        for (int dx = 0; dx < spriteWidth; dx++) {
            if (xPos + dx >= (int)outTexture->width) break;
            if (dx >= 640) break;

            u32 total_a = 0, total_r = 0, total_g = 0, total_b = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    u8* src_px = &pixels[(dy * 2 + sy) * 1280 * 4 + (dx * 2 + sx) * 4];
                    u32 sa = src_px[0];
                    total_a += sa;
                    total_r += (u32)src_px[1] * sa;
                    total_g += (u32)src_px[2] * sa;
                    total_b += (u32)src_px[3] * sa;
                }
            }
            
            u8* dst_px = &outTexture->textureData[(yPos + dy) * outTexture->width * 4 + (xPos + dx) * 4];
            if (total_a > 0) {
                dst_px[0] = (u8)(total_a / 4);
                dst_px[1] = (u8)(total_r / total_a);
                dst_px[2] = (u8)(total_g / total_a);
                dst_px[3] = (u8)(total_b / total_a);
            } else {
                dst_px[0] = dst_px[1] = dst_px[2] = dst_px[3] = 0;
            }
        }
    }
    g_AnmManager->SetCurrentTexture(outTexture->handle);
    // PS3 uses GL_ARGB_SCE (0x6007) and GL_UNSIGNED_INT_8_8_8_8 (0x8035)
#ifndef GL_ARGB_SCE
#define GL_ARGB_SCE 0x6007
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#endif
    g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB_SCE, outTexture->width, outTexture->height, 0, GL_ARGB_SCE,
                               GL_UNSIGNED_INT_8_8_8_8, outTexture->textureData);
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

    if (g_TextBufferSurface != NULL)
    {
        SDL_FreeSurface((SDL_Surface*)g_TextBufferSurface);
        g_TextBufferSurface = NULL;
    }
#else
    if (g_TextBufferSurface != NULL)
    {
        free(g_TextBufferSurface);
        g_TextBufferSurface = NULL;
    }
#endif
    
    return;
}
