#include "RenderGpu2D.h"

#include <cmath>
#include <limits>

// Keep the base replay and feature extensions in one translation unit so text/vector
// wrappers can reuse UiRenderer2D private state without publishing a second internal API.
// Vector/SVG content is materialized into ordinary DrawImage intent before replay.
#define BuildGeometry BuildGeometryBase
#define Render RenderBase
#define RenderFrame RenderFrameBase
#define Close CloseBase
#include "RenderGpu2DBase.inc"
#undef BuildGeometry
#undef Render
#undef RenderFrame
#undef Close

namespace Upp {

float UiRenderer2D::ColorChannel(byte channel) const
{
	const float value = channel / 255.0f;
	if(working_color_format != GpuFormat::RGBA8Srgb && working_color_format != GpuFormat::BGRA8Srgb)
		return value;
	return value <= 0.04045f ? value / 12.92f : (float)std::pow((value + 0.055f) / 1.055f, 2.4f);
}

bool UiRenderer2D::BuildGeometry(const UiDisplayList& list, Size target_size)
{
	bool has_text = false;
	for(int i = 0; i < list.GetCount(); ++i)
		if(list[i].type == UiDisplayOpType::DrawText) {
			has_text = true;
			break;
		}
	if(!has_text)
		return BuildGeometryBase(list, target_size);

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

	auto extend_solid_batch = [&](int first, int count, BatchKind kind) {
		if(count <= 0)
			return;
		if(!batches.IsEmpty() && batches.Top().kind == kind &&
		   batches.Top().first_vertex + batches.Top().vertex_count == first)
			batches.Top().vertex_count += count;
		else {
			Batch& batch = batches.Add();
			batch.kind = kind;
			batch.first_vertex = first;
			batch.vertex_count = count;
		}
	};

	auto append_polygon = [&](const Vector<Pointf>& source, Rgba8 color,
	                          BatchKind kind = BatchKind::Solid) -> bool {
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
		const float r = ColorChannel(color.r);
		const float g = ColorChannel(color.g);
		const float b = ColorChannel(color.b);
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
		extend_solid_batch(first, vertices.GetCount() - first, kind);
		stats.emitted_primitive_count++;
		return true;
	};

	auto append_rect = [&](const Rectf& rect, Rgba8 color,
	                       BatchKind kind = BatchKind::Solid) -> bool {
		if(!IsFiniteRect(rect) || rect.right <= rect.left || rect.bottom <= rect.top)
			return Fail("UiRenderer2D received an invalid rectangle");
		return append_polygon(TransformRect(rect, state.transform), color, kind);
	};

	auto append_textured = [&](const Rectf& rect, const Rectf& uv, GpuTextureId texture,
	                           Rgba8 tint, const char *kind) -> bool {
		if(!texture.IsValid())
			return Fail(String("UiRenderer2D ") + kind + " has no sampled texture");
		if(!IsFiniteRect(rect) || rect.right <= rect.left || rect.bottom <= rect.top ||
		   !IsFiniteRect(uv))
			return Fail(String("UiRenderer2D received invalid ") + kind + " geometry");
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
		source[0].uv = Pointf(uv.left, uv.top);
		source[1].position = state.transform.TransformPoint(Pointf(rect.right, rect.top));
		source[1].uv = Pointf(uv.right, uv.top);
		source[2].position = state.transform.TransformPoint(Pointf(rect.right, rect.bottom));
		source[2].uv = Pointf(uv.right, uv.bottom);
		source[3].position = state.transform.TransformPoint(Pointf(rect.left, rect.bottom));
		source[3].uv = Pointf(uv.left, uv.bottom);
		Vector<TexturedPoint> polygon = ClipTexturedPolygon(source, effective_clip);
		if(polygon.GetCount() < 3) {
			stats.clipped_primitive_count++;
			return true;
		}
		if(polygon.GetCount() != source.GetCount())
			stats.clipped_primitive_count++;
		for(const TexturedPoint& p : polygon)
			if(!IsFinitePoint(p.position) || !IsFinitePoint(p.uv))
				return Fail(String("UiRenderer2D generated non-finite ") + kind + " geometry");

		const float scale = 1.0f / 255.0f;
		const float r = ColorChannel(tint.r);
		const float g = ColorChannel(tint.g);
		const float b = ColorChannel(tint.b);
		const float a = tint.a * scale;
		const int first = textured_vertices.GetCount();
		auto add_vertex = [&](const TexturedPoint& p) {
			TexturedVertex& v = textured_vertices.Add();
			v.x = (float)(2.0 * p.position.x / target_size.cx - 1.0);
			v.y = (float)(1.0 - 2.0 * p.position.y / target_size.cy);
			v.u = (float)p.uv.x;
			v.v = (float)p.uv.y;
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
		const int count = textured_vertices.GetCount() - first;
		if(!batches.IsEmpty() && batches.Top().kind == BatchKind::Image &&
		   batches.Top().texture == texture &&
		   batches.Top().first_vertex + batches.Top().vertex_count == first)
			batches.Top().vertex_count += count;
		else if(count > 0) {
			Batch& batch = batches.Add();
			batch.kind = BatchKind::Image;
			batch.first_vertex = first;
			batch.vertex_count = count;
			batch.texture = texture;
		}
		return true;
	};

	auto append_image = [&](const Rectf& rect, const Image& image) -> bool {
		if(!IsFiniteRect(rect) || rect.right <= rect.left || rect.bottom <= rect.top)
			return Fail("UiRenderer2D received an invalid image rectangle");
		if(image.IsEmpty())
			return true;
		GpuTextureId texture;
		if(!EnsureImageTexture(image, texture))
			return false;
		const int before = textured_vertices.GetCount();
		if(!append_textured(rect, Rectf(0, 0, 1, 1), texture, Rgba8(255, 255, 255, 255), "image"))
			return false;
		stats.image_count++;
		if(textured_vertices.GetCount() > before)
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
			{
				Rectf resolved_clip;
				if(!TransformAxisAlignedClipRect(op.rect, state.transform, resolved_clip))
					return Fail("UiRenderer2D ClipRect under a non-axis-aligned transform is not representable by the current rectangular clip contract");
				if(!state.has_clip) {
					state.clip = resolved_clip;
					state.has_clip = true;
				}
				else
					state.clip = IntersectRectf(state.clip, resolved_clip);
			}
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
		case UiDisplayOpType::InvertRect:
			stats.primitive_count++;
			if(!append_rect(op.rect, Rgba8(255, 255, 255, 255), BatchKind::Invert))
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
		case UiDisplayOpType::DrawText: {
			stats.primitive_count++;
			stats.text_run_count++;
			if(!IsFinitePoint(op.point))
				return Fail("UiRenderer2D received a non-finite text position");
			if(op.text.IsEmpty())
				break;

			Font run_font = op.font;
			run_font.RealizeStd();
			double pen_x = op.point.x;
			bool emitted = false;
			for(int c = 0; c < op.text.GetCount(); ++c) {
				GlyphDraw glyph;
				if(!EnsureGlyph(run_font, op.text[c], glyph))
					return false;
				stats.glyph_count++;
				if(glyph.drawable) {
					Rectf glyph_rect(pen_x + glyph.offset.x,
					                 op.point.y + glyph.offset.y,
					                 pen_x + glyph.offset.x + glyph.size.cx,
					                 op.point.y + glyph.offset.y + glyph.size.cy);
					const int before = textured_vertices.GetCount();
					if(!append_textured(glyph_rect, glyph.uv, glyph.texture, op.color, "glyph"))
						return false;
					emitted = emitted || textured_vertices.GetCount() > before;
				}
				pen_x += glyph.advance;
			}
			if(emitted)
				stats.emitted_primitive_count++;

			const double ascent = run_font.GetAscent();
			const double thickness = max(ascent / 16.0, 1.0);
			const double run_width = pen_x - op.point.x;
			if(run_width > 0 && run_font.IsUnderline()) {
				const double y = op.point.y + ascent + thickness;
				if(!append_rect(Rectf(op.point.x, y, op.point.x + run_width, y + thickness), op.color))
					return false;
			}
			if(run_width > 0 && run_font.IsStrikeout()) {
				const double y = op.point.y + 2 * ascent / 3.0;
				if(!append_rect(Rectf(op.point.x, y, op.point.x + run_width, y + thickness), op.color))
					return false;
			}
			break;
		}
		case UiDisplayOpType::FillPath:
		case UiDisplayOpType::StrokePath:
		case UiDisplayOpType::DrawSvg:
			return Fail("UiRenderer2D internal error: vector operation was not materialized");
		}
	}
	if(!stack.IsEmpty())
		return Fail("UiRenderer2D replay ended with unbalanced save state");
	stats.textured_vertex_count = textured_vertices.GetCount();
	stats.vertex_count = vertices.GetCount() + textured_vertices.GetCount();
	stats.uploaded_bytes = (int64)vertices.GetCount() * sizeof(Vertex) +
	                       (int64)textured_vertices.GetCount() * sizeof(TexturedVertex);
	stats.batch_count = batches.GetCount();
	if(text_impl)
		stats.glyph_atlas_page_count = text_impl->pages.GetCount();
	return true;
}

bool UiRenderer2D::RenderInternal(const UiDisplayList& list, const UiRenderer2DTarget& target)
{
	working_color_format = target.color_format;
	bool has_vector = false;
	for(int i = 0; i < list.GetCount(); ++i) {
		const UiDisplayOpType type = list[i].type;
		if(type == UiDisplayOpType::FillPath || type == UiDisplayOpType::StrokePath ||
		   type == UiDisplayOpType::DrawSvg) {
			has_vector = true;
			break;
		}
	}
	if(has_vector) {
		error.Clear();
		if(!ready || !device)
			return Fail("UiRenderer2D is not ready");
		if(!target.color_target.IsValid() || target.size.cx <= 0 || target.size.cy <= 0 ||
		   !IsSupportedColorFormat(target.color_format))
			return Fail("UiRenderer2D target is invalid or unsupported");

		UiDisplayList materialized;
		UiRenderer2DStats vector_stats;
		if(!MaterializeVectorList(list, materialized, vector_stats))
			return false;
		if(!RenderInternal(materialized, target))
			return false;

		stats.display_op_count = list.GetCount();
		stats.vector_op_count = vector_stats.vector_op_count;
		stats.vector_path_count = vector_stats.vector_path_count;
		stats.gradient_count = vector_stats.gradient_count;
		stats.svg_count = vector_stats.svg_count;
		stats.vector_cache_miss_count = vector_stats.vector_cache_miss_count;
		stats.vector_cache_entry_count = vector_stats.vector_cache_entry_count;
		stats.vector_raster_count = vector_stats.vector_raster_count;
		return true;
	}

	bool has_text = false;
	for(int i = 0; i < list.GetCount(); ++i)
		if(list[i].type == UiDisplayOpType::DrawText) {
			has_text = true;
			break;
		}
	if(!has_text)
		return RenderBase(list, target);

	error.Clear();
	if(!ready || !device)
		return Fail("UiRenderer2D is not ready");
	if(!target.color_target.IsValid() || target.size.cx <= 0 || target.size.cy <= 0 ||
	   !IsSupportedColorFormat(target.color_format))
		return Fail("UiRenderer2D target is invalid or unsupported");
	if(!BuildGeometry(list, target.size))
		return false;

	GpuPipelineId solid_pipeline;
	GpuPipelineId invert_pipeline;
	GpuPipelineId textured_pipeline;
	bool has_invert = false;
	for(const Batch& batch : batches)
		if(batch.kind == BatchKind::Invert) {
			has_invert = true;
			break;
		}
	if(!vertices.IsEmpty() && !EnsurePipeline(target.color_format, false, solid_pipeline))
		return false;
	if(has_invert && !EnsurePipeline(target.color_format, false, invert_pipeline,
	                                GpuBlendMode::DestinationInvert))
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
	return Submit(target, solid_pipeline, invert_pipeline, textured_pipeline);
}

void UiRenderer2D::SetCacheLimits(const UiRenderer2DCacheLimits& limits)
{
	cache_limits.image_bytes = max<int64>(0, limits.image_bytes);
	cache_limits.image_entries = max(0, limits.image_entries);
	cache_limits.vector_bytes = max<int64>(0, limits.vector_bytes);
	cache_limits.vector_entries = max(0, limits.vector_entries);
	cache_limits.glyph_bytes = max<int64>(0, limits.glyph_bytes);
	cache_limits.glyph_entries = max(0, limits.glyph_entries);
	TrimCaches();
	UpdateCacheStats();
}

int64 UiRenderer2D::ImageCacheBytes() const
{
	int64 bytes = 0;
	for(const ImageCacheEntry& entry : image_cache)
		bytes += (int64)entry.size.cx * entry.size.cy * sizeof(RGBA);
	return bytes;
}

int64 UiRenderer2D::VectorCacheBytes() const
{
	int64 bytes = 0;
	if(vector_impl)
		for(const VectorImpl::CacheEntry& entry : vector_impl->cache)
			bytes += (int64)entry.image.GetLength() * sizeof(RGBA);
	return bytes;
}

bool UiRenderer2D::ReserveImageCache(int64 bytes)
{
	if(bytes > cache_limits.image_bytes || cache_limits.image_entries == 0)
		return Fail("UiRenderer2D image exceeds the configured pixel cache budget");
	while(ImageCacheBytes() > cache_limits.image_bytes - bytes ||
	      image_cache.GetCount() >= cache_limits.image_entries) {
		int oldest = -1;
		for(int i = 0; i < image_cache.GetCount(); ++i)
			if(image_cache[i].last_frame != cache_frame &&
			   (oldest < 0 || image_cache[i].last_frame < image_cache[oldest].last_frame))
				oldest = i;
		if(oldest < 0)
			return Fail("UiRenderer2D active image frame exceeds the configured pixel cache budget");
		GpuResult result = device->DestroyTexture(image_cache[oldest].texture);
		if(result != GpuResult::Ok)
			return Fail("UiRenderer2D cache retirement failed: " + DumpGpuResult(result));
		image_cache.Remove(oldest);
		frame_image_evictions++;
	}
	return true;
}

bool UiRenderer2D::ReserveVectorCache(int64 bytes)
{
	if(bytes > cache_limits.vector_bytes || cache_limits.vector_entries == 0)
		return Fail("UiRenderer2D vector exceeds the configured raster cache budget");
	while(VectorCacheBytes() > cache_limits.vector_bytes - bytes ||
	      vector_impl->cache.GetCount() >= cache_limits.vector_entries) {
		int oldest = -1;
		for(int i = 0; i < vector_impl->cache.GetCount(); ++i)
			if(vector_impl->cache[i].last_frame != cache_frame &&
			   (oldest < 0 || vector_impl->cache[i].last_frame < vector_impl->cache[oldest].last_frame))
				oldest = i;
		if(oldest < 0)
			return Fail("UiRenderer2D active vector frame exceeds the configured raster cache budget");
		vector_impl->cache.Remove(oldest);
		frame_vector_evictions++;
	}
	return true;
}

void UiRenderer2D::TrimCaches()
{
	if(text_impl && ((int64)text_impl->pages.GetCount() * TextImpl::ATLAS_SIZE *
	                  TextImpl::ATLAS_SIZE * sizeof(RGBA) > cache_limits.glyph_bytes ||
	                  text_impl->glyphs.GetCount() > cache_limits.glyph_entries))
		DestroyTextExtension();
	// Only called between replays. Existing backend destruction waits for GPU
	// completion; this must remain true if submission later becomes asynchronous.
	for(int i = image_cache.GetCount() - 1; i >= 0 && (ImageCacheBytes() > cache_limits.image_bytes ||
	     image_cache.GetCount() > cache_limits.image_entries); --i)
		if(device->DestroyTexture(image_cache[i].texture) == GpuResult::Ok)
			image_cache.Remove(i);
	if(vector_impl)
		while(!vector_impl->cache.IsEmpty() &&
		      (VectorCacheBytes() > cache_limits.vector_bytes ||
		       vector_impl->cache.GetCount() > cache_limits.vector_entries))
			vector_impl->cache.Remove(0);
}

void UiRenderer2D::UpdateCacheStats()
{
	stats.glyph_cache_bytes = text_impl ? (int64)text_impl->pages.GetCount() *
	                         TextImpl::ATLAS_SIZE * TextImpl::ATLAS_SIZE * sizeof(RGBA) : 0;
	stats.glyph_cache_entry_count = text_impl ? text_impl->glyphs.GetCount() : 0;
	stats.image_cache_bytes = ImageCacheBytes();
	stats.vector_cache_bytes = VectorCacheBytes();
	stats.image_cache_entry_count = image_cache.GetCount();
	stats.image_cache_hit_count = frame_image_hits;
	stats.image_cache_eviction_count = frame_image_evictions;
	stats.vector_cache_eviction_count = frame_vector_evictions;
	stats.image_upload_bytes = frame_image_upload_bytes;
	if(vector_impl)
		stats.vector_cache_entry_count = vector_impl->cache.GetCount();
}

bool UiRenderer2D::Render(const UiDisplayList& list, const UiRenderer2DTarget& target)
{
	++cache_frame;
	frame_image_hits = frame_image_evictions = frame_vector_evictions = 0;
	frame_image_upload_bytes = 0;
	// Reset an exhausted atlas only between frames. Never invalidate glyph
	// handles already referenced by the frame currently being recorded.
	bool reset_glyphs = text_impl &&
		((int64)text_impl->pages.GetCount() * TextImpl::ATLAS_SIZE *
		 TextImpl::ATLAS_SIZE * sizeof(RGBA) >= cache_limits.glyph_bytes ||
		 text_impl->glyphs.GetCount() >= cache_limits.glyph_entries);
	if(reset_glyphs) DestroyTextExtension();
	bool result = RenderInternal(list, target);
	stats.glyph_cache_reset_count = reset_glyphs ? 1 : 0;
	UpdateCacheStats();
	return result;
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
	DestroyVectorExtension();
	DestroyTextExtension();
	CloseBase();
	stats = UiRenderer2DStats();
}

}
