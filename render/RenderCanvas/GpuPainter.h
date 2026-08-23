#pragma once

#include <RenderCanvas/RenderCanvas.h>

namespace Upp {

// Application-facing immediate-style painter for GPU surfaces.
// Calls are recorded into the existing backend-neutral immutable display list;
// backend details remain below this boundary. Clear() configures the frame
// clear colour rather than emitting a full-surface rectangle.
class GpuPainter : public UiDisplayListBuilder {
public:
	GpuPainter() = default;

	GpuPainter& Clear(Rgba8 color) {
		background = color;
		return *this;
	}
	GpuPainter& Clear(Color color) { return Clear(Rgba8::FromColor(color)); }

	Rgba8 GetBackground() const { return background; }

	using UiDisplayListBuilder::FillRect;
	using UiDisplayListBuilder::StrokeRect;
	using UiDisplayListBuilder::FillRoundedRect;
	using UiDisplayListBuilder::DrawText;

	void FillRect(const Rectf& rect, Color color) {
		UiDisplayListBuilder::FillRect(rect, Rgba8::FromColor(color));
	}
	void StrokeRect(const Rectf& rect, double width, Color color) {
		UiDisplayListBuilder::StrokeRect(rect, width, Rgba8::FromColor(color));
	}
	void FillRoundedRect(const RoundedRect& rect, Color color) {
		UiDisplayListBuilder::FillRoundedRect(rect, Rgba8::FromColor(color));
	}
	void DrawText(const Pointf& point, const WString& text, Font font, Color color) {
		UiDisplayListBuilder::DrawText(point, text, font, Rgba8::FromColor(color));
	}
	void DrawText(const Pointf& point, const String& text, Font font, Color color) {
		DrawText(point, text.ToWString(), font, color);
	}

	bool FinishFrame(UiDisplayList& out, Rgba8& out_background, String& error) {
		out_background = background;
		if(!Finish(out)) {
			error = GetError();
			return false;
		}
		if(!out.IsValid()) {
			error = out.GetError();
			return false;
		}
		error.Clear();
		return true;
	}

private:
	Rgba8 background = Rgba8(0, 0, 0, 255);
};

}
