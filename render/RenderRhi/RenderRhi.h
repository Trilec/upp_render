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
	// Raw source channels: no implicit colour conversion or premultiplication.
	RGBA16,
	RGBA16F,
	RGBA32F,
	R32F,
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
	Compute,
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
	DestinationInvert,
	// Source RGB is already multiplied by source alpha in the working colour space.
	PremultipliedSourceOver,
	DestinationIn, // Multiply destination colour and alpha by source alpha.
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

// DeviceLocal forbids host reads/writes, including on unified-memory devices.
enum class GpuBufferMemory { HostVisible, DeviceLocal };

struct GpuBufferDesc : Moveable<GpuBufferDesc> {
	int64 size = 0;
	int usage = GpuBufferUsage_None;
	String label;
	GpuBufferMemory memory = GpuBufferMemory::HostVisible;
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

// Physical-device support for optimal, single-sample, single-mip 2D images.
// max_size/resource size apply to the exact requested usage, including the
// backend's transfer-destination requirement; they are not available-memory budgets.
struct GpuTextureCapabilities : Moveable<GpuTextureCapabilities> {
	GpuFormat format = GpuFormat::Unknown;
	int bytes_per_pixel = 0;
	bool sampled = false;
	bool linear_filter = false;
	bool color_attachment = false;
	bool color_blend = false;
	bool transfer_src = false;
	bool transfer_dst = false;
	Size max_size = Size(0, 0);
	uint64 max_resource_bytes = 0;
};

struct GpuShaderDesc : Moveable<GpuShaderDesc> {
	GpuShaderStage stage = GpuShaderStage::Unknown;
	GpuShaderFormat format = GpuShaderFormat::Unknown;
	String code;
	String entry_point = "main";
	String label;
};

// Optional compute support on the device's existing queue. No second queue is implied.
struct GpuComputeCapabilities : Moveable<GpuComputeCapabilities> {
	uint32 max_group_count[3] = {};
	uint32 max_local_size[3] = {};
	uint32 max_local_invocations = 0;
	uint32 max_storage_buffers = 0;
	uint64 max_storage_range = 0;
	uint64 storage_offset_alignment = 0;
	uint32 max_parameter_bytes = 0;
	uint32 max_dispatches_per_command = 0;
};

struct GpuComputeStats : Moveable<GpuComputeStats> {
	uint64 descriptor_pools_created = 0, descriptor_pools_reused = 0;
	int live_commands = 0, retained_descriptor_pools = 0;
};

// Completed sampled-image descriptor pools are bounded and reusable.
struct GpuSamplingStats : Moveable<GpuSamplingStats> {
	uint64 descriptor_pools_created = 0, descriptor_pools_reused = 0;
	int live_commands = 0, retained_descriptor_pools = 0;
};

struct GpuComputePipelineDesc : Moveable<GpuComputePipelineDesc> {
	GpuShaderId shader;
	int storage_buffer_count = 0; // Set 0, consecutive storage-buffer bindings.
	int parameter_bytes = 0;     // Entire push-constant payload, multiple of 4.
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

// Native channel storage width; Unknown and invalid enum values return zero.
int GpuFormatBytesPerPixel(GpuFormat format);

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

	// Clears out on failure. Providers without physical queries return Unsupported.
	virtual GpuResult GetTextureCapabilities(GpuFormat, int, GpuTextureCapabilities& out) {
		out = GpuTextureCapabilities();
		return GpuResult::Unsupported;
	}
	virtual GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) = 0;
	virtual GpuResult WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size) = 0;
	virtual GpuResult DestroyTexture(GpuTextureId id) = 0;

	// identity denotes immutable tight native-format pixels, unique in the device
	// domain for their lifetime. Each returned handle is independently released
	// with DestroyTexture. Backends may share the allocation, never handle IDs.
	virtual GpuResult AcquireImmutableTexture(uint64 identity, const GpuTextureDesc& desc,
	                                        const void *data, int64 size, GpuTextureId& out,
	                                        bool& uploaded) {
		out = GpuTextureId();
		uploaded = false;
		const int bpp = GpuFormatBytesPerPixel(desc.format);
		if(bpp <= 0 || desc.format == GpuFormat::D24S8) return GpuResult::Unsupported;
		if(!identity || !data || size <= 0 || desc.size.cx <= 0 || desc.size.cy <= 0 ||
		   size / bpp / desc.size.cx < desc.size.cy)
			return GpuResult::InvalidArgument;
		GpuResult result = CreateTexture(desc, out);
		if(result != GpuResult::Ok) return result;
		GpuTextureWriteDesc write;
		write.size = desc.size;
		write.row_pitch = (int64)desc.size.cx * bpp;
		result = WriteTexture(out, write, data, size);
		if(result != GpuResult::Ok) {
			DestroyTexture(out);
			out = GpuTextureId();
		}
		else uploaded = true;
		return result;
	}

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

	// Compute records outside render passes and uses the same command submission.
	// The caller owns shader bounds, source/target aliasing and exclusive queue access.
	// Submit is currently synchronous; these methods do not establish async streaming.
	virtual GpuResult GetComputeCapabilities(GpuComputeCapabilities& out) {
		out = GpuComputeCapabilities(); return GpuResult::Unsupported;
	}
	virtual GpuResult GetSamplingStats(GpuSamplingStats& out) {
		out = GpuSamplingStats(); return GpuResult::Unsupported;
	}
	virtual GpuResult GetComputeStats(GpuComputeStats& out) {
		out = GpuComputeStats(); return GpuResult::Unsupported;
	}
	virtual GpuResult CreateComputePipeline(const GpuComputePipelineDesc&, GpuPipelineId& out) {
		out = GpuPipelineId(); return GpuResult::Unsupported;
	}
	virtual GpuResult SetComputePipeline(GpuCommandListId, GpuPipelineId) { return GpuResult::Unsupported; }
	virtual GpuResult SetStorageBuffer(GpuCommandListId, int, GpuBufferId, int64 = 0, int64 = 0) { return GpuResult::Unsupported; }
	virtual GpuResult SetComputeParameters(GpuCommandListId, const void *, int) { return GpuResult::Unsupported; }
	virtual GpuResult Dispatch(GpuCommandListId, uint32, uint32 = 1, uint32 = 1) { return GpuResult::Unsupported; }
	// GPU-resident copy. Whole uninitialized images must be defined before partial updates.
	virtual GpuResult CopyBufferToTexture(GpuCommandListId, GpuBufferId, int64,
	                                     GpuTextureId, const GpuTextureWriteDesc&) { return GpuResult::Unsupported; }
	// Synchronous diagnostic native-byte read; host-visible TransferSrc required, <=64 MiB.
	// Only previously written bytes are defined. Not a per-frame readback API.
	virtual GpuResult ReadBuffer(GpuBufferId, int64, int64, Vector<byte>& out) {
		out.Clear(); return GpuResult::Unsupported;
	}

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
