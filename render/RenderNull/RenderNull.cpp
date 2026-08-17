#include "RenderNull.h"

#include <cmath>

namespace Upp {

static bool IsValidTopology(GpuPrimitiveTopology topology)
{
	switch(topology) {
	case GpuPrimitiveTopology::TriangleList:
	case GpuPrimitiveTopology::LineList:
		return true;
	}
	return false;
}

static bool IsValidShaderStage(GpuShaderStage stage)
{
	switch(stage) {
	case GpuShaderStage::Vertex:
	case GpuShaderStage::Fragment:
		return true;
	case GpuShaderStage::Unknown:
		return false;
	}
	return false;
}

static bool IsValidShaderFormat(GpuShaderFormat format)
{
	return format == GpuShaderFormat::SpirV;
}

static bool IsValidVertexLayout(GpuVertexLayout layout)
{
	return layout == GpuVertexLayout::Position2Color4F ||
	       layout == GpuVertexLayout::Position2Uv2Color4F;
}

static bool IsValidSamplerFilter(GpuSamplerFilter filter)
{
	return filter == GpuSamplerFilter::Nearest || filter == GpuSamplerFilter::Linear;
}

static bool IsValidSamplerAddressMode(GpuSamplerAddressMode mode)
{
	return mode == GpuSamplerAddressMode::ClampToEdge || mode == GpuSamplerAddressMode::Repeat;
}

static bool IsValidLoadOp(GpuLoadOp op)
{
	switch(op) {
	case GpuLoadOp::Load:
	case GpuLoadOp::Clear:
	case GpuLoadOp::DontCare:
		return true;
	}
	return false;
}

static bool IsValidStoreOp(GpuStoreOp op)
{
	switch(op) {
	case GpuStoreOp::Store:
	case GpuStoreOp::DontCare:
		return true;
	}
	return false;
}

static bool IsFiniteClearColor(const GpuClearColor& color)
{
	return std::isfinite(color.red) && std::isfinite(color.green) &&
	       std::isfinite(color.blue) && std::isfinite(color.alpha);
}

static bool HasSpirVMagic(const String& code)
{
	if(code.GetCount() < 4 || (code.GetCount() & 3) != 0)
		return false;
	const byte *p = reinterpret_cast<const byte *>(code.Begin());
	const uint32 magic = (uint32)p[0] |
	                     ((uint32)p[1] << 8) |
	                     ((uint32)p[2] << 16) |
	                     ((uint32)p[3] << 24);
	return magic == 0x07230203u;
}

static int BytesPerPixel(GpuFormat format)
{
	switch(format) {
	case GpuFormat::RGBA8:
	case GpuFormat::BGRA8:
	case GpuFormat::RGBA8Srgb:
	case GpuFormat::BGRA8Srgb:
	case GpuFormat::D24S8:
		return 4;
	case GpuFormat::R16F:
		return 2;
	case GpuFormat::Unknown:
		return 0;
	}
	return 0;
}

static bool ValidateNativeWindowDesc(const GpuSurfaceDesc& desc, String& reason)
{
	switch(desc.native_window.kind) {
	case GpuNativeWindowKind::None:
		if(desc.native_window.handle != 0) {
			reason = "none_window_handle_not_zero";
			return false;
		}
		return true;
	case GpuNativeWindowKind::Win32:
		if(desc.native_window.handle == 0) {
			reason = "win32_window_handle_zero";
			return false;
		}
		return true;
	}
	reason = "unknown_native_window_kind";
	return false;
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
	adapter_info.capability_flags = GpuCapability_Buffers | GpuCapability_Textures | GpuCapability_RenderPass |
	                                GpuCapability_Pipelines | GpuCapability_Shaders;
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

bool NullGpuDevice::CheckSurfaceExists(GpuSurfaceId id) const
{
	return id.IsValid() && surfaces.Find(id.value) >= 0;
}

bool NullGpuDevice::CheckSwapchainExists(GpuSwapchainId id) const
{
	return id.IsValid() && swapchains.Find(id.value) >= 0;
}

bool NullGpuDevice::CheckFrameExists(GpuFrameId id) const
{
	return id.IsValid() && frames.Find(id.value) >= 0;
}

bool NullGpuDevice::CheckShaderExists(GpuShaderId id) const
{
	return id.IsValid() && shaders.Find(id.value) >= 0;
}

NullGpuDevice::TextureState *NullGpuDevice::FindTextureState(GpuTextureId id)
{
	int index = textures.Find(id.value);
	return index >= 0 ? &textures[index] : nullptr;
}

const NullGpuDevice::TextureState *NullGpuDevice::FindTextureState(GpuTextureId id) const
{
	int index = textures.Find(id.value);
	return index >= 0 ? &textures[index] : nullptr;
}

NullGpuDevice::SwapchainState *NullGpuDevice::FindSwapchainState(GpuSwapchainId id)
{
	int index = swapchains.Find(id.value);
	return index >= 0 ? &swapchains[index] : nullptr;
}

const NullGpuDevice::SwapchainState *NullGpuDevice::FindSwapchainState(GpuSwapchainId id) const
{
	int index = swapchains.Find(id.value);
	return index >= 0 ? &swapchains[index] : nullptr;
}

NullGpuDevice::FrameState *NullGpuDevice::FindFrameState(GpuFrameId id)
{
	int index = frames.Find(id.value);
	return index >= 0 ? &frames[index] : nullptr;
}

const NullGpuDevice::FrameState *NullGpuDevice::FindFrameState(GpuFrameId id) const
{
	int index = frames.Find(id.value);
	return index >= 0 ? &frames[index] : nullptr;
}

NullGpuDevice::ShaderState *NullGpuDevice::FindShaderState(GpuShaderId id)
{
	int index = shaders.Find(id.value);
	return index >= 0 ? &shaders[index] : nullptr;
}

const NullGpuDevice::ShaderState *NullGpuDevice::FindShaderState(GpuShaderId id) const
{
	int index = shaders.Find(id.value);
	return index >= 0 ? &shaders[index] : nullptr;
}

bool NullGpuDevice::CheckPipelineExists(GpuPipelineId id) const
{
	return id.IsValid() && pipelines.Find(id.value) >= 0;
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
		reason = "command_list_already_submitted";
		return false;
	}
	if(state->ended) {
		reason = "command_list_already_ended";
		return false;
	}
	if(!state->begun) {
		reason = "command_list_not_begun";
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

GpuResult NullGpuDevice::WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size)
{
	int index = buffers.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("WriteBuffer id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	if(offset < 0 || size <= 0 || data == nullptr) {
		Fail("WriteBuffer id=" + id.Dump() + " offset=" + AsString(offset) + " size=" + AsString(size) + " reason=invalid_write");
		return GpuResult::InvalidArgument;
	}
	const int64 buffer_size = buffers[index].desc.size;
	if(offset > buffer_size || size > buffer_size - offset) {
		Fail("WriteBuffer id=" + id.Dump() + " offset=" + AsString(offset) + " size=" + AsString(size) + " reason=out_of_range");
		return GpuResult::InvalidArgument;
	}
	AppendLog("WriteBuffer id=" + id.Dump() + " offset=" + AsString(offset) + " size=" + AsString(size));
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

GpuResult NullGpuDevice::WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size)
{
	int index = textures.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("WriteTexture id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	const TextureState& state = textures[index];
	if(state.swapchain_backbuffer) {
		Fail("WriteTexture id=" + id.Dump() + " reason=swapchain_backbuffer");
		return GpuResult::InvalidState;
	}
	if(data == nullptr || data_size <= 0 || desc.origin.x < 0 || desc.origin.y < 0 ||
	   desc.size.cx <= 0 || desc.size.cy <= 0 || desc.row_pitch <= 0) {
		Fail("WriteTexture id=" + id.Dump() + " reason=invalid_write");
		return GpuResult::InvalidArgument;
	}
	if(desc.origin.x > state.desc.size.cx - desc.size.cx || desc.origin.y > state.desc.size.cy - desc.size.cy) {
		Fail("WriteTexture id=" + id.Dump() + " reason=out_of_range");
		return GpuResult::InvalidArgument;
	}
	const int bytes_per_pixel = BytesPerPixel(state.desc.format);
	if(bytes_per_pixel <= 0) {
		Fail("WriteTexture id=" + id.Dump() + " reason=unsupported_format");
		return GpuResult::Unsupported;
	}
	const int64 tight_row = (int64)desc.size.cx * bytes_per_pixel;
	if(desc.row_pitch < tight_row) {
		Fail("WriteTexture id=" + id.Dump() + " row_pitch=" + AsString(desc.row_pitch) + " reason=row_pitch_too_small");
		return GpuResult::InvalidArgument;
	}
	if(desc.size.cy > 1 && desc.row_pitch > (INT64_MAX - tight_row) / (desc.size.cy - 1)) {
		Fail("WriteTexture id=" + id.Dump() + " reason=layout_overflow");
		return GpuResult::InvalidArgument;
	}
	const int64 required_size = desc.row_pitch * (desc.size.cy - 1) + tight_row;
	if(data_size < required_size) {
		Fail("WriteTexture id=" + id.Dump() + " data_size=" + AsString(data_size) + " required=" + AsString(required_size) + " reason=data_too_small");
		return GpuResult::InvalidArgument;
	}
	AppendLog("WriteTexture id=" + id.Dump() + " origin=" + AsString(desc.origin.x) + "," + AsString(desc.origin.y) +
	          " size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " row_pitch=" + AsString(desc.row_pitch));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroyTexture(GpuTextureId id)
{
	int index = textures.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroyTexture id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	if(textures[index].swapchain_backbuffer) {
		Fail("DestroyTexture id=" + id.Dump() + " reason=swapchain_backbuffer");
		return GpuResult::InvalidState;
	}
	for(int i = 0; i < command_lists.GetCount(); ++i) {
		if(command_lists[i].submitted)
			continue;
		for(const GpuTextureId& referenced : command_lists[i].referenced_sampled_textures)
			if(referenced == id) {
				Fail("DestroyTexture id=" + id.Dump() + " reason=referenced_by_command_list");
				return GpuResult::InvalidState;
			}
	}
	textures.Remove(index);
	AppendLog("DestroyTexture id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::CreateSurface(const GpuSurfaceDesc& desc, GpuSurfaceId& out)
{
	if(desc.size.cx <= 0 || desc.size.cy <= 0) {
		Fail("CreateSurface size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " reason=invalid_size");
		out = GpuSurfaceId();
		return GpuResult::InvalidArgument;
	}
	String native_reason;
	if(!ValidateNativeWindowDesc(desc, native_reason)) {
		Fail("CreateSurface native_window=" + DumpGpuNativeWindowDesc(desc.native_window) + " reason=" + native_reason);
		out = GpuSurfaceId();
		return GpuResult::InvalidArgument;
	}
	GpuSurfaceId id;
	id.value = next_surface_id++;
	SurfaceState& state = surfaces.Add(id.value, SurfaceState());
	state.desc = desc;
	state.alive = true;
	state.live_swapchains = 0;
	out = id;
	AppendLog("CreateSurface id=" + id.Dump() + " size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " native_window=" + DumpGpuNativeWindowDesc(desc.native_window));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroySurface(GpuSurfaceId id)
{
	int index = surfaces.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroySurface id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	if(surfaces[index].live_swapchains > 0) {
		Fail("DestroySurface id=" + id.Dump() + " reason=live_swapchain");
		return GpuResult::InvalidState;
	}
	surfaces.Remove(index);
	AppendLog("DestroySurface id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::CreateSwapchain(const GpuSwapchainDesc& desc, GpuSwapchainId& out)
{
	if(!CheckSurfaceExists(desc.surface)) {
		Fail("CreateSwapchain surface=" + desc.surface.Dump() + " reason=unknown_surface");
		out = GpuSwapchainId();
		return GpuResult::InvalidHandle;
	}
	if(desc.size.cx <= 0 || desc.size.cy <= 0) {
		Fail("CreateSwapchain surface=" + desc.surface.Dump() + " size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " reason=invalid_size");
		out = GpuSwapchainId();
		return GpuResult::InvalidArgument;
	}
	if(desc.color_format == GpuFormat::Unknown) {
		Fail("CreateSwapchain surface=" + desc.surface.Dump() + " format=Unknown reason=invalid_format");
		out = GpuSwapchainId();
		return GpuResult::InvalidArgument;
	}
	if(desc.image_count < 2) {
		Fail("CreateSwapchain surface=" + desc.surface.Dump() + " images=" + AsString(desc.image_count) + " reason=image_count_too_small");
		out = GpuSwapchainId();
		return GpuResult::InvalidArgument;
	}
	GpuSwapchainId id;
	id.value = next_swapchain_id++;
	SwapchainState& state = swapchains.Add(id.value, SwapchainState());
	state.desc = desc;
	state.alive = true;
	state.active_frame = GpuFrameId();

	GpuTextureId backbuffer;
	backbuffer.value = next_texture_id++;
	TextureState& tex = textures.Add(backbuffer.value, TextureState());
	tex.desc.size = desc.size;
	tex.desc.format = desc.color_format;
	tex.desc.usage = GpuTextureUsage_ColorAttachment;
	tex.desc.label = desc.label;
	tex.alive = true;
	tex.swapchain_backbuffer = true;
	tex.owner_swapchain = id;
	tex.owner_frame = GpuFrameId();
	tex.renderable = false;

	state.backbuffer = backbuffer;
	int surface_index = surfaces.Find(desc.surface.value);
	if(surface_index >= 0)
		surfaces[surface_index].live_swapchains++;
	out = id;
	AppendLog("CreateSwapchain id=" + id.Dump() + " surface=" + desc.surface.Dump() + " size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " format=" + DumpGpuFormat(desc.color_format) + " images=" + AsString(desc.image_count));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroySwapchain(GpuSwapchainId id)
{
	int index = swapchains.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroySwapchain id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	SwapchainState& state = swapchains[index];
	if(state.active_frame.IsValid()) {
		Fail("DestroySwapchain id=" + id.Dump() + " reason=active_frame");
		return GpuResult::InvalidState;
	}
	int surface_index = surfaces.Find(state.desc.surface.value);
	if(surface_index >= 0)
		surfaces[surface_index].live_swapchains--;
	int tex_index = textures.Find(state.backbuffer.value);
	if(tex_index >= 0)
		textures.Remove(tex_index);
	swapchains.Remove(index);
	AppendLog("DestroySwapchain id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::ResizeSwapchain(GpuSwapchainId id, Size size)
{
	int index = swapchains.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("ResizeSwapchain id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	SwapchainState& state = swapchains[index];
	if(state.active_frame.IsValid()) {
		Fail("ResizeSwapchain id=" + id.Dump() + " reason=frame_active");
		return GpuResult::InvalidState;
	}
	if(size.cx <= 0 || size.cy <= 0) {
		Fail("ResizeSwapchain id=" + id.Dump() + " size=" + AsString(size.cx) + "x" + AsString(size.cy) + " reason=invalid_size");
		return GpuResult::InvalidArgument;
	}
	state.desc.size = size;
	TextureState *tex = FindTextureState(state.backbuffer);
	if(tex) {
		tex->desc.size = size;
		tex->renderable = false;
	}
	AppendLog("ResizeSwapchain id=" + id.Dump() + " size=" + AsString(size.cx) + "x" + AsString(size.cy));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::BeginFrame(GpuSwapchainId swapchain, GpuFrameInfo& out)
{
	SwapchainState *state = FindSwapchainState(swapchain);
	if(!swapchain.IsValid() || !state) {
		Fail("BeginFrame swapchain=" + swapchain.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	if(state->active_frame.IsValid()) {
		Fail("BeginFrame swapchain=" + swapchain.Dump() + " reason=frame_already_active");
		return GpuResult::InvalidState;
	}
	TextureState *tex = FindTextureState(state->backbuffer);
	if(!tex || !tex->alive) {
		Fail("BeginFrame swapchain=" + swapchain.Dump() + " reason=missing_backbuffer");
		return GpuResult::InvalidState;
	}
	GpuFrameId frame;
	frame.value = next_frame_id++;
	FrameState& frame_state = frames.Add(frame.value, FrameState());
	frame_state.swapchain = swapchain;
	frame_state.color_target = state->backbuffer;
	frame_state.active = true;
	frame_state.presented = false;
	state->active_frame = frame;
	tex->owner_frame = frame;
	tex->renderable = true;
	out.frame = frame;
	out.swapchain = swapchain;
	out.color_target = state->backbuffer;
	out.size = state->desc.size;
	out.color_format = state->desc.color_format;
	AppendLog("BeginFrame id=" + frame.Dump() + " swapchain=" + swapchain.Dump() + " target=" + state->backbuffer.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::Present(GpuFrameId frame)
{
	FrameState *state = FindFrameState(frame);
	if(!frame.IsValid() || !state) {
		Fail("Present frame=" + frame.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	if(state->presented) {
		Fail("Present frame=" + frame.Dump() + " reason=frame_already_presented");
		return GpuResult::InvalidState;
	}
	if(!state->active) {
		Fail("Present frame=" + frame.Dump() + " reason=frame_not_active");
		return GpuResult::InvalidState;
	}
	SwapchainState *swapchain = FindSwapchainState(state->swapchain);
	if(!swapchain || swapchain->active_frame.value != frame.value) {
		Fail("Present frame=" + frame.Dump() + " reason=frame_not_active");
		return GpuResult::InvalidState;
	}
	TextureState *tex = FindTextureState(state->color_target);
	if(tex)
		tex->renderable = false;
	state->active = false;
	state->presented = true;
	swapchain->active_frame = GpuFrameId();
	AppendLog("Present frame=" + frame.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::CreateShader(const GpuShaderDesc& desc, GpuShaderId& out)
{
	out = GpuShaderId();
	if(!IsValidShaderStage(desc.stage)) {
		Fail("CreateShader stage=" + DumpGpuShaderStage(desc.stage) + " reason=invalid_stage");
		return GpuResult::InvalidArgument;
	}
	if(!IsValidShaderFormat(desc.format)) {
		Fail("CreateShader format=" + DumpGpuShaderFormat(desc.format) + " reason=invalid_format");
		return GpuResult::InvalidArgument;
	}
	if(desc.entry_point.IsEmpty()) {
		Fail("CreateShader stage=" + DumpGpuShaderStage(desc.stage) + " reason=empty_entry_point");
		return GpuResult::InvalidArgument;
	}
	if(!HasSpirVMagic(desc.code)) {
		Fail("CreateShader stage=" + DumpGpuShaderStage(desc.stage) + " format=" + DumpGpuShaderFormat(desc.format) + " reason=invalid_spirv");
		return GpuResult::InvalidArgument;
	}
	GpuShaderId id;
	id.value = next_shader_id++;
	ShaderState& state = shaders.Add(id.value, ShaderState());
	state.desc = desc;
	state.alive = true;
	out = id;
	AppendLog("CreateShader id=" + id.Dump() + " stage=" + DumpGpuShaderStage(desc.stage) +
	          " format=" + DumpGpuShaderFormat(desc.format) + " bytes=" + AsString(desc.code.GetCount()) +
	          " entry=" + desc.entry_point);
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroyShader(GpuShaderId id)
{
	int index = shaders.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroyShader id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	shaders.Remove(index);
	AppendLog("DestroyShader id=" + id.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out)
{
	out = GpuPipelineId();
	if(desc.color_format == GpuFormat::Unknown) {
		Fail("CreatePipeline format=Unknown reason=invalid_format");
		return GpuResult::InvalidArgument;
	}
	if(!IsValidTopology(desc.topology)) {
		Fail("CreatePipeline topology=" + DumpGpuPrimitiveTopology(desc.topology) + " reason=invalid_topology");
		return GpuResult::InvalidArgument;
	}
	if(!IsValidVertexLayout(desc.vertex_layout)) {
		Fail("CreatePipeline layout=" + DumpGpuVertexLayout(desc.vertex_layout) + " reason=invalid_vertex_layout");
		return GpuResult::InvalidArgument;
	}
	if(desc.sampled_texture_count < 0 || desc.sampled_texture_count > 1 ||
	   !IsValidSamplerFilter(desc.sampler_filter) || !IsValidSamplerAddressMode(desc.sampler_address)) {
		Fail("CreatePipeline sampled_textures=" + AsString(desc.sampled_texture_count) + " reason=invalid_sampled_texture_contract");
		return GpuResult::InvalidArgument;
	}
	if((desc.sampled_texture_count == 0 && desc.vertex_layout != GpuVertexLayout::Position2Color4F) ||
	   (desc.sampled_texture_count == 1 && desc.vertex_layout != GpuVertexLayout::Position2Uv2Color4F)) {
		Fail("CreatePipeline layout=" + DumpGpuVertexLayout(desc.vertex_layout) + " sampled_textures=" +
		     AsString(desc.sampled_texture_count) + " reason=layout_sample_mismatch");
		return GpuResult::InvalidArgument;
	}
	if(!CheckShaderExists(desc.vertex_shader) || !CheckShaderExists(desc.fragment_shader)) {
		Fail("CreatePipeline vertex=" + desc.vertex_shader.Dump() + " fragment=" + desc.fragment_shader.Dump() + " reason=unknown_shader");
		return GpuResult::InvalidHandle;
	}
	const ShaderState *vertex = FindShaderState(desc.vertex_shader);
	const ShaderState *fragment = FindShaderState(desc.fragment_shader);
	if(!vertex || vertex->desc.stage != GpuShaderStage::Vertex || !fragment || fragment->desc.stage != GpuShaderStage::Fragment) {
		Fail("CreatePipeline vertex=" + desc.vertex_shader.Dump() + " fragment=" + desc.fragment_shader.Dump() + " reason=shader_stage_mismatch");
		return GpuResult::InvalidArgument;
	}
	GpuPipelineId id;
	id.value = next_pipeline_id++;
	PipelineState& state = pipelines.Add(id.value, PipelineState());
	state.desc = desc;
	state.alive = true;
	out = id;
	AppendLog("CreatePipeline id=" + id.Dump() + " topology=" + DumpGpuPrimitiveTopology(desc.topology) +
	          " format=" + DumpGpuFormat(desc.color_format) + " vertex=" + desc.vertex_shader.Dump() +
	          " fragment=" + desc.fragment_shader.Dump() + " layout=" + DumpGpuVertexLayout(desc.vertex_layout) +
	          " sampled_textures=" + AsString(desc.sampled_texture_count) + " filter=" + DumpGpuSamplerFilter(desc.sampler_filter) +
	          " address=" + DumpGpuSamplerAddressMode(desc.sampler_address));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::DestroyPipeline(GpuPipelineId id)
{
	int index = pipelines.Find(id.value);
	if(!id.IsValid() || index < 0) {
		Fail("DestroyPipeline id=" + id.Dump() + " reason=unknown");
		return GpuResult::InvalidHandle;
	}
	pipelines.Remove(index);
	AppendLog("DestroyPipeline id=" + id.Dump());
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
	state.ended = false;
	state.submitted = false;
	state.render_pass_active = false;
	state.pipeline = GpuPipelineId();
	state.vertex_buffer = GpuBufferId();
	state.sampled_textures.Clear();
	state.referenced_sampled_textures.Clear();
	state.draw_count = 0;
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
	if(!IsValidLoadOp(desc.color_load) || !IsValidStoreOp(desc.color_store)) {
		Fail("BeginRenderPass list=" + list.Dump() + " load=" + DumpGpuLoadOp(desc.color_load) +
		     " store=" + DumpGpuStoreOp(desc.color_store) + " reason=invalid_load_store");
		return GpuResult::InvalidArgument;
	}
	if(desc.color_load == GpuLoadOp::Clear && !IsFiniteClearColor(desc.clear_color)) {
		Fail("BeginRenderPass list=" + list.Dump() + " reason=non_finite_clear_color");
		return GpuResult::InvalidArgument;
	}
	const TextureState *texture = FindTextureState(desc.color_target);
	if(texture && texture->swapchain_backbuffer) {
		SwapchainState *swapchain = FindSwapchainState(texture->owner_swapchain);
		const FrameState *frame = FindFrameState(texture->owner_frame);
		if(!texture->renderable || !swapchain || !frame || !frame->active || frame->presented || swapchain->active_frame.value != texture->owner_frame.value) {
			Fail("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " reason=backbuffer_not_active");
			return GpuResult::InvalidState;
		}
	}
	if(!texture || (texture->desc.usage & GpuTextureUsage_ColorAttachment) == 0) {
		Fail("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " reason=not_color_attachment");
		return GpuResult::InvalidArgument;
	}
	if(desc.color_format == GpuFormat::Unknown || desc.color_format != texture->desc.format) {
		Fail("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " format=" + DumpGpuFormat(desc.color_format) + " reason=format_mismatch");
		return GpuResult::InvalidArgument;
	}
	mutable_state.render_pass_active = true;
	mutable_state.pass_desc = desc;
	mutable_state.pipeline = GpuPipelineId();
	mutable_state.vertex_buffer = GpuBufferId();
	mutable_state.sampled_textures.Clear();
	mutable_state.draw_count = 0;
	AppendLog("BeginRenderPass list=" + list.Dump() + " target=" + desc.color_target.Dump() + " color_format=" + DumpGpuFormat(desc.color_format) +
	          " load=" + DumpGpuLoadOp(desc.color_load) + " store=" + DumpGpuStoreOp(desc.color_store));
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::SetPipeline(GpuCommandListId list, GpuPipelineId pipeline)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("SetPipeline list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(!mutable_state.render_pass_active) {
		Fail("SetPipeline list=" + list.Dump() + " reason=render_pass_required");
		return GpuResult::InvalidState;
	}
	if(!CheckPipelineExists(pipeline)) {
		Fail("SetPipeline list=" + list.Dump() + " pipeline=" + pipeline.Dump() + " reason=unknown_pipeline");
		return GpuResult::InvalidHandle;
	}
	const PipelineState& pipeline_state = pipelines[pipelines.Find(pipeline.value)];
	if(pipeline_state.desc.color_format != mutable_state.pass_desc.color_format) {
		Fail("SetPipeline list=" + list.Dump() + " pipeline=" + pipeline.Dump() + " reason=pipeline_format_mismatch");
		return GpuResult::InvalidArgument;
	}
	mutable_state.pipeline = pipeline;
	mutable_state.sampled_textures.Clear();
	if(pipeline_state.desc.sampled_texture_count > 0)
		mutable_state.sampled_textures.SetCount(pipeline_state.desc.sampled_texture_count);
	AppendLog("SetPipeline list=" + list.Dump() + " pipeline=" + pipeline.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("SetVertexBuffer list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(!mutable_state.render_pass_active) {
		Fail("SetVertexBuffer list=" + list.Dump() + " reason=render_pass_required");
		return GpuResult::InvalidState;
	}
	if(!CheckBufferExists(buffer)) {
		Fail("SetVertexBuffer list=" + list.Dump() + " buffer=" + buffer.Dump() + " reason=unknown_buffer");
		return GpuResult::InvalidHandle;
	}
	const BufferState& buffer_state = buffers[buffers.Find(buffer.value)];
	if((buffer_state.desc.usage & GpuBufferUsage_Vertex) == 0) {
		Fail("SetVertexBuffer list=" + list.Dump() + " buffer=" + buffer.Dump() + " reason=missing_vertex_usage");
		return GpuResult::InvalidArgument;
	}
	mutable_state.vertex_buffer = buffer;
	AppendLog("SetVertexBuffer list=" + list.Dump() + " buffer=" + buffer.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::SetSampledTexture(GpuCommandListId list, int slot, GpuTextureId texture)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("SetSampledTexture list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(!mutable_state.render_pass_active || !mutable_state.pipeline.IsValid()) {
		Fail("SetSampledTexture list=" + list.Dump() + " reason=pipeline_and_render_pass_required");
		return GpuResult::InvalidState;
	}
	int pipeline_index = pipelines.Find(mutable_state.pipeline.value);
	if(pipeline_index < 0) {
		Fail("SetSampledTexture list=" + list.Dump() + " reason=unknown_pipeline");
		return GpuResult::InvalidHandle;
	}
	const GpuPipelineDesc& pipeline = pipelines[pipeline_index].desc;
	if(slot < 0 || slot >= pipeline.sampled_texture_count) {
		Fail("SetSampledTexture list=" + list.Dump() + " slot=" + AsString(slot) + " reason=slot_out_of_range");
		return GpuResult::InvalidArgument;
	}
	int texture_index = textures.Find(texture.value);
	if(!texture.IsValid() || texture_index < 0) {
		Fail("SetSampledTexture list=" + list.Dump() + " texture=" + texture.Dump() + " reason=unknown_texture");
		return GpuResult::InvalidHandle;
	}
	const TextureState& texture_state = textures[texture_index];
	if(texture_state.swapchain_backbuffer || (texture_state.desc.usage & GpuTextureUsage_Sampled) == 0) {
		Fail("SetSampledTexture list=" + list.Dump() + " texture=" + texture.Dump() + " reason=not_sampled_texture");
		return GpuResult::InvalidArgument;
	}
	if(mutable_state.pass_desc.color_target == texture) {
		Fail("SetSampledTexture list=" + list.Dump() + " texture=" + texture.Dump() + " reason=color_target_alias");
		return GpuResult::InvalidState;
	}
	mutable_state.sampled_textures[slot] = texture;
	bool found = false;
	for(const GpuTextureId& referenced : mutable_state.referenced_sampled_textures)
		if(referenced == texture) {
			found = true;
			break;
		}
	if(!found)
		mutable_state.referenced_sampled_textures.Add(texture);
	AppendLog("SetSampledTexture list=" + list.Dump() + " slot=" + AsString(slot) + " texture=" + texture.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::Draw(GpuCommandListId list, int vertex_count, int first_vertex)
{
	const CommandState *state = nullptr;
	String reason;
	if(!CanUseCommandList(list, state, reason)) {
		Fail("Draw list=" + list.Dump() + " reason=" + reason);
		return GpuResult::InvalidState;
	}
	CommandState& mutable_state = *FindCommandState(list);
	if(!mutable_state.render_pass_active) {
		Fail("Draw list=" + list.Dump() + " reason=render_pass_required");
		return GpuResult::InvalidState;
	}
	if(!mutable_state.pipeline.IsValid()) {
		Fail("Draw list=" + list.Dump() + " reason=pipeline_required");
		return GpuResult::InvalidState;
	}
	int pipeline_index = pipelines.Find(mutable_state.pipeline.value);
	if(pipeline_index < 0) {
		Fail("Draw list=" + list.Dump() + " pipeline=" + mutable_state.pipeline.Dump() + " reason=unknown_pipeline");
		return GpuResult::InvalidHandle;
	}
	if(vertex_count <= 0 || first_vertex < 0) {
		Fail("Draw list=" + list.Dump() + " vertices=" + AsString(vertex_count) + " first=" + AsString(first_vertex) + " reason=invalid_vertex_range");
		return GpuResult::InvalidArgument;
	}
	if(!mutable_state.vertex_buffer.IsValid()) {
		Fail("Draw list=" + list.Dump() + " reason=vertex_buffer_required");
		return GpuResult::InvalidState;
	}
	if(!CheckBufferExists(mutable_state.vertex_buffer)) {
		Fail("Draw list=" + list.Dump() + " buffer=" + mutable_state.vertex_buffer.Dump() + " reason=unknown_vertex_buffer");
		return GpuResult::InvalidHandle;
	}
	const int required_samples = pipelines[pipeline_index].desc.sampled_texture_count;
	if(mutable_state.sampled_textures.GetCount() != required_samples) {
		Fail("Draw list=" + list.Dump() + " reason=sample_binding_count_mismatch");
		return GpuResult::InvalidState;
	}
	for(int i = 0; i < required_samples; ++i) {
		const GpuTextureId sampled = mutable_state.sampled_textures[i];
		const TextureState *texture = FindTextureState(sampled);
		if(!sampled.IsValid() || !texture || (texture->desc.usage & GpuTextureUsage_Sampled) == 0) {
			Fail("Draw list=" + list.Dump() + " slot=" + AsString(i) + " reason=sampled_texture_required");
			return GpuResult::InvalidState;
		}
	}
	mutable_state.draw_count += 1;
	AppendLog("Draw list=" + list.Dump() + " vertices=" + AsString(vertex_count) + " first=" + AsString(first_vertex));
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
	mutable_state.pipeline = GpuPipelineId();
	mutable_state.vertex_buffer = GpuBufferId();
	mutable_state.sampled_textures.Clear();
	AppendLog("EndRenderPass list=" + list.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::EndCommands(GpuCommandListId list)
{
	if(!list.IsValid()) {
		Fail("EndCommands list=" + list.Dump() + " reason=invalid command list");
		return GpuResult::InvalidState;
	}
	CommandState *state = FindCommandState(list);
	if(!state) {
		Fail("EndCommands list=" + list.Dump() + " reason=unknown command list");
		return GpuResult::InvalidState;
	}
	if(state->submitted) {
		Fail("EndCommands list=" + list.Dump() + " reason=command_list_already_submitted");
		return GpuResult::InvalidState;
	}
	if(state->ended) {
		Fail("EndCommands list=" + list.Dump() + " reason=command_list_already_ended");
		return GpuResult::InvalidState;
	}
	if(state->render_pass_active) {
		Fail("EndCommands list=" + list.Dump() + " reason=render_pass_still_active");
		return GpuResult::InvalidState;
	}
	if(!state->begun) {
		Fail("EndCommands list=" + list.Dump() + " reason=command_list_not_begun");
		return GpuResult::InvalidState;
	}
	state->ended = true;
	active_command_list = GpuCommandListId();
	AppendLog("EndCommands id=" + list.Dump());
	return GpuResult::Ok;
}

GpuResult NullGpuDevice::Submit(GpuCommandListId list)
{
	if(!list.IsValid()) {
		Fail("Submit list=" + list.Dump() + " reason=invalid command list");
		return GpuResult::InvalidState;
	}
	CommandState *state = FindCommandState(list);
	if(!state) {
		Fail("Submit list=" + list.Dump() + " reason=unknown command list");
		return GpuResult::InvalidState;
	}
	if(state->submitted) {
		Fail("Submit list=" + list.Dump() + " reason=command_list_already_submitted");
		return GpuResult::InvalidState;
	}
	if(!state->ended) {
		Fail("Submit list=" + list.Dump() + " reason=command_list_not_ended");
		return GpuResult::InvalidState;
	}
	state->submitted = true;
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
