#pragma once

#include <RenderRhi/RenderRhi.h>

namespace Upp {

class NullGpuDevice final : public GpuDevice {
public:
	NullGpuDevice();
	explicit NullGpuDevice(const GpuDeviceDesc& desc);

	GpuDeviceId GetDeviceId() const override;
	GpuBackendKind GetBackendKind() const override;
	GpuAdapterInfo GetAdapterInfo() const override;

	GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) override;
	GpuResult DestroyBuffer(GpuBufferId id) override;

	GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) override;
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

	String DumpLog() const;

private:
	struct BufferState : Moveable<BufferState> {
		GpuBufferDesc desc;
		bool alive = true;
	};

	struct TextureState : Moveable<TextureState> {
		GpuTextureDesc desc;
		bool alive = true;
		bool swapchain_backbuffer = false;
		GpuSwapchainId owner_swapchain;
		GpuFrameId owner_frame;
		bool renderable = true;
	};

	struct SurfaceState : Moveable<SurfaceState> {
		GpuSurfaceDesc desc;
		bool alive = true;
		int live_swapchains = 0;
	};

	struct SwapchainState : Moveable<SwapchainState> {
		GpuSwapchainDesc desc;
		bool alive = true;
		GpuTextureId backbuffer;
		GpuFrameId active_frame;
	};

	struct FrameState : Moveable<FrameState> {
		GpuSwapchainId swapchain;
		GpuTextureId color_target;
		bool active = true;
		bool presented = false;
	};

	struct CommandState : Moveable<CommandState> {
		bool begun = false;
		bool render_pass_active = false;
		bool ended = false;
		bool submitted = false;
		GpuPipelineId pipeline;
		GpuBufferId vertex_buffer;
		int draw_count = 0;
		GpuRenderPassDesc pass_desc;
	};

	struct PipelineState : Moveable<PipelineState> {
		GpuPipelineDesc desc;
		bool alive = true;
	};

	GpuDeviceDesc device_desc;
	GpuDeviceId device_id;
	GpuAdapterInfo adapter_info;
	int next_buffer_id = 1;
	int next_texture_id = 1;
	int next_surface_id = 1;
	int next_swapchain_id = 1;
	int next_frame_id = 1;
	int next_pipeline_id = 1;
	int next_command_list_id = 1;
	VectorMap<int, SurfaceState> surfaces;
	VectorMap<int, SwapchainState> swapchains;
	VectorMap<int, FrameState> frames;
	VectorMap<int, BufferState> buffers;
	VectorMap<int, TextureState> textures;
	VectorMap<int, PipelineState> pipelines;
	VectorMap<int, CommandState> command_lists;
	Vector<String> log;
	GpuCommandListId active_command_list;

	void AppendLog(const String& line);
	void Fail(const String& line);
	bool CheckSurfaceExists(GpuSurfaceId id) const;
	bool CheckSwapchainExists(GpuSwapchainId id) const;
	bool CheckFrameExists(GpuFrameId id) const;
	bool CheckBufferExists(GpuBufferId id) const;
	bool CheckTextureExists(GpuTextureId id) const;
	bool CheckPipelineExists(GpuPipelineId id) const;
	TextureState *FindTextureState(GpuTextureId id);
	const TextureState *FindTextureState(GpuTextureId id) const;
	SwapchainState *FindSwapchainState(GpuSwapchainId id);
	const SwapchainState *FindSwapchainState(GpuSwapchainId id) const;
	FrameState *FindFrameState(GpuFrameId id);
	const FrameState *FindFrameState(GpuFrameId id) const;
	CommandState *FindCommandState(GpuCommandListId id);
	const CommandState *FindCommandState(GpuCommandListId id) const;
	bool CanUseCommandList(GpuCommandListId id, const CommandState*& out_state, String& reason) const;
};

}
