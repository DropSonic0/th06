#pragma once

#include "GfxInterface.hpp"

struct FixedFunctionGL : GfxInterface
{
    static void SetContextFlags();
    static GfxInterface *Init();

    virtual void SetFogRange(f32 nearPlane, f32 farPlane);
    virtual void SetFogColor(ZunColor color);
    virtual void ToggleVertexAttribute(u8 attr, bool enable);
    virtual void SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr);
    virtual void SetColorOp(TextureOpComponent component, ColorOp op);
    virtual void SetTextureFactor(ZunColor factor);
    virtual void SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix);
    virtual void SetDepthMask(bool enable);
    virtual void SetDepthFunc(DepthFunc func);
    virtual void Clear(ZunColor color);
    virtual void Draw();
};

