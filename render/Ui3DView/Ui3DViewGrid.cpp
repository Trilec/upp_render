#include "Ui3DViewContent.h"
#include <algorithm>
#include <cmath>

namespace Upp {
Scene3DVec3 Ui3DViewGridLayout::ToWorld(double u, double v) const
{
	switch(plane) {
	case Ui3DGridPlane::XY: return Scene3DVec3(u, v, 0);
	case Ui3DGridPlane::YZ: return Scene3DVec3(0, u, v);
	case Ui3DGridPlane::XZ: return Scene3DVec3(u, 0, v);
	}
	return Scene3DVec3();
}

bool Ui3DViewGridLayout::operator==(const Ui3DViewGridLayout& other) const
{
	return plane == other.plane && spacing == other.spacing && center_u == other.center_u &&
	       center_v == other.center_v && extent == other.extent;
}

bool ComputeUi3DViewGridLayout(const Scene3DCamera& camera, const Ui3DViewOptions& options,
                              Size viewport, Ui3DViewGridLayout& out)
{
	out = Ui3DViewGridLayout();
	String error;
	if(viewport.cx <= 0 || viewport.cy <= 0 || !camera.Validate(error) ||
	   options.grid_extent < 1 || options.grid_extent > 100 || !std::isfinite(options.grid_spacing) ||
	   options.grid_spacing < 0.001 || options.grid_spacing > 10000 ||
	   (options.grid_plane != Ui3DGridPlane::XY && options.grid_plane != Ui3DGridPlane::YZ &&
	    options.grid_plane != Ui3DGridPlane::XZ)) return false;
	Ui3DViewGridLayout grid;
	grid.plane = options.grid_plane; grid.extent = options.grid_extent; grid.spacing = options.grid_spacing;
	if(options.adaptive_grid) {
		const double overscan = camera.gate_aspect > 0 ?
		    std::max(1.0, camera.gate_aspect * viewport.cy / viewport.cx) : 1.0;
		const double height = (camera.orthographic ? camera.ortho_height :
		                       2 * camera.distance * std::tan(camera.fov_y * 0.5)) * overscan;
		const double width = height * ((double)viewport.cx / viewport.cy);
		// Maintain readable cells and enough bounded coverage after a half-cell snap.
		const double requested = std::max(height * 48.0 / viewport.cy,
		                                  std::max(width, height) / (2 * grid.extent - 1));
		const double decade = std::pow(10.0, std::floor(std::log10(requested / options.grid_spacing)));
		const double fraction = requested / (options.grid_spacing * decade);
		const double nice = fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10;
		grid.spacing = std::max(1e-9, std::min(1e10, options.grid_spacing * decade * nice));
		double u = camera.target.x, v = camera.target.z;
		if(grid.plane == Ui3DGridPlane::XY) v = camera.target.y;
		if(grid.plane == Ui3DGridPlane::YZ) u = camera.target.y;
		grid.center_u = std::round(u / grid.spacing) * grid.spacing;
		grid.center_v = std::round(v / grid.spacing) * grid.spacing;
	}
	out = grid;
	return true;
}

}
