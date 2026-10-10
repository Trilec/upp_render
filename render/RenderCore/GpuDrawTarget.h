#pragma once

#include <Draw/Draw.h>

namespace Upp {

// Optional resolved-shape capability for Draw receivers. Ui consumes this only
// in GPUUI builds; there is no Vulkan, control or theme dependency here.
// Return false before emitting anything when a shape is unsupported.
class GpuDrawTarget {
public:
	virtual ~GpuDrawTarget() {}
	virtual bool DrawRoundedFaceFrame(const Rect& bounds, int radius,
	                                 int frame_width, Color face, Color frame) = 0;
};

}
