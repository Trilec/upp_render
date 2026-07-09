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

	GpuResult BeginCommands(GpuCommandListId& out) override;
	GpuResult BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc) override;
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
	};

	struct CommandState : Moveable<CommandState> {
		bool begun = false;
		bool render_pass_active = false;
		bool ended = false;
		bool submitted = false;
		GpuRenderPassDesc pass_desc;
	};

	GpuDeviceDesc device_desc;
	GpuDeviceId device_id;
	GpuAdapterInfo adapter_info;
	int next_buffer_id = 1;
	int next_texture_id = 1;
	int next_command_list_id = 1;
	VectorMap<int, BufferState> buffers;
	VectorMap<int, TextureState> textures;
	VectorMap<int, CommandState> command_lists;
	Vector<String> log;
	GpuCommandListId active_command_list;

	void AppendLog(const String& line);
	void Fail(const String& line);
	bool CheckBufferExists(GpuBufferId id) const;
	bool CheckTextureExists(GpuTextureId id) const;
	CommandState *FindCommandState(GpuCommandListId id);
	const CommandState *FindCommandState(GpuCommandListId id) const;
	bool CanUseCommandList(GpuCommandListId id, const CommandState*& out_state, String& reason) const;
};

}
