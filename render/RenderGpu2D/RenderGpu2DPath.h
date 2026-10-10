#pragma once

#include <RenderCanvas/RenderCanvas.h>

namespace Upp {

// Private preparation seam. Points are final device pixels, not CPU pixels.
// Unsupported topology/style uses the existing bounded Painter reference path.
struct GpuPathVertex : Moveable<GpuPathVertex> {
	Pointf point;
	double coverage = 1;
};
struct GpuPathMesh : Moveable<GpuPathMesh> {
	Vector<GpuPathVertex> triangles;
};
bool PrepareGpuConvexPath(const UiDisplayOp& op, const Transform2D& transform,
                          GpuPathMesh& out);

}
