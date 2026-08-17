#pragma once

#include <RenderVulkan/RenderVulkanSurfaceSession.h>
#include <memory>

namespace Upp {

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

	GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) override;
	GpuResult WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size) override;
	GpuResult DestroyBuffer(GpuBufferId id) override;

	GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) override;
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
	GpuResult CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out) override;
	GpuResult DestroyPipeline(GpuPipelineId id) override;
	GpuResult BeginCommands(GpuCommandListId& out) override;
	GpuResult BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc) override;
	GpuResult SetPipeline(GpuCommandListId list, GpuPipelineId pipeline) override;
	GpuResult SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer) override;
	GpuResult SetSampledTexture(GpuCommandListId list, int slot, GpuTextureId texture) override;
	GpuResult Draw(GpuCommandListId list, int vertex_count, int first_vertex = 0) override;
	GpuResult EndRenderPass(GpuCommandListId list) override;
	GpuResult EndCommands(GpuCommandListId list) override;
	GpuResult Submit(GpuCommandListId list) override;

private:
	struct Impl;
	struct SampledImpl;
	struct SampledCleanup {
		VulkanGpuDevice *owner = nullptr;
		~SampledCleanup();
	};

	std::unique_ptr<Impl> impl;
	SampledImpl *sampled_impl = nullptr;
	SampledCleanup sampled_cleanup;

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
