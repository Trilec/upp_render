#include "RenderGpu2D.h"

#include <Painter/Painter.h>
#include <cstring>

namespace Upp {

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

bool UiRenderer2D::EnsureGlyph(Font font, int ch, GlyphDraw& out)
{
	out = GlyphDraw();
	if(!device)
		return Fail("UiRenderer2D glyph cache has no device");
	if((device->GetAdapterInfo().capability_flags & GpuCapability_Textures) == 0)
		return Fail("UiRenderer2D DrawText requires texture support");

	font.Underline(false).Strikeout(false);
	font.RealizeStd();
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

	stats.glyph_cache_miss_count++;
	TextImpl::GlyphEntry entry;
	entry.advance = max(0, font.GetWidth(ch));

	const int metric_width = max(1, max(font.GetMaxWidth(), font.GetWidth(ch)));
	const int metric_height = max(1, max(font.GetLineHeight(), font.GetCy()));
	const int margin = max(4, abs(font.GetOverhang()) + 3);
	const int canvas_width = max(16, metric_width * 3 + margin * 2);
	const int canvas_height = max(16, metric_height * 2 + margin * 2);
	const int draw_x = margin + metric_width;
	const int draw_y = margin;

	ImagePainter painter(canvas_width, canvas_height);
	painter.Clear(RGBAZero());
	WString one;
	one.Cat((wchar)ch);
	painter.DrawText(draw_x, draw_y, one, font, Color(255, 255, 255));
	Image raster = painter.GetResult();

	int left = raster.GetWidth();
	int top = raster.GetHeight();
	int right = -1;
	int bottom = -1;
	for(int y = 0; y < raster.GetHeight(); ++y) {
		const RGBA *row = raster[y];
		for(int x = 0; x < raster.GetWidth(); ++x) {
			if(row[x].a) {
				left = min(left, x);
				top = min(top, y);
				right = max(right, x);
				bottom = max(bottom, y);
			}
		}
	}

	if(right < left || bottom < top) {
		entry.drawable = false;
		text.glyphs.Add(key, pick(entry));
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
		GpuTextureDesc desc;
		desc.size = Size(TextImpl::ATLAS_SIZE, TextImpl::ATLAS_SIZE);
		desc.format = GpuFormat::RGBA8Srgb;
		desc.usage = GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst;
		desc.label = "UiRenderer2D glyph atlas";
		GpuTextureId texture;
		GpuResult result = device->CreateTexture(desc, texture);
		if(result != GpuResult::Ok)
			return Fail("UiRenderer2D glyph atlas texture creation failed: " + DumpGpuResult(result));
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
