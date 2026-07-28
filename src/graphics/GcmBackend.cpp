#include "GcmBackend.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"
#include <sysutil/sysutil_sysparam.h>
#include <sys/timer.h>
#include <cell/sysmodule.h>
#include <cstring>
#include <cmath>

extern "C" void *memalign(size_t boundary, size_t size);
extern "C" void free(void *ptr);

static GcmLocalAllocator s_localAllocator;

GfxInterface *GcmBackend::Init()
{
	g_GameErrorContext.Log("GcmBackend::Init (PS3 path) initializing GCM...\n");

	// Load GCM module
	cellSysmoduleLoadModule(CELL_SYSMODULE_GCM);

	// Dynamically allocate 1MB aligned I/O memory buffer to avoid assembly alignment limit on static BSS
	void *io_buffer = memalign(1024 * 1024, 1024 * 1024);
	if (io_buffer == nullptr) {
		g_GameErrorContext.Log("CRITICAL: Failed to allocate aligned IO buffer!\n");
		return nullptr;
	}

	// Initialize GCM context using 3 arguments (handle already-initialized as non-fatal)
	int32_t ret = cellGcmInit(1024 * 1024, 1024 * 1024, io_buffer);
	if (ret != CELL_OK && ret != -2145320705 && ret != 0x8013000F) {
		g_GameErrorContext.Log("CRITICAL: cellGcmInit failed! Error: %d\n", ret);
		free(io_buffer);
		return nullptr;
	}
	if (ret == -2145320705 || ret == 0x8013000F) {
		g_GameErrorContext.Log("GCM is already initialized. Proceeding with existing GCM context.\n");
	}
	else {
		g_GameErrorContext.Log("GCM initialized.\n");
	}

	CellGcmConfig config;
	cellGcmGetConfiguration(&config);

	// Initialize VRAM Local Allocator
	s_localAllocator.Init((uint32_t)(uintptr_t)config.localAddress, config.localSize);

	CellVideoOutState videoState;
	cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);

	uint32_t width = 1280;
	uint32_t height = 720;
	if (videoState.displayMode.resolutionId == CELL_VIDEO_OUT_RESOLUTION_1080) {
		width = 1920;
		height = 1080;
	}

	g_GameWindowWidthReal = width;
	g_GameWindowHeightReal = height;
	g_GameErrorContext.Log("GCM Render Resolution: %ux%u\n", width, height);

	// Allocate 2 display buffers in local memory (VRAM)
	uint32_t buffer_size = width * height * 4;
	uint32_t buffer0 = s_localAllocator.Allocate(buffer_size, 1024 * 1024);
	uint32_t buffer1 = s_localAllocator.Allocate(buffer_size, 1024 * 1024);

	if (buffer0 == 0 || buffer1 == 0) {
		g_GameErrorContext.Log("CRITICAL: Failed to allocate display buffers in VRAM!\n");
		free(io_buffer);
		return nullptr;
	}

	cellGcmSetDisplayBuffer(0, buffer0, width * 4, width, height);
	cellGcmSetDisplayBuffer(1, buffer1, width * 4, width, height);

	// Allocate depth buffer in VRAM
	uint32_t depth_size = width * height * 4;
	uint32_t depth_buffer = s_localAllocator.Allocate(depth_size, 1024 * 1024);
	if (depth_buffer == 0) {
		g_GameErrorContext.Log("CRITICAL: Failed to allocate depth buffer in VRAM!\n");
		free(io_buffer);
		return nullptr;
	}

	// Set surface configuration
	CellGcmSurface surface;
	std::memset(&surface, 0, sizeof(surface));
	surface.colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
	surface.colorTarget = CELL_GCM_SURFACE_TARGET_0;
	surface.colorLocation[0] = CELL_GCM_LOCATION_LOCAL;
	surface.colorOffset[0] = buffer0;
	surface.colorPitch[0] = width * 4;
	surface.depthFormat = CELL_GCM_SURFACE_Z24S8;
	surface.depthLocation = CELL_GCM_LOCATION_LOCAL;
	surface.depthOffset = depth_buffer;
	surface.depthPitch = width * 4;
	surface.x = 0;
	surface.y = 0;
	surface.width = width;
	surface.height = height;

	cellGcmSetSurface(gCellGcmCurrentContext, &surface);

	GcmBackend *self = new GcmBackend();
	self->current_buffer_index = 0;
	self->m_textureEnabled = false;
	self->m_localAddressBase = (uint8_t*)config.localAddress;
	self->m_localSize = config.localSize;
	self->m_displayBufferOffsets[0] = buffer0;
	self->m_displayBufferOffsets[1] = buffer1;
	self->m_depthBufferOffset = depth_buffer;
	self->m_ioBuffer = io_buffer;

	// Initialize Shader Cg Programs to draw basic primitives
	cellGcmCgInitProgram(self->m_vertexProgram);
	cellGcmCgGetUCode(self->m_vertexProgram, &self->m_vertexProgramUcode, nullptr);

	cellGcmCgInitProgram(self->m_fragmentProgram);
	void *ucode_main = nullptr;
	uint32_t ucode_size = 0;
	cellGcmCgGetUCode(self->m_fragmentProgram, &ucode_main, &ucode_size);
	void *ucode_local = (void*)(uintptr_t)s_localAllocator.Allocate(ucode_size, 1024);
	if (ucode_local != nullptr) {
		std::memcpy(ucode_local, ucode_main, ucode_size);
	}
	cellGcmCgGetCgbFragmentProgramConfiguration(self->m_fragmentProgram, &self->m_fragmentProgramCfg, 0, 1, 0);

	uint32_t offset = 0;
	cellGcmAddressToOffset(ucode_local, &offset);
	self->m_fragmentProgramCfg.offset = offset;

	// Bind shaders to RSX
	cellGcmSetVertexProgram(gCellGcmCurrentContext, self->m_vertexProgram, self->m_vertexProgramUcode);
	cellGcmSetFragmentProgramLoad(gCellGcmCurrentContext, &self->m_fragmentProgramCfg);

	// Determine safe viewport scale and offsets
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

	self->SetViewport(0, 0, 640, 480);
	self->SetDepthRange(0.0f, 1.0f);

	g_GameErrorContext.Log("GcmBackend::Init completed successfully.\n");
	return self;
}

void GcmBackend::SetContextFlags()
{
}

void GcmBackend::Exit()
{
	if (m_ioBuffer != nullptr) {
		free(m_ioBuffer);
		m_ioBuffer = nullptr;
	}
	cellSysmoduleUnloadModule(CELL_SYSMODULE_GCM);
}

void GcmBackend::SetFogRange(f32 nearPlane, f32 farPlane)
{
	// Fog range can be passed as constants to Cg shaders
}

void GcmBackend::SetFogColor(ZunColor color)
{
}

void GcmBackend::ToggleVertexAttribute(u8 attr, bool enable)
{
	if (attr & VERTEX_ATTR_TEX_COORD)
	{
		m_textureEnabled = enable;
	}
}

void GcmBackend::SetAttributePointer(VertexAttributeArrays attr, std::size_t stride, void *ptr)
{
	uint32_t offset = 0;
	cellGcmAddressToOffset(ptr, &offset);

	uint8_t size = 3;
	uint8_t type = CELL_GCM_VERTEX_F;
	uint32_t index = 0;

	switch (attr)
	{
	case VERTEX_ARRAY_POSITION:
		index = 0;
		size = 3;
		type = CELL_GCM_VERTEX_F;
		break;
	case VERTEX_ARRAY_TEX_COORD:
		index = 1;
		size = 2;
		type = CELL_GCM_VERTEX_F;
		break;
	case VERTEX_ARRAY_DIFFUSE:
		index = 2;
		size = 4;
		type = CELL_GCM_VERTEX_UB;
		break;
	}

	// Correctly map memory location to CELL_GCM_LOCATION_LOCAL or CELL_GCM_LOCATION_MAIN
	uint8_t location = ((uint8_t*)ptr >= m_localAddressBase && (uint8_t*)ptr < m_localAddressBase + m_localSize) ? CELL_GCM_LOCATION_LOCAL : CELL_GCM_LOCATION_MAIN;

	cellGcmSetVertexDataArray(gCellGcmCurrentContext, index, 0, stride, size, type, location, offset);
}

void GcmBackend::SetColorOp(TextureOpComponent component, ColorOp op)
{
}

void GcmBackend::SetTextureFactor(ZunColor factor)
{
}

void GcmBackend::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix)
{
}

void GcmBackend::SetTextureFilter()
{
}

void GcmBackend::GetViewport(u32 *viewport)
{
	std::memcpy(viewport, m_viewport, sizeof(m_viewport));
}

void GcmBackend::GetDepthRange(f32 *depthRange)
{
	std::memcpy(depthRange, m_depthRange, sizeof(m_depthRange));
}

void GcmBackend::SetViewport(i32 x, i32 y, i32 width, i32 height)
{
	m_viewport[0] = x;
	m_viewport[1] = y;
	m_viewport[2] = width;
	m_viewport[3] = height;

	float scale[4] = { width * 0.5f, -height * 0.5f, (m_depthRange[1] - m_depthRange[0]) * 0.5f, 0.0f };
	float offset[4] = { x + width * 0.5f, y + height * 0.5f, (m_depthRange[1] + m_depthRange[0]) * 0.5f, 0.0f };

	cellGcmSetViewport(gCellGcmCurrentContext, x, y, width, height, m_depthRange[0], m_depthRange[1], scale, offset);
	cellGcmSetScissor(gCellGcmCurrentContext, x, y, width, height);
}

void GcmBackend::SetDepthRange(f32 nearPlane, f32 farPlane)
{
	m_depthRange[0] = nearPlane;
	m_depthRange[1] = farPlane;
	SetViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
}

void GcmBackend::Enable(Capabilities cap)
{
	switch (cap)
	{
	case CAPS_BLEND:
		cellGcmSetBlendEnable(gCellGcmCurrentContext, CELL_GCM_TRUE);
		break;
	case CAPS_DEPTH_TEST:
		cellGcmSetDepthTestEnable(gCellGcmCurrentContext, CELL_GCM_TRUE);
		break;
	}
}

void GcmBackend::SetBlendMode(BlendMode mode)
{
	if (mode == BLEND_INV_SRC_ALPHA)
	{
		cellGcmSetBlendFunc(gCellGcmCurrentContext, CELL_GCM_SRC_ALPHA, CELL_GCM_ONE_MINUS_SRC_ALPHA, CELL_GCM_SRC_ALPHA, CELL_GCM_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		cellGcmSetBlendFunc(gCellGcmCurrentContext, CELL_GCM_SRC_ALPHA, CELL_GCM_ONE, CELL_GCM_SRC_ALPHA, CELL_GCM_ONE);
	}
}

void GcmBackend::SetDepthMask(bool enable)
{
	cellGcmSetDepthMask(gCellGcmCurrentContext, enable ? CELL_GCM_TRUE : CELL_GCM_FALSE);
}

void GcmBackend::SetDepthFunc(DepthFunc func)
{
	if (func == DEPTH_FUNC_ALWAYS)
	{
		cellGcmSetDepthFunc(gCellGcmCurrentContext, CELL_GCM_ALWAYS);
	}
	else
	{
		cellGcmSetDepthFunc(gCellGcmCurrentContext, CELL_GCM_LEQUAL);
	}
}

void GcmBackend::SetClearDepth(f32 depth)
{
	cellGcmSetClearDepthStencil(gCellGcmCurrentContext, ((uint32_t)(depth * 0xffffff) << 8));
}

void GcmBackend::SetClearColor(f32 r, f32 g, f32 b, f32 a)
{
	uint32_t color = ((uint32_t)(a * 255.0f) << 24) |
		((uint32_t)(r * 255.0f) << 16) |
		((uint32_t)(g * 255.0f) << 8) |
		((uint32_t)(b * 255.0f));
	cellGcmSetClearColor(gCellGcmCurrentContext, color);
}

void GcmBackend::Clear(u32 clearBits)
{
	uint32_t mask = 0;
	if (clearBits & CLEAR_COLOR_BUFFER)
		mask |= (CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A);
	if (clearBits & CLEAR_DEPTH_BUFFER)
		mask |= CELL_GCM_CLEAR_Z;

	cellGcmSetClearSurface(gCellGcmCurrentContext, mask);
}

GfxTextureHandle GcmBackend::CreateTexture()
{
	GcmTexture tex;
	std::memset(&tex, 0, sizeof(tex));

	uint32_t id = 0;
	if (!freeTextures.empty()) {
		id = freeTextures.back();
		freeTextures.pop_back();
		textures[id] = tex;
	}
	else {
		id = textures.size();
		textures.push_back(tex);
	}
	return id + 1;
}

void GcmBackend::BindTexture(GfxTextureHandle handle)
{
	m_boundTexture = handle;
	if (handle > 0 && handle <= textures.size()) {
		GcmTexture &tex = textures[handle - 1];

		// Push texture offset, format, and other states directly to the GCM command stream
		cellGcmReserveMethodSizeInline(gCellGcmCurrentContext, 11);
		uint32_t *cmd = (uint32_t *)gCellGcmCurrentContext->current;
		cmd[0] = CELL_GCM_METHOD_HEADER_TEXTURE_OFFSET(0, 8);
		cmd[1] = tex.offset;
		cmd[2] = tex.format;
		cmd[3] = CELL_GCM_TEXTURE_CLAMP_TO_EDGE; // Wrap U, V
		cmd[4] = 0; // Control 0
		cmd[5] = tex.remap;
		cmd[6] = 0; // Filter
		cmd[7] = CELL_GCM_METHOD_DATA_TEXTURE_IMAGE_RECT(tex.height, tex.width);
		cmd[8] = 0; // Border color
		cmd[9] = CELL_GCM_METHOD_HEADER_TEXTURE_CONTROL3(0, 1);
		cmd[10] = 0; // Control 3
		gCellGcmCurrentContext->current += 11;
	}
}

void GcmBackend::DeleteTexture(GfxTextureHandle handle)
{
	if (handle > 0 && handle <= textures.size()) {
		freeTextures.push_back(handle - 1);
	}
}

void GcmBackend::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type, const void *data)
{
	if (m_boundTexture == 0 || m_boundTexture > textures.size()) return;
	GcmTexture &tex = textures[m_boundTexture - 1];

	tex.width = width;
	tex.height = height;
	tex.pitch = width * 4;

	uint32_t size = tex.pitch * height;
	uint32_t vram_addr = s_localAllocator.Allocate(size, 1024);
	if (vram_addr != 0) {
		if (data != nullptr) {
			std::memcpy((void*)(uintptr_t)vram_addr, data, size);
		}
		cellGcmAddressToOffset((void*)(uintptr_t)vram_addr, &tex.offset);
	}

	tex.format = CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
}

void GcmBackend::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height, const void *data)
{
	if (m_boundTexture == 0 || m_boundTexture > textures.size()) return;
	GcmTexture &tex = textures[m_boundTexture - 1];

	// Convert local VRAM offset to CPU/PPU address using m_localAddressBase
	uint8_t *dest_addr = m_localAddressBase + tex.offset;
	if (dest_addr != nullptr && data != nullptr) {
		// Copy to VRAM location with offsets
		for (i32 i = 0; i < height; ++i) {
			std::memcpy(
				dest_addr + (yoffset + i) * tex.pitch + xoffset * 4,
				(const void*)((uintptr_t)data + i * width * 4),
				width * 4
				);
		}
	}
}

void GcmBackend::ReadPixels(i32 x, i32 y, i32 width, i32 height, const void *pixels)
{
}

void GcmBackend::CopyTextureFromBackbuffer(i32 x, i32 y, i32 width, i32 height)
{
}

void GcmBackend::Draw(PrimitiveType type, i32 start, i32 count)
{
	uint32_t gcm_type = CELL_GCM_PRIMITIVE_TRIANGLES;
	if (type == PRIM_TRIANGLE_STRIP) {
		gcm_type = CELL_GCM_PRIMITIVE_TRIANGLE_STRIP;
	}
	cellGcmSetDrawArrays(gCellGcmCurrentContext, gcm_type, start, count);
}

void GcmBackend::SwapBuffers()
{
	while (cellGcmGetFlipStatus() != 0) {
		sys_timer_usleep(200);
	}

	cellGcmSetFlip(gCellGcmCurrentContext, current_buffer_index);
	cellGcmFlush(gCellGcmCurrentContext);

	current_buffer_index = (current_buffer_index + 1) % 2;

	// Bind current frame display buffer as surface color target
	CellGcmSurface surface;
	std::memset(&surface, 0, sizeof(surface));
	surface.colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
	surface.colorTarget = CELL_GCM_SURFACE_TARGET_0;
	surface.colorLocation[0] = CELL_GCM_LOCATION_LOCAL;

	uint32_t buffer_addr = m_displayBufferOffsets[current_buffer_index];
	surface.colorOffset[0] = buffer_addr;
	surface.colorPitch[0] = g_GameWindowWidthReal * 4;
	surface.depthFormat = CELL_GCM_SURFACE_Z24S8;
	surface.depthLocation = CELL_GCM_LOCATION_LOCAL;

	// Depth buffer resides at its allocated offset in local VRAM
	surface.depthOffset = m_depthBufferOffset;
	surface.depthPitch = g_GameWindowWidthReal * 4;
	surface.x = 0;
	surface.y = 0;
	surface.width = g_GameWindowWidthReal;
	surface.height = g_GameWindowHeightReal;

	cellGcmSetSurface(gCellGcmCurrentContext, &surface);
}
