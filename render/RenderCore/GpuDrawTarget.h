#pragma once

#include <Draw/Draw.h>
#include "GpuTextLayout.h"

namespace Upp {

class Painter;

// Optional Painter paint source, resolved at drawing time without CPU pixels.
class GpuPainterEffects {
public:
	virtual ~GpuPainterEffects() {}
	virtual bool DrawShapedText(Pointf origin,const std::shared_ptr<const GpuShapedText>& text,Color ink) { return false; }
	virtual void BlurLayer(int radius = 1) = 0;
	virtual void PaintAngular(Pointf centre, double start, double sweep,
	                          Color first, Color last, double stroke_width = -1) = 0;
};

// Optional resolved-shape capability for Draw receivers. Ui consumes this only
// in GPUUI builds; there is no Vulkan, control or theme dependency here.
// Return false before emitting anything when a shape is unsupported.
class GpuDrawTarget {
public:
	virtual ~GpuDrawTarget() {}
	// Immutable positioned glyphs from the shared layout service. Return false
	// before emitting when unsupported. Native font resources are owned by text.
	virtual bool DrawShapedText(Pointf origin, const std::shared_ptr<const GpuShapedText>& text,
	                            Color ink) { return false; }
	// Local Painter coordinates inside bounds. Return false before emitting if
	// unsupported; the caller may use its existing software receiver.
	virtual bool DrawVectorArtwork(const Rect& bounds,
	                               const Function<void(Painter&)>& paint) { return false; }
	virtual bool DrawRoundedFaceFrame(const Rect& bounds, int radius,
	                                 int frame_width, Color face, Color frame) = 0;
};

}
