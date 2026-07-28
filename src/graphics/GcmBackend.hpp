#pragma once

#include "GfxInterface.hpp"
#include <cell/gcm.h>
#include <cell/gcm/gcm_method_data.h>
#include <vector>

struct GcmLocalAllocator {
    uint32_t base;
    uint32_t size;
    uint32_t offset;
    
    void Init(uint32_t base_addr, uint32_t size_bytes) {
        base = base_addr;
        size = size_bytes;
        offset = 0;
    }
    
    uint32_t Allocate(uint32_t size_bytes, uint32_t alignment) {
        uint32_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
        if (aligned_offset + size_bytes > size) {
            return 0; // Out of memory
        }
        offset = aligned_offset + size_bytes;
        return base + aligned_offset;
    }
};

struct GcmTexture {
    uint32_t offset;
    uint32_t format;
    uint32_t remap;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

struct GcmBackend : GfxInterface
{
    static GfxInterface *Init();
    static void SetContextFlags();
    virtual void Exit();
    ~GcmBackend() override
    {
        Exit();
    };

    virtual void SetFogRange(f32 nearPlane, f32 farPlane);
    virtual void SetFogColor(ZunColor color);
    virtual void ToggleVertexAttribute(u8 attr, bool enable);
    virtual void SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr);
    virtual void SetColorOp(TextureOpComponent component, ColorOp op);
    virtual void SetTextureFactor(ZunColor factor);
    virtual void SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix);

    virtual void SetTextureFilter();

    virtual void GetViewport(u32 *viewport);
    virtual void GetDepthRange(f32 *depthRange);
    virtual void SetViewport(i32 x, i32 y, i32 width, i32 height);
    virtual void SetDepthRange(f32 nearPlane, f32 farPlane);

    virtual void Enable(Capabilities cap);
    virtual bool HasError() { return false; }
    virtual void SetBlendMode(BlendMode mode);
    virtual void SetDepthMask(bool enable);
    virtual void SetDepthFunc(DepthFunc func);

    virtual void SetClearDepth(f32 depth);
    virtual void SetClearColor(f32 r, f32 g, f32 b, f32 a);
    virtual void Clear(u32 clearBits);

    virtual GfxTextureHandle CreateTexture();
    virtual void BindTexture(GfxTextureHandle handle);
    virtual void DeleteTexture(GfxTextureHandle handle);
    virtual void SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data);
    virtual void SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data);

    virtual void ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels);
    virtual void CopyTextureFromBackbuffer(i32 x, i32 y, i32 width, i32 height);

    virtual void Draw(PrimitiveType type, i32 start, i32 count);
    virtual void SwapBuffers();

  private:
    uint32_t m_viewport[4];
    f32 m_depthRange[2];
    bool m_textureEnabled;
    GfxTextureHandle m_boundTexture = 0;
    
    // GCM specific fields
    uint32_t current_buffer_index;
    std::vector<GcmTexture> textures;
    std::vector<uint32_t> freeTextures;

    uint8_t *m_localAddressBase;
    uint32_t m_localSize;
    uint32_t m_displayBufferOffsets[2];
    uint32_t m_depthBufferOffset;
    void *m_ioBuffer;

    // Cg Shaders
    void *m_vertexProgramUcode;
    CGprogram m_vertexProgram;
    CGprogram m_fragmentProgram;
    CellCgbFragmentProgramConfiguration m_fragmentProgramCfg;
};
