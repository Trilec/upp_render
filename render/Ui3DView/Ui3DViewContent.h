#ifndef _Ui3DView_Content_h_
#define _Ui3DView_Content_h_

#include "Ui3DView.h"

namespace Upp {

struct Ui3DViewState {
	std::shared_ptr<const Scene3DSnapshot> scene;
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
