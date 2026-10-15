#include "RenderGpu2D.h"

#include <Painter/Painter.h>
#include <cstring>

namespace Upp {
namespace {
struct GpuGlyphOutline : LinearPathConsumer {
	UiPath path; bool failed = false;
	void Move(const Pointf& p) override { if(path.GetCount() >= 4096) failed = true; if(!failed) path.MoveTo(p); }
	void Line(const Pointf& p) override { if(path.GetCount() >= 4096) failed = true; if(!failed) path.LineTo(p); }
};
}

UiRenderer2D::TextCleanup::~TextCleanup()
{
	if(owner)
		owner->DestroyTextExtension();
}

UiRenderer2D::TextImpl& UiRenderer2D::Text()
{
	if(!text_impl) {
		text_impl = new TextImpl;
		text_cleanup.owner = this;
	}
	return *text_impl;
}

void UiRenderer2D::DestroyTextExtension()
{
	if(gpu_vector_rasterizer) gpu_vector_rasterizer->DiscardPending();
	if(!text_impl)
		return;
	if(device) {
		for(int i = text_impl->pages.GetCount() - 1; i >= 0; --i)
			if(text_impl->pages[i].texture.IsValid())
				device->DestroyTexture(text_impl->pages[i].texture);
	}
	delete text_impl;
	text_impl = nullptr;
	text_cleanup.owner = nullptr;
}

bool UiRenderer2D::EnsureGlyph(Font font, int ch, GlyphDraw& out, int depth)
{
	out = GlyphDraw();
	if(!device)
		return Fail("UiRenderer2D glyph cache has no device");
	if((device->GetAdapterInfo().capability_flags & GpuCapability_Textures) == 0)
		return Fail("UiRenderer2D DrawText requires texture support");

	font.Underline(false).Strikeout(false);
	font.RealizeStd();
	if(depth >= 8) return Fail("UiRenderer2D glyph fallback recursion budget exceeded");
	const GlyphInfo info = GetGlyphInfo(font, ch);
	if(!info.IsNormal()) {
		out.advance = max(0, font.GetWidth(ch));
		auto append = [&](GlyphDraw& component, Pointf offset) {
			if(component.drawable) {
				auto& p = out.parts.Add(); p.texture = component.texture; p.uv = component.uv;
				p.offset = component.offset + offset; p.size = component.size;
			}
			for(auto& p : component.parts) { p.offset += offset; out.parts.Add(p); }
		};
		if(info.IsComposed()) {
			ComposedGlyph composed; Compose(font, ch, composed);
			GlyphDraw base, mark;
			if(!EnsureGlyph(font, composed.basic_char, base, depth + 1) ||
			   !EnsureGlyph(composed.mark_font, composed.mark_char, mark, depth + 1)) return false;
			append(base, Pointf(0,0)); append(mark, composed.mark_pos);
		}
		else {
			Font fallback = font; int scalar = ch; Pointf offset(0,0);
			if(info.IsReplaced()) {
				fallback.Face(info.lspc).Height(info.rspc);
				offset.y = font.GetAscent() - fallback.GetAscent();
			}
			else {
				fallback = Arial(font.GetHeight()); scalar = 0x25a1;
				if(!GetGlyphInfo(fallback,scalar).IsNormal()) scalar = ' ';
			}
			GlyphDraw replacement;
			if(!EnsureGlyph(fallback, scalar, replacement, depth + 1)) return false;
			append(replacement, offset);
		}
		return true;
	}
	const String key = AsString(font.AsInt64()) + ":" + AsString(ch);
	TextImpl& text = Text();
	int found = text.glyphs.Find(key);
	if(found >= 0) {
		const TextImpl::GlyphEntry& entry = text.glyphs[found];
		out.advance = entry.advance;
		out.offset = entry.offset;
		out.size = entry.size;
		out.drawable = entry.drawable;
		if(entry.drawable) {
			const TextImpl::AtlasPage& page = text.pages[entry.page];
			out.texture = page.texture;
			out.uv = Rectf((double)entry.pixel_rect.left / TextImpl::ATLAS_SIZE,
			               (double)entry.pixel_rect.top / TextImpl::ATLAS_SIZE,
			               (double)entry.pixel_rect.right / TextImpl::ATLAS_SIZE,
			               (double)entry.pixel_rect.bottom / TextImpl::ATLAS_SIZE);
		}
		stats.glyph_atlas_page_count = text.pages.GetCount();
		return true;
	}

	if(text.glyphs.GetCount() >= cache_limits.glyph_entries)
		return Fail("UiRenderer2D active glyph frame exceeds the configured entry budget");
	stats.glyph_cache_miss_count++;
	TextImpl::GlyphEntry entry;
	entry.advance = max(0, font.GetWidth(ch));

	const int metric_width = max(1, max(font.GetMaxWidth(), font.GetWidth(ch)));
	const int metric_height = max(1, max(font.GetLineHeight(), font.GetCy()));
	const int overhang = abs(font.GetOverhang());
	if(metric_width > TextImpl::ATLAS_SIZE || metric_height > TextImpl::ATLAS_SIZE ||
	   overhang > TextImpl::ATLAS_SIZE)
		return Fail("UiRenderer2D font metrics exceed the glyph-atlas raster bound");
	const int margin = max(4, overhang + 3);
	const int canvas_width = max(16, metric_width * 3 + margin * 2);
	const int canvas_height = max(16, metric_height * 2 + margin * 2);
	const int draw_x = margin + metric_width;
	const int draw_y = margin;

	const bool gpu_coverage = CanGpuVector();
	GpuVectorGeometry glyph_geometry;
	Image raster;
	int left = 0, top = 0, right = -1, bottom = -1;
	if(gpu_coverage) {
		GpuGlyphOutline outline;
		// Preserve the established U++ fallback/composition and outline subdivision.
		// This prepares edges only; no ImagePainter or CPU coverage image is used.
		ApproximateChar(outline, Pointf(0, 0), ch, font, 0.3);
		if(outline.failed) return Fail("GPU glyph outline command budget exceeded");
		UiDisplayOp op; op.type = UiDisplayOpType::FillPath; op.path = pick(outline.path);
		op.paint = UiPaint::Solid(Rgba8(255, 255, 255)); op.fill_rule = UiFillRule::EvenOdd;
		String detail;
		if(!PrepareGpuVectorGeometry(op, Transform2D(), glyph_geometry, detail)) return Fail(detail);
		glyph_geometry.parameters.reserved = 32; // Fixed font coverage quality, not a control knob.
		if(!glyph_geometry.bounds.IsEmpty()) {
			left = draw_x + glyph_geometry.bounds.left; top = draw_y + glyph_geometry.bounds.top;
			right = left + glyph_geometry.bounds.Width() - 1; bottom = top + glyph_geometry.bounds.Height() - 1;
		}
	}
	else {
		ImagePainter painter(canvas_width, canvas_height);
		painter.Clear(RGBAZero());
		WString one; one.Cat((wchar)ch);
		painter.DrawText(draw_x, draw_y, one, font, Color(255, 255, 255));
		raster = painter.GetResult();
		left = raster.GetWidth(); top = raster.GetHeight(); right = bottom = -1;
		for(int y = 0; y < raster.GetHeight(); ++y) {
			const RGBA *row = raster[y];
			for(int x = 0; x < raster.GetWidth(); ++x)
				if(row[x].a) {
					left = min(left, x); top = min(top, y);
					right = max(right, x); bottom = max(bottom, y);
				}
		}
		stats.cpu_glyph_raster_count++;
	}
	if(right < left || bottom < top) {
		entry.drawable = false;
		out.advance = entry.advance; // Cold whitespace has the same metrics as a cached glyph.
		TextImpl::GlyphEntry& stored = text.glyphs.Add(key);
		stored = pick(entry);
		stats.glyph_atlas_page_count = text.pages.GetCount();
		return true;
	}

	const int glyph_width = right - left + 1;
	const int glyph_height = bottom - top + 1;
	const int padded_width = glyph_width + 2 * TextImpl::ATLAS_PADDING;
	const int padded_height = glyph_height + 2 * TextImpl::ATLAS_PADDING;
	if(padded_width > TextImpl::ATLAS_SIZE || padded_height > TextImpl::ATLAS_SIZE)
		return Fail("UiRenderer2D glyph is larger than the atlas page");

	auto create_page = [&]() -> bool {
		const int64 page_bytes = (int64)TextImpl::ATLAS_SIZE * TextImpl::ATLAS_SIZE * sizeof(RGBA);
		if((text.pages.GetCount() + 1) * page_bytes > cache_limits.glyph_bytes)
			return Fail("UiRenderer2D active glyph frame exceeds the configured atlas budget");
		GpuTextureDesc desc;
		desc.size = Size(TextImpl::ATLAS_SIZE, TextImpl::ATLAS_SIZE);
		desc.format = GpuFormat::RGBA8Srgb;
		desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
		desc.label = "UiRenderer2D glyph atlas";
		GpuTextureId texture;
		GpuResult result = device->CreateTexture(desc, texture);
		if(result != GpuResult::Ok)
			return Fail("UiRenderer2D glyph atlas texture creation failed: " + DumpGpuResult(result));
		if(gpu_coverage) {
			GpuVectorGeometry clear;
			clear.bounds = Rect(desc.size); clear.parameters.width = desc.size.cx; clear.parameters.height = desc.size.cy;
			auto& zero = clear.edges.Add(); zero.x0 = zero.y0 = zero.x1 = zero.y1 = 0;
			clear.rows.SetCount(((desc.size.cy + 7) / 8) * 2, 0);
			GpuVectorStop stop {}; stop.rgba[0] = stop.rgba[1] = stop.rgba[2] = stop.rgba[3] = 1;
			clear.stops.Add(stop); clear.parameters.stop_count = 1;
			String detail;
			if(!gpu_vector_rasterizer->Render(clear, texture, Point(0, 0), false, detail)) {
				device->DestroyTexture(texture); return Fail("GPU glyph atlas clear: " + detail);
			}
		}
		TextImpl::AtlasPage& page = text.pages.Add();
		page.texture = texture;
		stats.glyph_atlas_page_count = text.pages.GetCount();
		return true;
	};

	auto try_pack = [&](TextImpl::AtlasPage& page, Point& position) -> bool {
		if(page.cursor_x + padded_width > TextImpl::ATLAS_SIZE) {
			page.cursor_x = TextImpl::ATLAS_PADDING;
			page.cursor_y += page.row_height;
			page.row_height = 0;
		}
		if(page.cursor_y + padded_height > TextImpl::ATLAS_SIZE)
			return false;
		position = Point(page.cursor_x + TextImpl::ATLAS_PADDING,
		                 page.cursor_y + TextImpl::ATLAS_PADDING);
		page.cursor_x += padded_width;
		page.row_height = max(page.row_height, padded_height);
		return true;
	};

	Point atlas_position;
	int page_index = -1;
	for(int i = 0; i < text.pages.GetCount(); ++i) {
		if(try_pack(text.pages[i], atlas_position)) {
			page_index = i;
			break;
		}
	}
	if(page_index < 0) {
		if(!create_page())
			return false;
		page_index = text.pages.GetCount() - 1;
		if(!try_pack(text.pages[page_index], atlas_position))
			return Fail("UiRenderer2D could not pack glyph into a fresh atlas page");
	}

	if(gpu_coverage) {
		String detail;
		if(!gpu_vector_rasterizer->QueueRender(pick(glyph_geometry), text.pages[page_index].texture, atlas_position, false, detail))
			return Fail("GPU glyph coverage: " + detail);
		stats.gpu_glyph_raster_count++;
	}
	else {
		Vector<RGBA> padded;
		padded.SetCount(padded_width * padded_height);
		std::memset(padded.Begin(), 0, padded.GetCount() * sizeof(RGBA));
		for(int y = 0; y < glyph_height; ++y) {
			const RGBA *source = raster[top + y] + left;
			RGBA *target = padded.Begin() + (y + TextImpl::ATLAS_PADDING) * padded_width + TextImpl::ATLAS_PADDING;
			for(int x = 0; x < glyph_width; ++x) {
				const byte alpha = source[x].a;
				target[x].r = alpha ? 255 : 0;
				target[x].g = alpha ? 255 : 0;
				target[x].b = alpha ? 255 : 0;
				target[x].a = alpha;
			}
		}

		GpuTextureWriteDesc upload;
		upload.origin = Point(atlas_position.x - TextImpl::ATLAS_PADDING,
		                      atlas_position.y - TextImpl::ATLAS_PADDING);
		upload.size = Size(padded_width, padded_height);
		upload.row_pitch = (int64)padded_width * (int)sizeof(RGBA);
		GpuResult result = device->WriteTexture(text.pages[page_index].texture, upload, padded.Begin(),
		                                        (int64)padded.GetCount() * sizeof(RGBA));
		if(result != GpuResult::Ok)
			return Fail("UiRenderer2D glyph atlas upload failed: " + DumpGpuResult(result));

	}

	entry.page = page_index;
	entry.pixel_rect = Rect(atlas_position.x, atlas_position.y,
	                        atlas_position.x + glyph_width, atlas_position.y + glyph_height);
	entry.offset = Pointf(left - draw_x, top - draw_y);
	entry.size = Size(glyph_width, glyph_height);
	entry.drawable = true;
	TextImpl::GlyphEntry& stored = text.glyphs.Add(key);
	stored = pick(entry);
	stats.glyph_atlas_upload_count++;
	stats.glyph_atlas_page_count = text.pages.GetCount();

	out.texture = text.pages[page_index].texture;
	out.uv = Rectf((double)stored.pixel_rect.left / TextImpl::ATLAS_SIZE,
	               (double)stored.pixel_rect.top / TextImpl::ATLAS_SIZE,
	               (double)stored.pixel_rect.right / TextImpl::ATLAS_SIZE,
	               (double)stored.pixel_rect.bottom / TextImpl::ATLAS_SIZE);
	out.offset = stored.offset;
	out.size = stored.size;
	out.advance = stored.advance;
	out.drawable = true;
	return true;
}

}
