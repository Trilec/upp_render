#pragma once

#include <RenderVulkan/RenderVulkanSurfaceSession.h>
#include <memory>

namespace Upp {

// S17B borrows Vulkan ownership from an already-open surface session.
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
	GpuResult CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out) override;
	GpuResult DestroyPipeline(GpuPipelineId id) override;
	GpuResult BeginCommands(GpuCommandListId& out) override;
	GpuResult BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc) override;
	GpuResult SetPipeline(GpuCommandListId list, GpuPipelineId pipeline) override;
	GpuResult SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer) override;
	GpuResult Draw(GpuCommandListId list, int vertex_count, int first_vertex = 0) override;
	GpuResult EndRenderPass(GpuCommandListId list) override;
	GpuResult EndCommands(GpuCommandListId list) override;
	GpuResult Submit(GpuCommandListId list) override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}
