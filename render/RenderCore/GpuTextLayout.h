#pragma once
#include <Draw/Draw.h>
#include <memory>

namespace Upp {

// A font face is immutable and safe to retain through a queued render frame.
// Identity includes the actual fallback face, face index and simulations.
// The implementation owns its native resources; no platform types cross this API.
class GpuGlyphFaceSource {
public:
	virtual ~GpuGlyphFaceSource() {}
	virtual const String& GetIdentity() const = 0;
	// Baseline-relative outline; em_size is in device pixels. No coverage pixels.
	virtual bool GetOutline(uint32 glyph, double em_size, FontGlyphConsumer& out,
	                        String& error) const = 0;
};

struct GpuPositionedGlyph : Moveable<GpuPositionedGlyph> {
	uint32 glyph = 0;
	Pointf origin = Pointf(0,0);
	bool operator==(const GpuPositionedGlyph& b) const { return glyph==b.glyph && origin==b.origin; }
};

struct GpuGlyphRunSnapshot {
	std::shared_ptr<const GpuGlyphFaceSource> face;
	double em_size = 12;
	Vector<GpuPositionedGlyph> glyphs;
	Color ink = Null; // Null inherits the drawing caller's ink.
	byte opacity = 255;
	// Embedded font pixels are immutable source assets, not CPU glyph coverage.
	// Bitmap runs contain exactly one glyph and retain its native face identity.
	Image bitmap;
	Rectf bitmap_bounds = Rectf(0,0,0,0);

};

struct GpuTextDecoration : Moveable<GpuTextDecoration> {
	Rectf rect = Rectf(0,0,0,0);
	Color ink = Null;
	GpuTextDecoration() = default;
	GpuTextDecoration(Rectf r,Color c=Null):rect(r),ink(c) {}
	bool operator==(const GpuTextDecoration& b) const { return rect==b.rect && ink==b.ink; }
};

// Derived immutable rendering snapshot, not a second editable text model.
// Layout/caret/selection remain the text-layout service's responsibility.
// Runs use final local pixel positions; the ordinary canvas transform applies.
struct GpuShapedText {
	Array<GpuGlyphRunSnapshot> runs;
	Vector<GpuTextDecoration> decorations;
	bool IsValid(String *error = nullptr) const;
	int GetGlyphCount() const;
	int64 GetPayloadBytes() const;
	bool operator==(const GpuShapedText& other) const;
};
}
