#include "RenderRhi.h"

namespace Upp {

static String DumpId(const char *name, int value)
{
	return String(name) + "#" + AsString(value);
}

String GpuAdapterId::Dump() const { return DumpId("Adapter", value); }
String GpuDeviceId::Dump() const { return DumpId("Device", value); }
String GpuBufferId::Dump() const { return DumpId("Buffer", value); }
String GpuTextureId::Dump() const { return DumpId("Texture", value); }
String GpuShaderId::Dump() const { return DumpId("Shader", value); }
String GpuPipelineId::Dump() const { return DumpId("Pipeline", value); }
String GpuCommandListId::Dump() const { return DumpId("Cmd", value); }
String GpuSurfaceId::Dump() const { return DumpId("Surface", value); }
String GpuSwapchainId::Dump() const { return DumpId("Swapchain", value); }
String GpuFrameId::Dump() const { return DumpId("Frame", value); }

bool GpuNativeWindowDesc::IsValid() const
{
	switch(kind) {
	case GpuNativeWindowKind::None:
		return handle == 0;
	case GpuNativeWindowKind::Win32:
		return handle != 0;
	}
	return false;
}

String DumpGpuBackendKind(GpuBackendKind kind)
{
	switch(kind) {
	case GpuBackendKind::Unknown: return "Unknown";
	case GpuBackendKind::Null: return "Null";
	case GpuBackendKind::Vulkan: return "Vulkan";
	case GpuBackendKind::Metal: return "Metal";
	case GpuBackendKind::WebGPU: return "WebGPU";
	case GpuBackendKind::OpenGL: return "OpenGL";
	}
	return "Unknown";
}

String DumpGpuNativeWindowKind(GpuNativeWindowKind kind)
{
	switch(kind) {
	case GpuNativeWindowKind::None: return "None";
	case GpuNativeWindowKind::Win32: return "Win32";
	}
	return String("Unknown(") + AsString((int)kind) + ")";
}

String DumpGpuNativeWindowDesc(const GpuNativeWindowDesc& desc)
{
	String out;
	out << DumpGpuNativeWindowKind(desc.kind);
	out << " handle=" << (desc.handle ? "set" : "unset");
	return out;
}

String DumpGpuFormat(GpuFormat format)
{
	switch(format) {
	case GpuFormat::Unknown: return "Unknown";
	case GpuFormat::RGBA8: return "RGBA8";
	case GpuFormat::BGRA8: return "BGRA8";
	case GpuFormat::RGBA8Srgb: return "RGBA8Srgb";
	case GpuFormat::BGRA8Srgb: return "BGRA8Srgb";
	case GpuFormat::R16F: return "R16F";
	case GpuFormat::D24S8: return "D24S8";
	}
	return "Unknown";
}

static String DumpFlags(int flags, const char *const *names, const int *bits, int count)
{
	if(flags == 0)
		return "None";
	String out;
	for(int i = 0; i < count; ++i) {
		if(flags & bits[i]) {
			if(!out.IsEmpty())
				out << '|';
			out << names[i];
		}
	}
	if(out.IsEmpty())
		out = AsString(flags);
	return out;
}

String DumpGpuBufferUsage(int usage)
{
	static const char *const names[] = { "Vertex", "Index", "Uniform", "Storage", "TransferSrc", "TransferDst" };
	static const int bits[] = { GpuBufferUsage_Vertex, GpuBufferUsage_Index, GpuBufferUsage_Uniform,
		GpuBufferUsage_Storage, GpuBufferUsage_TransferSrc, GpuBufferUsage_TransferDst };
	return DumpFlags(usage, names, bits, 6);
}

String DumpGpuTextureUsage(int usage)
{
	static const char *const names[] = { "ColorAttachment", "Sampled", "TransferSrc", "TransferDst" };
	static const int bits[] = { GpuTextureUsage_ColorAttachment, GpuTextureUsage_Sampled,
		GpuTextureUsage_TransferSrc, GpuTextureUsage_TransferDst };
	return DumpFlags(usage, names, bits, 4);
}

String DumpGpuLoadOp(GpuLoadOp op)
{
	switch(op) {
	case GpuLoadOp::Load: return "Load";
	case GpuLoadOp::Clear: return "Clear";
	case GpuLoadOp::DontCare: return "DontCare";
	}
	return String("Unknown(") + AsString((int)op) + ")";
}

String DumpGpuStoreOp(GpuStoreOp op)
{
	switch(op) {
	case GpuStoreOp::Store: return "Store";
	case GpuStoreOp::DontCare: return "DontCare";
	}
	return String("Unknown(") + AsString((int)op) + ")";
}

String DumpGpuPrimitiveTopology(GpuPrimitiveTopology topology)
{
	switch(topology) {
	case GpuPrimitiveTopology::TriangleList: return "TriangleList";
	case GpuPrimitiveTopology::LineList: return "LineList";
	}
	return String("Unknown(") + AsString((int)topology) + ")";
}

String DumpGpuShaderStage(GpuShaderStage stage)
{
	switch(stage) {
	case GpuShaderStage::Unknown: return "Unknown";
	case GpuShaderStage::Vertex: return "Vertex";
	case GpuShaderStage::Fragment: return "Fragment";
	}
	return String("Unknown(") + AsString((int)stage) + ")";
}

String DumpGpuShaderFormat(GpuShaderFormat format)
{
	switch(format) {
	case GpuShaderFormat::Unknown: return "Unknown";
	case GpuShaderFormat::SpirV: return "SpirV";
	}
	return String("Unknown(") + AsString((int)format) + ")";
}

String DumpGpuVertexLayout(GpuVertexLayout layout)
{
	switch(layout) {
	case GpuVertexLayout::Unknown: return "Unknown";
	case GpuVertexLayout::Position2Color4F: return "Position2Color4F";
	case GpuVertexLayout::Position2Uv2Color4F: return "Position2Uv2Color4F";
	}
	return String("Unknown(") + AsString((int)layout) + ")";
}

String DumpGpuSamplerFilter(GpuSamplerFilter filter)
{
	switch(filter) {
	case GpuSamplerFilter::Nearest: return "Nearest";
	case GpuSamplerFilter::Linear: return "Linear";
	}
	return String("Unknown(") + AsString((int)filter) + ")";
}

String DumpGpuSamplerAddressMode(GpuSamplerAddressMode mode)
{
	switch(mode) {
	case GpuSamplerAddressMode::ClampToEdge: return "ClampToEdge";
	case GpuSamplerAddressMode::Repeat: return "Repeat";
	}
	return String("Unknown(") + AsString((int)mode) + ")";
}

String DumpGpuCapabilityFlags(int flags)
{
	static const char *const names[] = { "Buffers", "Textures", "RenderPass", "Pipelines", "Shaders" };
	static const int bits[] = { GpuCapability_Buffers, GpuCapability_Textures, GpuCapability_RenderPass,
		GpuCapability_Pipelines, GpuCapability_Shaders };
	return DumpFlags(flags, names, bits, 5);
}

String DumpGpuResult(GpuResult result)
{
	switch(result) {
	case GpuResult::Ok: return "Ok";
	case GpuResult::InvalidArgument: return "InvalidArgument";
	case GpuResult::InvalidHandle: return "InvalidHandle";
	case GpuResult::InvalidState: return "InvalidState";
	case GpuResult::NotFound: return "NotFound";
	case GpuResult::Unsupported: return "Unsupported";
	}
	return "Ok";
}

String DumpGpuError(GpuError error)
{
	switch(error) {
	case GpuError::None: return "None";
	case GpuError::InvalidHandle: return "InvalidHandle";
	case GpuError::InvalidState: return "InvalidState";
	case GpuError::InvalidArgument: return "InvalidArgument";
	case GpuError::NotFound: return "NotFound";
	case GpuError::Unsupported: return "Unsupported";
	}
	return "None";
}

}
