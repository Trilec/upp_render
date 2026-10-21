#include "GpuTextLayout.h"
#include <cmath>

namespace Upp {
int GpuShapedText::GetGlyphCount() const
{
	int count=0;
	for(const auto& run:runs) {
		if(run.glyphs.GetCount()>65536-count) return 65537;
		count+=run.glyphs.GetCount();
	}
	return count;
}
int64 GpuShapedText::GetPayloadBytes() const
{
	int64 bytes=(int64)runs.GetCount()*sizeof(GpuGlyphRunSnapshot) +
	            (int64)decorations.GetCount()*sizeof(GpuTextDecoration);
	for(const auto& run:runs) bytes+=(int64)run.glyphs.GetCount()*sizeof(GpuPositionedGlyph)+
		(run.face?run.face->GetIdentity().GetCount():0)+(int64)run.bitmap.GetLength()*sizeof(RGBA);
	return bytes;
}
bool GpuShapedText::IsValid(String *error) const
{
	auto fail=[&](const char *why) { if(error) *error=why; return false; };
	if(runs.GetCount()>1024 || decorations.GetCount()>4096 || GetGlyphCount()>65536 ||
	   GetPayloadBytes()>4*1024*1024) return fail("shaped text exceeds the frame payload budget");
	auto finite=[](double value) { return std::isfinite(value) && fabs(value)<=1e7; };
	for(const auto& run:runs) {
		if(!run.face || run.face->GetIdentity().IsEmpty() || run.face->GetIdentity().GetCount()>8192)
			return fail("shaped glyph run has no bounded font-face identity");
		if(!std::isfinite(run.em_size) || run.em_size<=0 || run.em_size>512)
			return fail("shaped glyph size is outside the atlas contract");
		if(!run.bitmap.IsEmpty()) {
			const Rectf& b=run.bitmap_bounds;
			if(run.glyphs.GetCount()!=1 || run.bitmap.GetWidth()>1024 || run.bitmap.GetHeight()>1024 ||
			   !finite(b.left)||!finite(b.top)||!finite(b.right)||!finite(b.bottom)||b.IsEmpty())
				return fail("invalid or oversized embedded glyph image");
		}
		for(const auto& glyph:run.glyphs)
			if(glyph.glyph>65535 || !finite(glyph.origin.x) || !finite(glyph.origin.y))
				return fail("invalid shaped glyph ID or placement");
	}
	for(const auto& decoration:decorations) {
		const Rectf& r=decoration.rect;
		if(!finite(r.left)||!finite(r.top)||!finite(r.right)||!finite(r.bottom)||
		   r.Width()<=0||r.Height()<=0) return fail("invalid shaped text decoration");
	}
	if(error) error->Clear();
	return true;
}
bool GpuShapedText::operator==(const GpuShapedText& b) const
{
	if(runs.GetCount()!=b.runs.GetCount() || decorations!=b.decorations) return false;
	for(int i=0;i<runs.GetCount();++i) {
		const auto& a=runs[i]; const auto& c=b.runs[i];
		if((bool)a.face!=(bool)c.face || a.em_size!=c.em_size || a.ink!=c.ink || a.opacity!=c.opacity || a.glyphs!=c.glyphs ||
		   a.bitmap.GetSerialId()!=c.bitmap.GetSerialId() || a.bitmap_bounds!=c.bitmap_bounds ||
		   (a.face && a.face->GetIdentity()!=c.face->GetIdentity())) return false;
	}
	return true;
}
}
