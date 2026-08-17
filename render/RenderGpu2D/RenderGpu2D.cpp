#include "RenderGpu2D.h"

#include <cmath>
#include <limits>

namespace Upp {

namespace {

static const uint32 kVertexShader[] = {
	0x07230203u, 0x00010000u, 0x00000000u, 0x00000016u, 0x00000000u, 0x00020011u,
	0x00000001u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0009000fu, 0x00000000u,
	0x0000000fu, 0x6e69616du, 0x00000000u, 0x00000007u, 0x00000009u, 0x0000000bu,
	0x0000000cu, 0x00040047u, 0x00000007u, 0x0000001eu, 0x00000000u, 0x00040047u,
	0x00000009u, 0x0000001eu, 0x00000001u, 0x00040047u, 0x0000000bu, 0x0000000bu,
	0x00000000u, 0x00040047u, 0x0000000cu, 0x0000001eu, 0x00000000u, 0x00020013u,
	0x00000001u, 0x00030021u, 0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u,
	0x00000020u, 0x00040017u, 0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u,
	0x00000005u, 0x00000003u, 0x00000004u, 0x00040020u, 0x00000006u, 0x00000001u,
	0x00000004u, 0x0004003bu, 0x00000006u, 0x00000007u, 0x00000001u, 0x00040020u,
	0x00000008u, 0x00000001u, 0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u,
	0x00000001u, 0x00040020u, 0x0000000au, 0x00000003u, 0x00000005u, 0x0004003bu,
	0x0000000au, 0x0000000bu, 0x00000003u, 0x0004003bu, 0x0000000au, 0x0000000cu,
	0x00000003u, 0x0004002bu, 0x00000003u, 0x0000000du, 0x00000000u, 0x0004002bu,
	0x00000003u, 0x0000000eu, 0x3f800000u, 0x00050036u, 0x00000001u, 0x0000000fu,
	0x00000000u, 0x00000002u, 0x000200f8u, 0x00000010u, 0x0004003du, 0x00000004u,
	0x00000011u, 0x00000007u, 0x00050051u, 0x00000003u, 0x00000012u, 0x00000011u,
	0x00000000u, 0x00050051u, 0x00000003u, 0x00000013u, 0x00000011u, 0x00000001u,
	0x00070050u, 0x00000005u, 0x00000014u, 0x00000012u, 0x00000013u, 0x0000000du,
	0x0000000eu, 0x0003003eu, 0x0000000bu, 0x00000014u, 0x0004003du, 0x00000005u,
	0x00000015u, 0x00000009u, 0x0003003eu, 0x0000000cu, 0x00000015u, 0x000100fdu,
	0x00010038u,
};

static const uint32 kFragmentShader[] = {
	0x07230203u, 0x00010000u, 0x00000000u, 0x0000000cu, 0x00000000u, 0x00020011u,
	0x00000001u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0007000fu, 0x00000004u,
	0x00000009u, 0x6e69616du, 0x00000000u, 0x00000006u, 0x00000008u, 0x00030010u,
	0x00000009u, 0x00000007u, 0x00040047u, 0x00000006u, 0x0000001eu, 0x00000000u,
	0x00040047u, 0x00000008u, 0x0000001eu, 0x00000000u, 0x00020013u, 0x00000001u,
	0x00030021u, 0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u,
	0x00040017u, 0x00000004u, 0x00000003u, 0x00000004u, 0x00040020u, 0x00000005u,
	0x00000001u, 0x00000004u, 0x0004003bu, 0x00000005u, 0x00000006u, 0x00000001u,
	0x00040020u, 0x00000007u, 0x00000003u, 0x00000004u, 0x0004003bu, 0x00000007u,
	0x00000008u, 0x00000003u, 0x00050036u, 0x00000001u, 0x00000009u, 0x00000000u,
	0x00000002u, 0x000200f8u, 0x0000000au, 0x0004003du, 0x00000004u, 0x0000000bu,
	0x00000006u, 0x0003003eu, 0x00000008u, 0x0000000bu, 0x000100fdu, 0x00010038u,
};

// Stage-5 sampled-image shaders. Position, UV and colour stay backend-neutral;
// descriptor-set details remain inside the Vulkan backend.
static const uint32 kTexturedVertexShader[] = {
	0x07230203u, 0x00010000u, 0x00000000u, 0x0000001au, 0x00000000u, 0x00020011u,
	0x00000001u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x000b000fu, 0x00000000u,
	0x00000012u, 0x6e69616du, 0x00000000u, 0x00000007u, 0x0000000du, 0x00000009u,
	0x0000000bu, 0x0000000fu, 0x0000000cu, 0x00040047u, 0x00000007u, 0x0000001eu,
	0x00000000u, 0x00040047u, 0x0000000du, 0x0000001eu, 0x00000001u, 0x00040047u,
	0x00000009u, 0x0000001eu, 0x00000002u, 0x00040047u, 0x0000000bu, 0x0000000bu,
	0x00000000u, 0x00040047u, 0x0000000fu, 0x0000001eu, 0x00000000u, 0x00040047u,
	0x0000000cu, 0x0000001eu, 0x00000001u, 0x00020013u, 0x00000001u, 0x00030021u,
	0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u, 0x00040017u,
	0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u, 0x00000005u, 0x00000003u,
	0x00000004u, 0x00040020u, 0x00000006u, 0x00000001u, 0x00000004u, 0x0004003bu,
	0x00000006u, 0x00000007u, 0x00000001u, 0x00040020u, 0x00000008u, 0x00000001u,
	0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x00040020u,
	0x0000000au, 0x00000003u, 0x00000005u, 0x0004003bu, 0x0000000au, 0x0000000bu,
	0x00000003u, 0x0004003bu, 0x0000000au, 0x0000000cu, 0x00000003u, 0x0004003bu,
	0x00000006u, 0x0000000du, 0x00000001u, 0x00040020u, 0x0000000eu, 0x00000003u,
	0x00000004u, 0x0004003bu, 0x0000000eu, 0x0000000fu, 0x00000003u, 0x0004002bu,
	0x00000003u, 0x00000010u, 0x00000000u, 0x0004002bu, 0x00000003u, 0x00000011u,
	0x3f800000u, 0x00050036u, 0x00000001u, 0x00000012u, 0x00000000u, 0x00000002u,
	0x000200f8u, 0x00000013u, 0x0004003du, 0x00000004u, 0x00000014u, 0x00000007u,
	0x00050051u, 0x00000003u, 0x00000015u, 0x00000014u, 0x00000000u, 0x00050051u,
	0x00000003u, 0x00000016u, 0x00000014u, 0x00000001u, 0x00070050u, 0x00000005u,
	0x00000017u, 0x00000015u, 0x00000016u, 0x00000010u, 0x00000011u, 0x0003003eu,
	0x0000000bu, 0x00000017u, 0x0004003du, 0x00000004u, 0x00000018u, 0x0000000du,
	0x0003003eu, 0x0000000fu, 0x00000018u, 0x0004003du, 0x00000005u, 0x00000019u,
	0x00000009u, 0x0003003eu, 0x0000000cu, 0x00000019u, 0x000100fdu, 0x00010038u,
};

static const uint32 kTexturedFragmentShader[] = {
	0x07230203u, 0x00010000u, 0x00000000u, 0x00000017u, 0x00000000u, 0x00020011u,
	0x00000001u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0008000fu, 0x00000004u,
	0x00000010u, 0x6e69616du, 0x00000000u, 0x00000007u, 0x00000009u, 0x0000000bu,
	0x00030010u, 0x00000010u, 0x00000007u, 0x00040047u, 0x00000007u, 0x0000001eu,
	0x00000000u, 0x00040047u, 0x00000009u, 0x0000001eu, 0x00000001u, 0x00040047u,
	0x0000000bu, 0x0000001eu, 0x00000000u, 0x00040047u, 0x0000000fu, 0x00000022u,
	0x00000000u, 0x00040047u, 0x0000000fu, 0x00000021u, 0x00000000u, 0x00020013u,
	0x00000001u, 0x00030021u, 0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u,
	0x00000020u, 0x00040017u, 0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u,
	0x00000005u, 0x00000003u, 0x00000004u, 0x00040020u, 0x00000006u, 0x00000001u,
	0x00000004u, 0x0004003bu, 0x00000006u, 0x00000007u, 0x00000001u, 0x00040020u,
	0x00000008u, 0x00000001u, 0x00000005u, 0x0004003bu, 0x00000008u, 0x00000009u,
	0x00000001u, 0x00040020u, 0x0000000au, 0x00000003u, 0x00000005u, 0x0004003bu,
	0x0000000au, 0x0000000bu, 0x00000003u, 0x00090019u, 0x0000000cu, 0x00000003u,
	0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u,
	0x0003001bu, 0x0000000du, 0x0000000cu, 0x00040020u, 0x0000000eu, 0x00000000u,
	0x0000000du, 0x0004003bu, 0x0000000eu, 0x0000000fu, 0x00000000u, 0x00050036u,
	0x00000001u, 0x00000010u, 0x00000000u, 0x00000002u, 0x000200f8u, 0x00000011u,
	0x0004003du, 0x00000004u, 0x00000012u, 0x00000007u, 0x0004003du, 0x0000000du,
	0x00000013u, 0x0000000fu, 0x00050057u, 0x00000005u, 0x00000014u, 0x00000013u,
	0x00000012u, 0x0004003du, 0x00000005u, 0x00000015u, 0x00000009u, 0x00050085u,
	0x00000005u, 0x00000016u, 0x00000014u, 0x00000015u, 0x0003003eu, 0x0000000bu,
	0x00000016u, 0x000100fdu, 0x00010038u,
};

static String SpirV(const uint32 *words, int count)
{
	return String(reinterpret_cast<const char *>(words), count * (int)sizeof(uint32));
}

static bool IsFinite(double v)
{
	return std::isfinite(v);
}

static bool IsFinitePoint(const Pointf& p)
{
	return IsFinite(p.x) && IsFinite(p.y);
}

static bool IsFiniteRect(const Rectf& r)
{
	return IsFinite(r.left) && IsFinite(r.top) && IsFinite(r.right) && IsFinite(r.bottom);
}

static bool IsFiniteTransform(const Transform2D& m)
{
	return IsFinitePoint(m.x) && IsFinitePoint(m.y) && IsFinitePoint(m.t);
}

static Rectf IntersectRectf(const Rectf& a, const Rectf& b)
{
	return Rectf(max(a.left, b.left), max(a.top, b.top), min(a.right, b.right), min(a.bottom, b.bottom));
}

static bool IsEmptyRectf(const Rectf& r)
{
	return r.right <= r.left || r.bottom <= r.top;
}

static Vector<Pointf> TransformRect(const Rectf& rect, const Transform2D& transform)
{
	Vector<Pointf> out;
	out.Add(transform.TransformPoint(Pointf(rect.left, rect.top)));
	out.Add(transform.TransformPoint(Pointf(rect.right, rect.top)));
	out.Add(transform.TransformPoint(Pointf(rect.right, rect.bottom)));
	out.Add(transform.TransformPoint(Pointf(rect.left, rect.bottom)));
	return out;
}

enum ClipEdge {
	ClipLeft,
	ClipRight,
	ClipTop,
	ClipBottom,
};

static bool Inside(const Pointf& p, const Rectf& clip, ClipEdge edge)
{
	switch(edge) {
	case ClipLeft: return p.x >= clip.left;
	case ClipRight: return p.x <= clip.right;
	case ClipTop: return p.y >= clip.top;
	case ClipBottom: return p.y <= clip.bottom;
	}
	return false;
}

static Pointf Intersect(const Pointf& a, const Pointf& b, const Rectf& clip, ClipEdge edge)
{
	if(edge == ClipLeft || edge == ClipRight) {
		const double x = edge == ClipLeft ? clip.left : clip.right;
		const double dx = b.x - a.x;
		const double t = dx == 0.0 ? 0.0 : (x - a.x) / dx;
		return Pointf(x, a.y + (b.y - a.y) * t);
	}
	const double y = edge == ClipTop ? clip.top : clip.bottom;
	const double dy = b.y - a.y;
	const double t = dy == 0.0 ? 0.0 : (y - a.y) / dy;
	return Pointf(a.x + (b.x - a.x) * t, y);
}

static Vector<Pointf> ClipPolygon(const Vector<Pointf>& source, const Rectf& clip)
{
	Vector<Pointf> input;
	input.Reserve(source.GetCount());
	for(const Pointf& p : source)
		input.Add(p);
	for(int e = ClipLeft; e <= ClipBottom && !input.IsEmpty(); ++e) {
		Vector<Pointf> output;
		Pointf previous = input.Top();
		bool previous_inside = Inside(previous, clip, (ClipEdge)e);
		for(const Pointf& current : input) {
			const bool current_inside = Inside(current, clip, (ClipEdge)e);
			if(current_inside != previous_inside)
				output.Add(Intersect(previous, current, clip, (ClipEdge)e));
			if(current_inside)
				output.Add(current);
			previous = current;
			previous_inside = current_inside;
		}
		input = pick(output);
	}
	return input;
}

struct TexturedPoint : Moveable<TexturedPoint> {
	Pointf position;
	Pointf uv;
};

static TexturedPoint IntersectTextured(const TexturedPoint& a, const TexturedPoint& b,
                                       const Rectf& clip, ClipEdge edge)
{
	double t = 0.0;
	Pointf position;
	if(edge == ClipLeft || edge == ClipRight) {
		const double x = edge == ClipLeft ? clip.left : clip.right;
		const double dx = b.position.x - a.position.x;
		t = dx == 0.0 ? 0.0 : (x - a.position.x) / dx;
		position = Pointf(x, a.position.y + (b.position.y - a.position.y) * t);
	}
	else {
		const double y = edge == ClipTop ? clip.top : clip.bottom;
		const double dy = b.position.y - a.position.y;
		t = dy == 0.0 ? 0.0 : (y - a.position.y) / dy;
		position = Pointf(a.position.x + (b.position.x - a.position.x) * t, y);
	}
	TexturedPoint out;
	out.position = position;
	out.uv = Pointf(a.uv.x + (b.uv.x - a.uv.x) * t,
	                a.uv.y + (b.uv.y - a.uv.y) * t);
	return out;
}

static Vector<TexturedPoint> ClipTexturedPolygon(const Vector<TexturedPoint>& source, const Rectf& clip)
{
	Vector<TexturedPoint> input;
	input.Reserve(source.GetCount());
	for(const TexturedPoint& p : source)
		input.Add(p);
	for(int e = ClipLeft; e <= ClipBottom && !input.IsEmpty(); ++e) {
		Vector<TexturedPoint> output;
		TexturedPoint previous = input.Top();
		bool previous_inside = Inside(previous.position, clip, (ClipEdge)e);
		for(const TexturedPoint& current : input) {
			const bool current_inside = Inside(current.position, clip, (ClipEdge)e);
			if(current_inside != previous_inside)
				output.Add(IntersectTextured(previous, current, clip, (ClipEdge)e));
			if(current_inside)
				output.Add(current);
			previous = current;
			previous_inside = current_inside;
		}
		input = pick(output);
	}
	return input;
}

static Vector<Pointf> RoundedPolygon(const RoundedRect& rounded, const Transform2D& transform)
{
	const Rectf& rect = rounded.rect;
	const double half_w = (rect.right - rect.left) * 0.5;
	const double half_h = (rect.bottom - rect.top) * 0.5;
	const double radius = min(max(rounded.radius, 0.0), min(half_w, half_h));
	if(radius <= 0.0)
		return TransformRect(rect, transform);

	const int segments = 8;
	const double pi = 3.14159265358979323846;
	const Pointf centers[4] = {
		Pointf(rect.right - radius, rect.top + radius),
		Pointf(rect.right - radius, rect.bottom - radius),
		Pointf(rect.left + radius, rect.bottom - radius),
		Pointf(rect.left + radius, rect.top + radius),
	};
	const double starts[4] = { -0.5 * pi, 0.0, 0.5 * pi, pi };
	Vector<Pointf> out;
	out.Reserve(segments * 4);
	for(int corner = 0; corner < 4; ++corner) {
		for(int i = 0; i < segments; ++i) {
			const double angle = starts[corner] + (0.5 * pi) * ((double)i / (segments - 1));
			Pointf p(centers[corner].x + std::cos(angle) * radius,
			         centers[corner].y + std::sin(angle) * radius);
			out.Add(transform.TransformPoint(p));
		}
	}
	return out;
}

static bool IsSupportedColorFormat(GpuFormat format)
{
	return format == GpuFormat::RGBA8 || format == GpuFormat::BGRA8 ||
	       format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
}

}

UiRenderer2D::UiRenderer2D(GpuDevice& gpu)
	: device(&gpu)
{
	const int required = GpuCapability_Buffers | GpuCapability_RenderPass |
	                     GpuCapability_Pipelines | GpuCapability_Shaders;
	if((gpu.GetAdapterInfo().capability_flags & required) != required) {
		error = "UiRenderer2D requires buffers, render passes, pipelines, and shaders";
		return;
	}
	ready = true;
}

UiRenderer2D::~UiRenderer2D()
{
	Close();
}

bool UiRenderer2D::Fail(const String& message)
{
	error = message;
	return false;
}

bool UiRenderer2D::EnsureShaders(bool textured)
{
	GpuShaderId& vs_id = textured ? textured_vertex_shader : vertex_shader;
	GpuShaderId& fs_id = textured ? textured_fragment_shader : fragment_shader;
	if(vs_id.IsValid() && fs_id.IsValid())
		return true;

	const uint32 *vs_words = textured ? kTexturedVertexShader : kVertexShader;
	const int vs_count = textured ? (int)(sizeof(kTexturedVertexShader) / sizeof(kTexturedVertexShader[0]))
	                              : (int)(sizeof(kVertexShader) / sizeof(kVertexShader[0]));
	const uint32 *fs_words = textured ? kTexturedFragmentShader : kFragmentShader;
	const int fs_count = textured ? (int)(sizeof(kTexturedFragmentShader) / sizeof(kTexturedFragmentShader[0]))
	                              : (int)(sizeof(kFragmentShader) / sizeof(kFragmentShader[0]));

	GpuShaderDesc vs;
	vs.stage = GpuShaderStage::Vertex;
	vs.format = GpuShaderFormat::SpirV;
	vs.code = SpirV(vs_words, vs_count);
	vs.label = textured ? "UiRenderer2D textured vertex" : "UiRenderer2D solid vertex";
	GpuResult result = device->CreateShader(vs, vs_id);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D vertex shader creation failed: " + DumpGpuResult(result));

	GpuShaderDesc fs;
	fs.stage = GpuShaderStage::Fragment;
	fs.format = GpuShaderFormat::SpirV;
	fs.code = SpirV(fs_words, fs_count);
	fs.label = textured ? "UiRenderer2D textured fragment" : "UiRenderer2D solid fragment";
	result = device->CreateShader(fs, fs_id);
	if(result != GpuResult::Ok) {
		device->DestroyShader(vs_id);
		vs_id = GpuShaderId();
		return Fail("UiRenderer2D fragment shader creation failed: " + DumpGpuResult(result));
	}
	return true;
}

bool UiRenderer2D::EnsurePipeline(GpuFormat format, bool textured, GpuPipelineId& out)
{
	for(const PipelineEntry& entry : pipelines) {
		if(entry.format == format && entry.textured == textured) {
			out = entry.pipeline;
			return true;
		}
	}
	if(!EnsureShaders(textured))
		return false;
	GpuPipelineDesc desc;
	desc.topology = GpuPrimitiveTopology::TriangleList;
	desc.color_format = format;
	desc.vertex_shader = textured ? textured_vertex_shader : vertex_shader;
	desc.fragment_shader = textured ? textured_fragment_shader : fragment_shader;
	desc.vertex_layout = textured ? GpuVertexLayout::Position2Uv2Color4F : GpuVertexLayout::Position2Color4F;
	desc.blend_mode = GpuBlendMode::SourceOver;
	desc.sampled_texture_count = textured ? 1 : 0;
	desc.sampler_filter = GpuSamplerFilter::Linear;
	desc.sampler_address = GpuSamplerAddressMode::ClampToEdge;
	desc.label = textured ? "UiRenderer2D image pipeline" : "UiRenderer2D solid pipeline";
	GpuPipelineId pipeline;
	GpuResult result = device->CreatePipeline(desc, pipeline);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D pipeline creation failed: " + DumpGpuResult(result));
	PipelineEntry& entry = pipelines.Add();
	entry.format = format;
	entry.textured = textured;
	entry.pipeline = pipeline;
	out = pipeline;
	return true;
}

bool UiRenderer2D::EnsureVertexBuffer(bool textured, int64 required_bytes)
{
	if(required_bytes <= 0)
		return true;
	GpuBufferId& buffer = textured ? textured_vertex_buffer : vertex_buffer;
	int64& capacity_ref = textured ? textured_vertex_buffer_capacity : vertex_buffer_capacity;
	if(buffer.IsValid() && capacity_ref >= required_bytes)
		return true;
	int64 capacity = capacity_ref > 0 ? capacity_ref : 4096;
	while(capacity < required_bytes) {
		if(capacity > std::numeric_limits<int64>::max() / 2)
			return Fail("UiRenderer2D vertex buffer size overflow");
		capacity *= 2;
	}
	if(buffer.IsValid()) {
		GpuResult destroy = device->DestroyBuffer(buffer);
		if(destroy != GpuResult::Ok)
			return Fail("UiRenderer2D old vertex buffer destruction failed: " + DumpGpuResult(destroy));
		buffer = GpuBufferId();
		capacity_ref = 0;
	}
	GpuBufferDesc desc;
	desc.size = capacity;
	desc.usage = GpuBufferUsage_Vertex | GpuBufferUsage_TransferDst;
	desc.label = textured ? "UiRenderer2D textured vertices" : "UiRenderer2D vertices";
	GpuResult result = device->CreateBuffer(desc, buffer);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D vertex buffer creation failed: " + DumpGpuResult(result));
	capacity_ref = capacity;
	if(textured)
		stats.textured_vertex_buffer_grew = true;
	else
		stats.vertex_buffer_grew = true;
	return true;
}

bool UiRenderer2D::EnsureImageTexture(const Image& image, GpuTextureId& out)
{
	out = GpuTextureId();
	if(image.IsEmpty())
		return true;
	if(image.IsPaintOnly())
		return Fail("UiRenderer2D cannot upload a paint-only Image");
	if((device->GetAdapterInfo().capability_flags & GpuCapability_Textures) == 0)
		return Fail("UiRenderer2D DrawImage requires texture support");

	const int64 serial = image.GetSerialId();
	for(const ImageCacheEntry& entry : image_cache) {
		if(entry.serial == serial && entry.size == image.GetSize()) {
			out = entry.texture;
			return true;
		}
	}

	GpuTextureDesc desc;
	desc.size = image.GetSize();
	desc.format = GpuFormat::RGBA8Srgb;
	desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
	desc.label = "UiRenderer2D image";
	GpuTextureId texture;
	GpuResult result = device->CreateTexture(desc, texture);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D image texture creation failed: " + DumpGpuResult(result));

	Image straight = Unmultiply(image);
	GpuTextureWriteDesc upload;
	upload.size = straight.GetSize();
	upload.row_pitch = (int64)straight.GetWidth() * (int)sizeof(RGBA);
	const int64 byte_count = (int64)straight.GetLength() * (int)sizeof(RGBA);
	result = device->WriteTexture(texture, upload, straight.Begin(), byte_count);
	if(result != GpuResult::Ok) {
		device->DestroyTexture(texture);
		return Fail("UiRenderer2D image texture upload failed: " + DumpGpuResult(result));
	}

	ImageCacheEntry& entry = image_cache.Add();
	entry.serial = serial;
	entry.size = image.GetSize();
	entry.texture = texture;
	stats.texture_upload_count++;
	out = texture;
	return true;
}

bool UiRenderer2D::BuildGeometry(const UiDisplayList& list, Size target_size)
{
	vertices.Clear();
	textured_vertices.Clear();
	batches.Clear();
	stats = UiRenderer2DStats();
	stats.display_op_count = list.GetCount();
	stats.vertex_buffer_capacity = vertex_buffer_capacity;
	stats.textured_vertex_buffer_capacity = textured_vertex_buffer_capacity;
	if(!list.IsValid())
		return Fail(list.GetError());
	if(target_size.cx <= 0 || target_size.cy <= 0)
		return Fail("UiRenderer2D target size must be positive");

	const Rectf target_clip(0, 0, target_size.cx, target_size.cy);
	ReplayState state;
	Vector<ReplayState> stack;

	auto extend_solid_batch = [&](int first, int count) {
		if(count <= 0)
			return;
		if(!batches.IsEmpty() && batches.Top().kind == BatchKind::Solid &&
		   batches.Top().first_vertex + batches.Top().vertex_count == first)
			batches.Top().vertex_count += count;
		else {
			Batch& batch = batches.Add();
			batch.kind = BatchKind::Solid;
			batch.first_vertex = first;
			batch.vertex_count = count;
		}
	};

	auto append_polygon = [&](const Vector<Pointf>& source, Rgba8 color) -> bool {
		Rectf effective_clip = target_clip;
		if(state.has_clip)
			effective_clip = IntersectRectf(effective_clip, state.clip);
		if(IsEmptyRectf(effective_clip)) {
			stats.clipped_primitive_count++;
			return true;
		}
		Vector<Pointf> polygon = ClipPolygon(source, effective_clip);
		if(polygon.GetCount() < 3) {
			stats.clipped_primitive_count++;
			return true;
		}
		if(polygon.GetCount() != source.GetCount())
			stats.clipped_primitive_count++;
		for(const Pointf& p : polygon)
			if(!IsFinitePoint(p))
				return Fail("UiRenderer2D generated non-finite geometry");
		const float scale = 1.0f / 255.0f;
		const float r = color.r * scale;
		const float g = color.g * scale;
		const float b = color.b * scale;
		const float a = color.a * scale;
		const int first = vertices.GetCount();
		auto add_vertex = [&](const Pointf& p) {
			Vertex& v = vertices.Add();
			v.x = (float)(2.0 * p.x / target_size.cx - 1.0);
			v.y = (float)(1.0 - 2.0 * p.y / target_size.cy);
			v.r = r; v.g = g; v.b = b; v.a = a;
			if(a > 0.0f && a < 1.0f)
				stats.translucent_vertex_count++;
		};
		for(int i = 1; i + 1 < polygon.GetCount(); ++i) {
			add_vertex(polygon[0]);
			add_vertex(polygon[i]);
			add_vertex(polygon[i + 1]);
			stats.triangle_count++;
		}
		extend_solid_batch(first, vertices.GetCount() - first);
		stats.emitted_primitive_count++;
		return true;
	};

	auto append_rect = [&](const Rectf& rect, Rgba8 color) -> bool {
		if(!IsFiniteRect(rect) || rect.right <= rect.left || rect.bottom <= rect.top)
			return Fail("UiRenderer2D received an invalid rectangle");
		return append_polygon(TransformRect(rect, state.transform), color);
	};

	auto append_image = [&](const Rectf& rect, const Image& image) -> bool {
		if(!IsFiniteRect(rect) || rect.right <= rect.left || rect.bottom <= rect.top)
			return Fail("UiRenderer2D received an invalid image rectangle");
		if(image.IsEmpty())
			return true;
		Rectf effective_clip = target_clip;
		if(state.has_clip)
			effective_clip = IntersectRectf(effective_clip, state.clip);
		if(IsEmptyRectf(effective_clip)) {
			stats.clipped_primitive_count++;
			return true;
		}
		Vector<TexturedPoint> source;
		source.SetCount(4);
		source[0].position = state.transform.TransformPoint(Pointf(rect.left, rect.top));
		source[0].uv = Pointf(0, 0);
		source[1].position = state.transform.TransformPoint(Pointf(rect.right, rect.top));
		source[1].uv = Pointf(1, 0);
		source[2].position = state.transform.TransformPoint(Pointf(rect.right, rect.bottom));
		source[2].uv = Pointf(1, 1);
		source[3].position = state.transform.TransformPoint(Pointf(rect.left, rect.bottom));
		source[3].uv = Pointf(0, 1);
		Vector<TexturedPoint> polygon = ClipTexturedPolygon(source, effective_clip);
		if(polygon.GetCount() < 3) {
			stats.clipped_primitive_count++;
			return true;
		}
		if(polygon.GetCount() != source.GetCount())
			stats.clipped_primitive_count++;
		for(const TexturedPoint& p : polygon)
			if(!IsFinitePoint(p.position) || !IsFinitePoint(p.uv))
				return Fail("UiRenderer2D generated non-finite image geometry");

		GpuTextureId texture;
		if(!EnsureImageTexture(image, texture))
			return false;
		const int first = textured_vertices.GetCount();
		auto add_vertex = [&](const TexturedPoint& p) {
			TexturedVertex& v = textured_vertices.Add();
			v.x = (float)(2.0 * p.position.x / target_size.cx - 1.0);
			v.y = (float)(1.0 - 2.0 * p.position.y / target_size.cy);
			v.u = (float)p.uv.x;
			v.v = (float)p.uv.y;
		};
		for(int i = 1; i + 1 < polygon.GetCount(); ++i) {
			add_vertex(polygon[0]);
			add_vertex(polygon[i]);
			add_vertex(polygon[i + 1]);
			stats.triangle_count++;
		}
		const int count = textured_vertices.GetCount() - first;
		if(!batches.IsEmpty() && batches.Top().kind == BatchKind::Image &&
		   batches.Top().texture == texture &&
		   batches.Top().first_vertex + batches.Top().vertex_count == first)
			batches.Top().vertex_count += count;
		else {
			Batch& batch = batches.Add();
			batch.kind = BatchKind::Image;
			batch.first_vertex = first;
			batch.vertex_count = count;
			batch.texture = texture;
		}
		stats.image_count++;
		stats.emitted_primitive_count++;
		return true;
	};

	for(int i = 0; i < list.GetCount(); ++i) {
		const UiDisplayOp& op = list[i];
		switch(op.type) {
		case UiDisplayOpType::Save:
			stack.Add(state);
			break;
		case UiDisplayOpType::Restore:
			if(stack.IsEmpty())
				return Fail("UiRenderer2D restore without matching save");
			state = stack.Pop();
			break;
		case UiDisplayOpType::ClipRect:
			if(!IsFiniteRect(op.rect))
				return Fail("UiRenderer2D received a non-finite clip rectangle");
			if(!state.has_clip) {
				state.clip = op.rect;
				state.has_clip = true;
			}
			else
				state.clip = IntersectRectf(state.clip, op.rect);
			break;
		case UiDisplayOpType::ConcatTransform:
			if(!IsFiniteTransform(op.transform))
				return Fail("UiRenderer2D received a non-finite transform");
			state.transform = state.transform * op.transform;
			break;
		case UiDisplayOpType::FillRect:
			stats.primitive_count++;
			if(!append_rect(op.rect, op.color))
				return false;
			break;
		case UiDisplayOpType::StrokeRect: {
			stats.primitive_count++;
			if(!IsFiniteRect(op.rect) || !IsFinite(op.width) || op.width <= 0.0 ||
			   op.rect.right <= op.rect.left || op.rect.bottom <= op.rect.top)
				return Fail("UiRenderer2D received an invalid stroke rectangle");
			const double h = op.width * 0.5;
			Rectf outer(op.rect.left - h, op.rect.top - h, op.rect.right + h, op.rect.bottom + h);
			Rectf inner(op.rect.left + h, op.rect.top + h, op.rect.right - h, op.rect.bottom - h);
			if(IsEmptyRectf(inner)) {
				if(!append_rect(outer, op.color)) return false;
				break;
			}
			if(!append_rect(Rectf(outer.left, outer.top, outer.right, inner.top), op.color)) return false;
			if(!append_rect(Rectf(outer.left, inner.bottom, outer.right, outer.bottom), op.color)) return false;
			if(!append_rect(Rectf(outer.left, inner.top, inner.left, inner.bottom), op.color)) return false;
			if(!append_rect(Rectf(inner.right, inner.top, outer.right, inner.bottom), op.color)) return false;
			break;
		}
		case UiDisplayOpType::FillRoundedRect:
			stats.primitive_count++;
			if(!IsFiniteRect(op.rounded.rect) || !IsFinite(op.rounded.radius) ||
			   op.rounded.rect.right <= op.rounded.rect.left || op.rounded.rect.bottom <= op.rounded.rect.top)
				return Fail("UiRenderer2D received an invalid rounded rectangle");
			if(!append_polygon(RoundedPolygon(op.rounded, state.transform), op.color))
				return false;
			break;
		case UiDisplayOpType::DrawImage:
			stats.primitive_count++;
			if(!append_image(op.rect, op.image))
				return false;
			break;
		}
	}
	if(!stack.IsEmpty())
		return Fail("UiRenderer2D replay ended with unbalanced save state");
	stats.textured_vertex_count = textured_vertices.GetCount();
	stats.vertex_count = vertices.GetCount() + textured_vertices.GetCount();
	stats.uploaded_bytes = (int64)vertices.GetCount() * sizeof(Vertex) +
	                       (int64)textured_vertices.GetCount() * sizeof(TexturedVertex);
	stats.batch_count = batches.GetCount();
	return true;
}

bool UiRenderer2D::Submit(const UiRenderer2DTarget& target, GpuPipelineId solid_pipeline,
                          GpuPipelineId textured_pipeline)
{
	GpuCommandListId commands;
	GpuResult result = device->BeginCommands(commands);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D BeginCommands failed: " + DumpGpuResult(result));

	bool pass_active = false;
	auto cleanup_commands = [&]() {
		if(pass_active) {
			device->EndRenderPass(commands);
			pass_active = false;
		}
		if(device->EndCommands(commands) == GpuResult::Ok)
			device->Submit(commands);
	};

	GpuRenderPassDesc pass;
	pass.color_target = target.color_target;
	pass.color_format = target.color_format;
	pass.color_load = target.load_op;
	pass.color_store = target.store_op;
	pass.clear_color = target.clear_color;
	pass.label = "UiRenderer2D frame";
	result = device->BeginRenderPass(commands, pass);
	if(result != GpuResult::Ok) {
		cleanup_commands();
		return Fail("UiRenderer2D BeginRenderPass failed: " + DumpGpuResult(result));
	}
	pass_active = true;

	for(const Batch& batch : batches) {
		const bool textured = batch.kind == BatchKind::Image;
		result = device->SetPipeline(commands, textured ? textured_pipeline : solid_pipeline);
		if(result != GpuResult::Ok) {
			cleanup_commands();
			return Fail("UiRenderer2D SetPipeline failed: " + DumpGpuResult(result));
		}
		result = device->SetVertexBuffer(commands, textured ? textured_vertex_buffer : vertex_buffer);
		if(result != GpuResult::Ok) {
			cleanup_commands();
			return Fail("UiRenderer2D SetVertexBuffer failed: " + DumpGpuResult(result));
		}
		if(textured) {
			result = device->SetSampledTexture(commands, 0, batch.texture);
			if(result != GpuResult::Ok) {
				cleanup_commands();
				return Fail("UiRenderer2D SetSampledTexture failed: " + DumpGpuResult(result));
			}
		}
		result = device->Draw(commands, batch.vertex_count, batch.first_vertex);
		if(result != GpuResult::Ok) {
			cleanup_commands();
			return Fail("UiRenderer2D Draw failed: " + DumpGpuResult(result));
		}
		stats.draw_count++;
	}

	result = device->EndRenderPass(commands);
	if(result != GpuResult::Ok) {
		cleanup_commands();
		return Fail("UiRenderer2D EndRenderPass failed: " + DumpGpuResult(result));
	}
	pass_active = false;
	result = device->EndCommands(commands);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D EndCommands failed: " + DumpGpuResult(result));
	result = device->Submit(commands);
	if(result != GpuResult::Ok)
		return Fail("UiRenderer2D Submit failed: " + DumpGpuResult(result));
	return true;
}

bool UiRenderer2D::Render(const UiDisplayList& list, const UiRenderer2DTarget& target)
{
	error.Clear();
	if(!ready || !device)
		return Fail("UiRenderer2D is not ready");
	if(!target.color_target.IsValid() || target.size.cx <= 0 || target.size.cy <= 0 ||
	   !IsSupportedColorFormat(target.color_format))
		return Fail("UiRenderer2D target is invalid or unsupported");
	if(!BuildGeometry(list, target.size))
		return false;

	GpuPipelineId solid_pipeline;
	GpuPipelineId textured_pipeline;
	if(!vertices.IsEmpty() && !EnsurePipeline(target.color_format, false, solid_pipeline))
		return false;
	if(!textured_vertices.IsEmpty() && !EnsurePipeline(target.color_format, true, textured_pipeline))
		return false;

	const int64 solid_bytes = (int64)vertices.GetCount() * sizeof(Vertex);
	const int64 textured_bytes = (int64)textured_vertices.GetCount() * sizeof(TexturedVertex);
	if(!EnsureVertexBuffer(false, solid_bytes) || !EnsureVertexBuffer(true, textured_bytes))
		return false;
	stats.vertex_buffer_capacity = vertex_buffer_capacity;
	stats.textured_vertex_buffer_capacity = textured_vertex_buffer_capacity;
	if(!vertices.IsEmpty()) {
		GpuResult result = device->WriteBuffer(vertex_buffer, 0, vertices.Begin(), solid_bytes);
		if(result != GpuResult::Ok)
			return Fail("UiRenderer2D vertex upload failed: " + DumpGpuResult(result));
	}
	if(!textured_vertices.IsEmpty()) {
		GpuResult result = device->WriteBuffer(textured_vertex_buffer, 0, textured_vertices.Begin(), textured_bytes);
		if(result != GpuResult::Ok)
			return Fail("UiRenderer2D textured vertex upload failed: " + DumpGpuResult(result));
	}
	return Submit(target, solid_pipeline, textured_pipeline);
}

bool UiRenderer2D::RenderFrame(const UiDisplayList& list, const GpuFrameInfo& frame,
                               const GpuClearColor& clear_color)
{
	UiRenderer2DTarget target;
	target.color_target = frame.color_target;
	target.size = frame.size;
	target.color_format = frame.color_format;
	target.load_op = GpuLoadOp::Clear;
	target.store_op = GpuStoreOp::Store;
	target.clear_color = clear_color;
	return Render(list, target);
}

void UiRenderer2D::Close()
{
	if(!device)
		return;
	for(int i = pipelines.GetCount() - 1; i >= 0; --i)
		if(pipelines[i].pipeline.IsValid())
			device->DestroyPipeline(pipelines[i].pipeline);
	pipelines.Clear();
	if(vertex_buffer.IsValid())
		device->DestroyBuffer(vertex_buffer);
	if(textured_vertex_buffer.IsValid())
		device->DestroyBuffer(textured_vertex_buffer);
	vertex_buffer = GpuBufferId();
	textured_vertex_buffer = GpuBufferId();
	vertex_buffer_capacity = 0;
	textured_vertex_buffer_capacity = 0;
	if(vertex_shader.IsValid())
		device->DestroyShader(vertex_shader);
	if(fragment_shader.IsValid())
		device->DestroyShader(fragment_shader);
	if(textured_vertex_shader.IsValid())
		device->DestroyShader(textured_vertex_shader);
	if(textured_fragment_shader.IsValid())
		device->DestroyShader(textured_fragment_shader);
	vertex_shader = GpuShaderId();
	fragment_shader = GpuShaderId();
	textured_vertex_shader = GpuShaderId();
	textured_fragment_shader = GpuShaderId();
	for(int i = image_cache.GetCount() - 1; i >= 0; --i)
		if(image_cache[i].texture.IsValid())
			device->DestroyTexture(image_cache[i].texture);
	image_cache.Clear();
	vertices.Clear();
	textured_vertices.Clear();
	batches.Clear();
	ready = false;
}

}
