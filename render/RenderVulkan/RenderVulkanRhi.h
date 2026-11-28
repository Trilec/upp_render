#pragma once

#include <RenderVulkan/RenderVulkanSurfaceSession.h>
#include <memory>

namespace Upp {

// Read-only snapshot of this adapter's existing surface session and actual
// selected physical device. Call on the owning presentation thread while the
// device is alive; it never acquires/presents, selects another GPU or changes WSI.
struct VulkanPresentationState {
	VulkanSurfaceReport surface;
	VulkanFrameReport frame;
	bool device_identity_available = false;
	bool device_luid_valid = false;
	byte device_luid[VK_LUID_SIZE]{};
	byte device_uuid[VK_UUID_SIZE]{};
	uint32_t device_node_mask = 0;
	String device_identity_error;
};

// VulkanGpuDevice borrows Vulkan ownership from an already-open surface session.
// Keep the session alive and ready until this adapter and its resources are destroyed.
class VulkanGpuDevice final : public GpuDevice {
public:
	explicit VulkanGpuDevice(VulkanSurfaceSession& session);
	~VulkanGpuDevice();

	VulkanGpuDevice(const VulkanGpuDevice&) = delete;
	VulkanGpuDevice& operator=(const VulkanGpuDevice&) = delete;
	VulkanGpuDevice(VulkanGpuDevice&&) = delete;
	VulkanGpuDevice& operator=(VulkanGpuDevice&&) = delete;

	bool IsReady() const;
	const String& GetError() const;
	bool QueryPresentationState(VulkanPresentationState& out) const;
	// Opt-in diagnostics for one command list at a time on the owning thread.
	// Reports the last successfully completed timed command buffer.
	GpuResult SetGpuSubmissionTiming(bool enable);
	bool GetLastGpuSubmissionMilliseconds(double& out) const;
	// VkMemoryRequirements-sized exclusive allocations owned by this adapter.
	// Shared images and vertex buffers have separate allocation counters.
	// Excludes driver/swapchain memory and temporary upload staging.
	uint64 GetLiveAllocationBytes() const;
	// Immutable buffer bytes shared across adapters on this native device.
	uint64 GetSharedImmutableBufferBytes() const;
	int GetLiveBufferCount() const;
	int GetLiveTextureCount() const;
	int GetLiveShaderCount() const;
	int GetLivePipelineCount() const;
	int GetLiveCommandCount() const;
	int GetLiveSurfaceCount() const;
	int GetLiveSwapchainCount() const;
	int GetLiveFrameCount() const;

	GpuDeviceId GetDeviceId() const override;
	GpuBackendKind GetBackendKind() const override;
	GpuAdapterInfo GetAdapterInfo() const override;
	String GetLastError() const override;

	GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) override;
	GpuResult WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size) override;
	GpuResult DestroyBuffer(GpuBufferId id) override;
	GpuResult AcquireImmutableBuffer(const std::shared_ptr<const void>& owner,
	                                       uint32 format_version, int usage, int64 size,
	                                       const GpuImmutableBufferFill& fill,
	                                       GpuBufferId& out, bool& uploaded) override;

	// Synchronous diagnostic readback; call only with exclusive queue access and no open commands.
	// Owned initialized colour textures with TransferSrc only; tightly packed native bytes.
	// Preserves raw UNORM/HALF/FLOAT channel storage; 64 MiB output bound.
	bool ReadTexturePixels(GpuTextureId id, Vector<byte>& pixels);

	GpuResult GetTextureCapabilities(GpuFormat format, int usage, GpuTextureCapabilities& out) override;
	GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) override;
	GpuResult AcquireImmutableTexture(uint64 identity, const GpuTextureDesc& desc,
	                                 const void *data, int64 size, GpuTextureId& out,
	                                 bool& uploaded) override;
	static uint64 GetSharedImmutableAllocationBytes();
	GpuResult WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size) override;
	GpuResult DestroyTexture(GpuTextureId id) override;

	GpuResult CreateSurface(const GpuSurfaceDesc& desc, GpuSurfaceId& out) override;
	GpuResult DestroySurface(GpuSurfaceId id) override;
	GpuResult CreateSwapchain(const GpuSwapchainDesc& desc, GpuSwapchainId& out) override;
	GpuResult DestroySwapchain(GpuSwapchainId id) override;
	GpuResult ResizeSwapchain(GpuSwapchainId id, Size size) override;
	GpuResult BeginFrame(GpuSwapchainId swapchain, GpuFrameInfo& out) override;
	GpuResult Present(GpuFrameId frame) override;

	GpuResult CreateShader(const GpuShaderDesc& desc, GpuShaderId& out) override;
	GpuResult DestroyShader(GpuShaderId id) override;
	GpuResult GetComputeCapabilities(GpuComputeCapabilities& out) override;
	GpuResult GetComputeStats(GpuComputeStats& out) override;
	GpuResult GetSamplingStats(GpuSamplingStats& out) override;
	GpuResult CreateComputePipeline(const GpuComputePipelineDesc& desc, GpuPipelineId& out) override;
	GpuResult SetComputePipeline(GpuCommandListId list, GpuPipelineId pipeline) override;
	GpuResult SetStorageBuffer(GpuCommandListId list, int slot, GpuBufferId buffer, int64 offset = 0, int64 size = 0) override;
	GpuResult SetComputeParameters(GpuCommandListId list, const void *data, int size) override;
	GpuResult Dispatch(GpuCommandListId list, uint32 x, uint32 y = 1, uint32 z = 1) override;
	GpuResult DispatchIndirect(GpuCommandListId, GpuBufferId, int64 offset = 0) override;
	GpuResult CopyBuffer(GpuCommandListId list, GpuBufferId source, int64 source_offset,
	                     GpuBufferId target, int64 target_offset, int64 size) override;
	GpuResult CopyBufferToTexture(GpuCommandListId list, GpuBufferId source, int64 offset,
	                             GpuTextureId target, const GpuTextureWriteDesc& region) override;
	GpuResult ReadBuffer(GpuBufferId buffer, int64 offset, int64 size, Vector<byte>& out) override;

	GpuResult CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out) override;
	GpuResult DestroyPipeline(GpuPipelineId id) override;
	GpuResult BeginCommands(GpuCommandListId& out) override;
	GpuResult BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc) override;
	GpuResult SetPipeline(GpuCommandListId list, GpuPipelineId pipeline) override;
	GpuResult SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer) override;
	GpuResult SetIndexBuffer(GpuCommandListId list, GpuBufferId buffer, GpuIndexType type) override;
	GpuResult SetGraphicsParameters(GpuCommandListId list, const void *data, int size) override;
	GpuResult DrawIndexed(GpuCommandListId list, int index_count, int first_index = 0) override;
	GpuResult GetVertexPullCapabilities(GpuVertexPullCapabilities&) override;
	GpuResult SetVertexStorageBuffer(GpuCommandListId, int slot, GpuBufferId, int64 offset = 0, int64 size = 0) override;
	GpuResult DrawInstanced(GpuCommandListId, int vertices, int instances, int first_vertex = 0) override;
	GpuResult DrawIndirect(GpuCommandListId, GpuBufferId, int64 offset = 0) override;
	GpuResult DiscardCommands(GpuCommandListId list) override;
	GpuResult SetSampledTexture(GpuCommandListId list, int slot, GpuTextureId texture) override;
	GpuResult Draw(GpuCommandListId list, int vertex_count, int first_vertex = 0) override;
	GpuResult EndRenderPass(GpuCommandListId list) override;
	GpuResult EndCommands(GpuCommandListId list) override;
	GpuResult Submit(GpuCommandListId list) override;

private:
	struct Impl;
	struct SampledImpl;
	struct ComputeImpl;
	struct SampledCleanup {
		VulkanGpuDevice *owner = nullptr;
		~SampledCleanup();
	};

	std::unique_ptr<Impl> impl;
	SampledImpl *sampled_impl = nullptr;
	SampledCleanup sampled_cleanup;

	ComputeImpl *compute_impl = nullptr;
	ComputeImpl& Compute();
	void DestroyComputeExtension();
	int GetComputePipelineCount() const;
	bool ComputeUsesShader(GpuShaderId shader) const;
	bool IsComputePipeline(GpuPipelineId pipeline) const;
	GpuResult DestroyComputePipeline(GpuPipelineId pipeline);
	void CompleteComputeCommands(GpuCommandListId list);
	GpuResult RecordDispatch(GpuCommandListId, uint32, uint32, uint32, GpuBufferId indirect, int64 offset);
	GpuResult BindVertexStorageForDraw(GpuCommandListId);

	SampledImpl& Sampled();
	void DestroySampledExtension();

	// These names are the byte-preserved Stage-3/Stage-4 implementation entry
	// points. RenderVulkanRhi.cpp wraps only the sampled-image-sensitive methods.
	int GetLivePipelineCountBase() const;
	GpuResult DestroyShaderBase(GpuShaderId id);
	GpuResult CreatePipelineBase(const GpuPipelineDesc& desc, GpuPipelineId& out);
	GpuResult DestroyPipelineBase(GpuPipelineId id);
	GpuResult BeginRenderPassBase(GpuCommandListId list, const GpuRenderPassDesc& desc);
	GpuResult SetPipelineBase(GpuCommandListId list, GpuPipelineId pipeline);
	GpuResult DrawBase(GpuCommandListId list, int vertex_count, int first_vertex = 0);
	GpuResult SubmitBase(GpuCommandListId list);
};

}
