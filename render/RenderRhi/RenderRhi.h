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

enum class GpuFormat {
	Unknown,
	RGBA8,
	BGRA8,
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

enum GpuCapabilityFlags {
	GpuCapability_None = 0,
	GpuCapability_Buffers = 1 << 0,
	GpuCapability_Textures = 1 << 1,
	GpuCapability_RenderPass = 1 << 2,
	GpuCapability_Pipelines = 1 << 3,
};

enum class GpuResult {
	Ok,
	InvalidArgument,
	InvalidHandle,
	InvalidState,
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

struct GpuRenderPassDesc : Moveable<GpuRenderPassDesc> {
	GpuTextureId color_target;
	GpuFormat color_format = GpuFormat::Unknown;
	GpuLoadOp color_load = GpuLoadOp::Load;
	GpuStoreOp color_store = GpuStoreOp::Store;
	String label;
};

struct GpuPipelineDesc : Moveable<GpuPipelineDesc> {
	GpuPrimitiveTopology topology = GpuPrimitiveTopology::TriangleList;
	GpuFormat color_format = GpuFormat::Unknown;
	String label;
};

class GpuDevice {
public:
	virtual ~GpuDevice() {}

	virtual GpuDeviceId GetDeviceId() const = 0;
	virtual GpuBackendKind GetBackendKind() const = 0;
	virtual GpuAdapterInfo GetAdapterInfo() const = 0;

	virtual GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) = 0;
	virtual GpuResult DestroyBuffer(GpuBufferId id) = 0;

	virtual GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) = 0;
	virtual GpuResult DestroyTexture(GpuTextureId id) = 0;

	virtual GpuResult CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out) = 0;
	virtual GpuResult DestroyPipeline(GpuPipelineId id) = 0;

	virtual GpuResult BeginCommands(GpuCommandListId& out) = 0;
	virtual GpuResult BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc) = 0;
	virtual GpuResult SetPipeline(GpuCommandListId list, GpuPipelineId pipeline) = 0;
	virtual GpuResult SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer) = 0;
	virtual GpuResult Draw(GpuCommandListId list, int vertex_count, int first_vertex = 0) = 0;
	virtual GpuResult EndRenderPass(GpuCommandListId list) = 0;
	virtual GpuResult EndCommands(GpuCommandListId list) = 0;
	virtual GpuResult Submit(GpuCommandListId list) = 0;
};

String DumpGpuBackendKind(GpuBackendKind kind);
String DumpGpuFormat(GpuFormat format);
String DumpGpuBufferUsage(int usage);
String DumpGpuTextureUsage(int usage);
String DumpGpuLoadOp(GpuLoadOp op);
String DumpGpuStoreOp(GpuStoreOp op);
String DumpGpuPrimitiveTopology(GpuPrimitiveTopology topology);
String DumpGpuCapabilityFlags(int flags);
String DumpGpuResult(GpuResult result);
String DumpGpuError(GpuError error);

}
