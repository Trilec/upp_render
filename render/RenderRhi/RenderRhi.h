#pragma once

#include <Core/Core.h>

namespace Upp {

enum class GpuBackendKind {
	Unknown,
	Null,
	Vulkan,
	Metal,
	WebGPU,
	OpenGL,
};

enum class GpuNativeWindowKind {
	None,
	Win32,
};

enum class GpuFormat {
	Unknown,
	RGBA8,
	BGRA8,
	RGBA8Srgb,
	BGRA8Srgb,
	R16F,
	D24S8,
};

enum GpuBufferUsage {
	GpuBufferUsage_None = 0,
	GpuBufferUsage_Vertex = 1 << 0,
	GpuBufferUsage_Index = 1 << 1,
	GpuBufferUsage_Uniform = 1 << 2,
	GpuBufferUsage_Storage = 1 << 3,
	GpuBufferUsage_TransferSrc = 1 << 4,
	GpuBufferUsage_TransferDst = 1 << 5,
};

enum GpuTextureUsage {
	GpuTextureUsage_None = 0,
	GpuTextureUsage_ColorAttachment = 1 << 0,
	GpuTextureUsage_Sampled = 1 << 1,
	GpuTextureUsage_TransferSrc = 1 << 2,
	GpuTextureUsage_TransferDst = 1 << 3,
};

enum class GpuLoadOp {
	Load,
	Clear,
	DontCare,
};

enum class GpuStoreOp {
	Store,
	DontCare,
};

enum class GpuPrimitiveTopology {
	TriangleList,
	LineList,
};

enum class GpuShaderStage {
	Unknown,
	Vertex,
	Fragment,
};

enum class GpuShaderFormat {
	Unknown,
	SpirV,
};

enum class GpuVertexLayout {
	Unknown,
	Position2Color4F,
	Position2Uv2Color4F,
};

enum class GpuBlendMode {
	Opaque,
	SourceOver,
};

enum class GpuSamplerFilter {
	Nearest,
	Linear,
};

enum class GpuSamplerAddressMode {
	ClampToEdge,
	Repeat,
};

enum GpuCapabilityFlags {
	GpuCapability_None = 0,
	GpuCapability_Buffers = 1 << 0,
	GpuCapability_Textures = 1 << 1,
	GpuCapability_RenderPass = 1 << 2,
	GpuCapability_Pipelines = 1 << 3,
	GpuCapability_Shaders = 1 << 4,
};

enum class GpuResult {
	Ok,
	InvalidArgument,
	InvalidHandle,
	InvalidState,
	OutOfDate,
	NotFound,
	Unsupported,
};

enum class GpuError {
	None,
	InvalidHandle,
	InvalidState,
	InvalidArgument,
	NotFound,
	Unsupported,
};

#define GPU_DEFINE_ID(name) \
struct name : Moveable<name> { \
	int value = 0; \
	bool IsValid() const { return value > 0; } \
	bool operator==(const name& other) const { return value == other.value; } \
	bool operator!=(const name& other) const { return !(*this == other); } \
	String Dump() const; \
};

GPU_DEFINE_ID(GpuAdapterId)
GPU_DEFINE_ID(GpuDeviceId)
GPU_DEFINE_ID(GpuBufferId)
GPU_DEFINE_ID(GpuTextureId)
GPU_DEFINE_ID(GpuShaderId)
GPU_DEFINE_ID(GpuPipelineId)
GPU_DEFINE_ID(GpuCommandListId)
GPU_DEFINE_ID(GpuSurfaceId)
GPU_DEFINE_ID(GpuSwapchainId)
GPU_DEFINE_ID(GpuFrameId)

#undef GPU_DEFINE_ID

struct GpuAdapterInfo : Moveable<GpuAdapterInfo> {
	GpuAdapterId adapter_id;
	GpuDeviceId device_id;
	GpuBackendKind backend_kind = GpuBackendKind::Unknown;
	String name;
	int capability_flags = GpuCapability_None;
};

struct GpuDeviceDesc : Moveable<GpuDeviceDesc> {
	GpuAdapterId adapter_id;
	String label;
	bool validation = true;
};

struct GpuBufferDesc : Moveable<GpuBufferDesc> {
	int64 size = 0;
	int usage = GpuBufferUsage_None;
	String label;
};

struct GpuTextureDesc : Moveable<GpuTextureDesc> {
	Size size = Size(0, 0);
	GpuFormat format = GpuFormat::Unknown;
	int usage = GpuTextureUsage_None;
	String label;
};

struct GpuTextureWriteDesc : Moveable<GpuTextureWriteDesc> {
	Point origin = Point(0, 0);
	Size size = Size(0, 0);
	int64 row_pitch = 0;
};

struct GpuShaderDesc : Moveable<GpuShaderDesc> {
	GpuShaderStage stage = GpuShaderStage::Unknown;
	GpuShaderFormat format = GpuShaderFormat::Unknown;
	String code;
	String entry_point = "main";
	String label;
};

struct GpuClearColor : Moveable<GpuClearColor> {
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;
	float alpha = 1.0f;
};

struct GpuRenderPassDesc : Moveable<GpuRenderPassDesc> {
	GpuTextureId color_target;
	GpuFormat color_format = GpuFormat::Unknown;
	GpuLoadOp color_load = GpuLoadOp::Load;
	GpuStoreOp color_store = GpuStoreOp::Store;
	GpuClearColor clear_color;
	String label;
};

struct GpuPipelineDesc : Moveable<GpuPipelineDesc> {
	GpuPrimitiveTopology topology = GpuPrimitiveTopology::TriangleList;
	GpuFormat color_format = GpuFormat::Unknown;
	GpuShaderId vertex_shader;
	GpuShaderId fragment_shader;
	GpuVertexLayout vertex_layout = GpuVertexLayout::Unknown;
	GpuBlendMode blend_mode = GpuBlendMode::Opaque;
	// Stage-5 starts with one conventional sampled 2D texture slot. Keeping
	// sampler state on the pipeline avoids exposing backend descriptor objects.
	int sampled_texture_count = 0;
	GpuSamplerFilter sampler_filter = GpuSamplerFilter::Linear;
	GpuSamplerAddressMode sampler_address = GpuSamplerAddressMode::ClampToEdge;
	String label;
};

struct GpuNativeWindowDesc : Moveable<GpuNativeWindowDesc> {
	GpuNativeWindowKind kind = GpuNativeWindowKind::None;
	uintptr_t handle = 0;

	bool IsValid() const;
};

struct GpuSurfaceDesc : Moveable<GpuSurfaceDesc> {
	String label;
	Size size = Size(0, 0);
	GpuNativeWindowDesc native_window;
};

struct GpuSwapchainDesc : Moveable<GpuSwapchainDesc> {
	String label;
	GpuSurfaceId surface;
	// Requested presentation properties. A backend may negotiate supported values;
	// BeginFrame reports the actual acquired size and color format in GpuFrameInfo.
	Size size = Size(0, 0);
	GpuFormat color_format = GpuFormat::RGBA8;
	int image_count = 2;
};

struct GpuFrameInfo : Moveable<GpuFrameInfo> {
	GpuFrameId frame;
	GpuSwapchainId swapchain;
	GpuTextureId color_target;
	Size size = Size(0, 0);
	GpuFormat color_format = GpuFormat::Unknown;
};

class GpuDevice {
public:
	virtual ~GpuDevice() {}

	virtual GpuDeviceId GetDeviceId() const = 0;
	virtual GpuBackendKind GetBackendKind() const = 0;
	virtual GpuAdapterInfo GetAdapterInfo() const = 0;
	virtual String GetLastError() const { return String(); }

	virtual GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) = 0;
	virtual GpuResult WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size) = 0;
	virtual GpuResult DestroyBuffer(GpuBufferId id) = 0;

	virtual GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) = 0;
	virtual GpuResult WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size) = 0;
	virtual GpuResult DestroyTexture(GpuTextureId id) = 0;

	virtual GpuResult CreateSurface(const GpuSurfaceDesc& desc, GpuSurfaceId& out) = 0;
	virtual GpuResult DestroySurface(GpuSurfaceId id) = 0;

	virtual GpuResult CreateSwapchain(const GpuSwapchainDesc& desc, GpuSwapchainId& out) = 0;
	virtual GpuResult DestroySwapchain(GpuSwapchainId id) = 0;
	virtual GpuResult ResizeSwapchain(GpuSwapchainId id, Size size) = 0;

	virtual GpuResult BeginFrame(GpuSwapchainId swapchain, GpuFrameInfo& out) = 0;
	virtual GpuResult Present(GpuFrameId frame) = 0;

	virtual GpuResult CreateShader(const GpuShaderDesc&, GpuShaderId& out) {
		out = GpuShaderId();
		return GpuResult::Unsupported;
	}
	virtual GpuResult DestroyShader(GpuShaderId) { return GpuResult::Unsupported; }

	virtual GpuResult CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out) = 0;
	virtual GpuResult DestroyPipeline(GpuPipelineId id) = 0;

	virtual GpuResult BeginCommands(GpuCommandListId& out) = 0;
	virtual GpuResult BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc) = 0;
	virtual GpuResult SetPipeline(GpuCommandListId list, GpuPipelineId pipeline) = 0;
	virtual GpuResult SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer) = 0;
	virtual GpuResult SetSampledTexture(GpuCommandListId, int, GpuTextureId) { return GpuResult::Unsupported; }
	virtual GpuResult Draw(GpuCommandListId list, int vertex_count, int first_vertex = 0) = 0;
	virtual GpuResult EndRenderPass(GpuCommandListId list) = 0;
	virtual GpuResult EndCommands(GpuCommandListId list) = 0;
	virtual GpuResult Submit(GpuCommandListId list) = 0;
};

String DumpGpuBackendKind(GpuBackendKind kind);
String DumpGpuNativeWindowKind(GpuNativeWindowKind kind);
String DumpGpuNativeWindowDesc(const GpuNativeWindowDesc& desc);
String DumpGpuFormat(GpuFormat format);
String DumpGpuBufferUsage(int usage);
String DumpGpuTextureUsage(int usage);
String DumpGpuLoadOp(GpuLoadOp op);
String DumpGpuStoreOp(GpuStoreOp op);
String DumpGpuPrimitiveTopology(GpuPrimitiveTopology topology);
String DumpGpuShaderStage(GpuShaderStage stage);
String DumpGpuShaderFormat(GpuShaderFormat format);
String DumpGpuVertexLayout(GpuVertexLayout layout);
String DumpGpuSamplerFilter(GpuSamplerFilter filter);
String DumpGpuSamplerAddressMode(GpuSamplerAddressMode mode);
String DumpGpuCapabilityFlags(int flags);
String DumpGpuResult(GpuResult result);
String DumpGpuError(GpuError error);

}
