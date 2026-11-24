#ifndef _Ui3DView_Content_h_
#define _Ui3DView_Content_h_

#include "Ui3DView.h"

namespace Upp {

// Internal, pure grid policy. Reusable tests can verify geometry without a GPU.
// u/v are world coordinates in XY, YZ or XZ; the omitted coordinate is zero.
struct Ui3DViewGridLayout {
	Ui3DGridPlane plane = Ui3DGridPlane::XZ;
	double spacing = 1, center_u = 0, center_v = 0;
	int extent = 10;
	Scene3DVec3 ToWorld(double u, double v) const;
	bool operator==(const Ui3DViewGridLayout& other) const;
};

// Clears out on failure. Adaptive spacing follows 1/2/5 * base-spacing decades;
// center snaps to cells and extent bounds geometry. Fixed mode stays origin-centred.
bool ComputeUi3DViewGridLayout(const Scene3DCamera& camera, const Ui3DViewOptions& options,
                              Size viewport, Ui3DViewGridLayout& out);

// Pure bounded guide layout shared by the GPU renderer and native qualification.
struct Ui3DViewGateLayout {
	Rect gate, action_safe, title_safe;
};
bool ComputeUi3DViewGateLayout(const Scene3DCamera&, const Ui3DViewOptions&,
                              Size viewport, Ui3DViewGateLayout&);

struct Ui3DViewState {
	std::shared_ptr<const Scene3DSnapshot> scene;
	std::shared_ptr<const Scene3DSnapshot> tool_preview_scene;
	std::shared_ptr<const Scene3DSnapshot> GetRenderScene() const {
		return tool_preview_scene ? tool_preview_scene : scene;
	}
	Scene3DCamera camera;
	Ui3DViewOptions options;
	std::vector<String> selection;
	Ui3DViewStats stats;
	String error;
	uint64 decoration_revision = 1;
	uint64 selection_revision = 1;
};

std::unique_ptr<GpuSurfaceContent> CreateUi3DViewContent(
	const std::shared_ptr<Ui3DViewState>& state);

}
#endif
