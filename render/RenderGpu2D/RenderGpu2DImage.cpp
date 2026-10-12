#include "RenderGpu2D.h"

#include <cmath>

namespace Upp {


bool UiRenderer2D::RenderTexture(GpuTextureId image, const UiRenderer2DTarget& target)
{
	error.Clear();
	if(!ready || !device || !image.IsValid() || !target.color_target.IsValid() ||
	   target.size.cx <= 0 || target.size.cy <= 0 ||
	   (target.color_format != GpuFormat::RGBA8 && target.color_format != GpuFormat::BGRA8 &&
	    target.color_format != GpuFormat::RGBA8Srgb && target.color_format != GpuFormat::BGRA8Srgb))
		return Fail("UiRenderer2D GPU image target is invalid");
	vertices.Clear(); textured_vertices.Clear(); batches.Clear(); stats = UiRenderer2DStats();
	if(!GeometryFits(6 * sizeof(TexturedVertex))) return false;
	GpuPipelineId pipeline;
	if(!EnsurePipeline(target.color_format, true, pipeline, GpuBlendMode::PremultipliedSourceOver) ||
	   !EnsureVertexBuffer(true, 6 * sizeof(TexturedVertex))) return false;
	const int corner[] = {0, 1, 2, 0, 2, 3};
	const float x[] = {-1, 1, 1, -1}, y[] = {1, 1, -1, -1};
	const float u[] = {0, 1, 1, 0}, v[] = {0, 0, 1, 1};
	for(int c : corner) {
		auto& vertex = textured_vertices.Add();
		vertex.x = x[c]; vertex.y = y[c]; vertex.u = u[c]; vertex.v = v[c];
	}
	if(device->WriteBuffer(textured_vertex_buffer, 0, textured_vertices.Begin(), 6 * sizeof(TexturedVertex)) != GpuResult::Ok)
		return Fail("UiRenderer2D GPU image vertices failed: " + device->GetLastError());
	auto& batch = batches.Add(); batch.kind = BatchKind::Image; batch.texture = image; batch.vertex_count = 6;
	stats.image_count = stats.batch_count = stats.emitted_primitive_count = 1;
	stats.triangle_count = 2; stats.vertex_count = stats.textured_vertex_count = 6;
	stats.uploaded_bytes = 6 * sizeof(TexturedVertex);
	stats.textured_vertex_buffer_capacity = textured_vertex_buffer_capacity;
	return Submit(target, GpuPipelineId(), GpuPipelineId(), pipeline, GpuPipelineId());
}
bool UiRenderer2D::RenderTextureFrame(GpuTextureId image, const GpuFrameInfo& frame)
{
	UiRenderer2DTarget target;
	target.color_target = frame.color_target; target.size = frame.size; target.color_format = frame.color_format;
	return RenderTexture(image, target);
}

Image UiRenderer2D::PrepareImagePixels(const Image& image, GpuFormat format)
{
	// UNORM sampling already matches U++ premultiplied pixels; retain original bytes.
	if(image.IsEmpty() || (format != GpuFormat::RGBA8Srgb && format != GpuFormat::BGRA8Srgb))
		return image;

	ImageBuffer output(image.GetSize());
	for(int y = 0; y < image.GetHeight(); ++y) {
		const RGBA *source = image[y];
		RGBA *target = output[y];
		for(int x = 0; x < image.GetWidth(); ++x) {
			const RGBA& s = source[x];
			RGBA& d = target[x];
			d.a = s.a;
			// Hardware sRGB decoding must produce colour premultiplied in linear space.
			auto channel = [&](byte value) -> byte {
				if(!s.a) return 0;
				double c = min(1.0, (double)value / s.a);
				c = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
				c *= s.a / 255.0;
				c = c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
				return (byte)min(255, max(0, (int)std::round(c * 255)));
			};
			d.r = channel(s.r); d.g = channel(s.g); d.b = channel(s.b);
		}
	}
	return Image(output);
}

}
