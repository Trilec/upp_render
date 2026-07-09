#include "RenderNull.h"

namespace Upp {

static String ResultLine(const String& op, GpuResult result)
{
	return op + " result=" + DumpGpuResult(result);
}

NullGpuDevice::NullGpuDevice()
	: NullGpuDevice(GpuDeviceDesc())
{
}

NullGpuDevice::NullGpuDevice(const GpuDeviceDesc& desc)
{
	device_desc = desc;
	device_id.value = 1;
	adapter_info.adapter_id.value = 1;
	adapter_info.device_id = device_id;
	adapter_info.backend_kind = GpuBackendKind::Null;
	adapter_info.name = desc.label.IsEmpty() ? "NullGpuDevice" : desc.label;
	adapter_info.capability_flags = GpuCapability_Buffers | GpuCapability_Textures | GpuCapability_RenderPass | GpuCapability_Pipelines;
	AppendLog("CreateDevice id=" + device_id.Dump() + " backend=" + DumpGpuBackendKind(GetBackendKind()));
}

void NullGpuDevice::AppendLog(const String& line)
{
	log.Add(line);
}

void NullGpuDevice::Fail(const String& line)
{
	AppendLog("FAIL " + line);
}

bool NullGpuDevice::CheckBufferExists(GpuBufferId id) const
{
	return id.IsValid() && buffers.Find(id.value) >= 0;
}

bool NullGpuDevice::CheckTextureExists(GpuTextureId id) const
{
	return id.IsValid() && textures.Find(id.value) >= 0;
}

NullGpuDevice::CommandState *NullGpuDevice::FindCommandState(GpuCommandListId id)
{
	int index = command_lists.Find(id.value);
	return index >= 0 ? &command_lists[index] : nullptr;
}

const NullGpuDevice::CommandState *NullGpuDevice::FindCommandState(GpuCommandListId id) const
{
	int index = command_lists.Find(id.value);
	return index >= 0 ? &command_lists[index] : nullptr;
}

bool NullGpuDevice::CanUseCommandList(GpuCommandListId id, const CommandState*& out_state, String& reason) const
{
	if(!id.IsValid()) {
		reason = "invalid command list";
		return false;
	}
	const CommandState *state = FindCommandState(id);
	if(!state) {
		reason = "unknown command list";
		return false;
	}
	if(state->submitted) {
		reason = "command list already submitted";
		return false;
	}
	if(!state->begun) {
		reason = "command list not begun";
		return false;
	}
	out_state = state;
	return true;
}

GpuDeviceId NullGpuDevice::GetDeviceId() const
{
	return device_id;
}

GpuBackendKind NullGpuDevice::GetBackendKind() const
{
	return GpuBackendKind::Null;
}

GpuAdapterInfo NullGpuDevice::GetAdapterInfo() const
{
	return adapter_info;
}

GpuResult NullGpuDevice::CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out)
{
	if(desc.size <= 0) {
		Fail("CreateBuffer size=" + AsString((int64)desc.size) + " usage=" + DumpGpuBufferUsage(desc.usage) + " reason=zero_size");
		out = GpuBufferId();
		return GpuResult::InvalidArgument;
	}
	GpuBufferId id;
	id.value = next_buffer_id++;
	BufferState& state = buffers.Add(id.value, BufferState());
	state.desc = desc;
	state.alive = true;
	out = id;
	AppendLog("CreateBuffer id=" + id.Dump() + " size=" + AsString((int64)desc.size) + " usage=" + DumpGpuBufferUsage(desc.usage));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroyBuffer(GpuBufferId id)
{
	int index = buffers.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroyBuffer id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	buffers.Remove(index);
	AppendLog("DestroyBuffer id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out)
{
	if(desc.size.cx <= 0 || desc.size.cy <= 0) {
		Fail("CreateTexture size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " format=" + DumpGpuFormat(desc.format) + " reason=invalid_size");
		out = GpuTextureId();
		return GpuResult::InvalidArgument;
	}
	if(desc.format == GpuFormat::Unknown) {
		Fail("CreateTexture size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " format=Unknown reason=invalid_format");
		out = GpuTextureId();
		return GpuResult::InvalidArgument;
	}
	GpuTextureId id;
	id.value = next_texture_id++;
	TextureState& state = textures.Add(id.value, TextureState());
	state.desc = desc;
	state.alive = true;
	out = id;
	AppendLog("CreateTexture id=" + id.Dump() + " size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " format=" + DumpGpuFormat(desc.format) + " usage=" + DumpGpuTextureUsage(desc.usage));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroyTexture(GpuTextureId id)
{
	int index = textures.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroyTexture id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	textures.Remove(index);
	AppendLog("DestroyTexture id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::BeginCommands(GpuCommandListId& out)
{
	if(active_command_list.IsValid()) {
		Fail("BeginCommands reason=command_list_already_active");
		out = GpuCommandListId();
		return GpuResult::InvalidState;
	}
	GpuCommandListId id;
	id.value = next_command_list_id++;
	CommandState& state = command_lists.Add(id.value, CommandState());
	state.begun = true;
	state.render_pass_active = false;
	state.ended = false;
	state.submitted = false;
	active_command_list = id;
	out = id;
	AppendLog("BeginCommands id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("BeginRenderPass list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(mutable_state.render_pass_active) {
		Fail("BeginRenderPass list=" + list.Dump() + " reason=render_pass_already_active");
		return GpuResult::InvalidState;
	}
	if(!CheckTextureExists(desc.color_target)) {
		Fail("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " reason=unknown_texture");
		return GpuResult::InvalidHandle;
	}
	const TextureState *texture = &textures[textures.Find(desc.color_target.value)];
	if(desc.color_format == GpuFormat::Unknown || desc.color_format != texture->desc.format) {
		Fail("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " format=" + DumpGpuFormat(desc.color_format) + " reason=format_mismatch");
		return GpuResult::InvalidArgument;
	}
	mutable_state.render_pass_active = true;
	mutable_state.pass_desc = desc;
	AppendLog("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " format=" + DumpGpuFormat(desc.color_format) + " load=" + DumpGpuLoadOp(desc.color_load) + " store=" + DumpGpuStoreOp(desc.color_store));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::EndRenderPass(GpuCommandListId list)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("EndRenderPass list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(!mutable_state.render_pass_active) {
		Fail("EndRenderPass list=" + list.Dump() + " reason=no_active_render_pass");
		return GpuResult::InvalidState;
	}
	mutable_state.render_pass_active = false;
	AppendLog("EndRenderPass list=" + list.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::EndCommands(GpuCommandListId list)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("EndCommands list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(mutable_state.render_pass_active) {
		Fail("EndCommands list=" + list.Dump() + " reason=render_pass_still_active");
		return GpuResult::InvalidState;
	}
	if(mutable_state.ended) {
		Fail("EndCommands list=" + list.Dump() + " reason=already_ended");
		return GpuResult::InvalidState;
	}
	mutable_state.ended = true;
	active_command_list = GpuCommandListId();
	AppendLog("EndCommands id=" + list.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::Submit(GpuCommandListId list)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("Submit list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(!mutable_state.ended) {
		Fail("Submit list=" + list.Dump() + " reason=unfinished_command_list");
		return GpuResult::InvalidState;
	}
	if(mutable_state.submitted) {
		Fail("Submit list=" + list.Dump() + " reason=already_submitted");
		return GpuResult::InvalidState;
	}
	mutable_state.submitted = true;
	AppendLog("Submit id=" + list.Dump());
	return GpuResult::Ok;
}

String NullGpuDevice::DumpLog() const
{
	String out;
	for(int i = 0; i < log.GetCount(); ++i) {
		if(i)
			out << '\n';
		out << log[i];
	}
	return out;
}

}
