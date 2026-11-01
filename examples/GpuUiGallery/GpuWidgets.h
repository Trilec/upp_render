#ifndef _GpuUiGallery_GpuWidgets_h_
#define _GpuUiGallery_GpuWidgets_h_
#include <Ui/Ui.h>

namespace Upp {

// A small, shared control composition for the workspace and Controls tab.
// Models belong to these native controls; this is not a parallel gallery model.
class GpuWidgets : public Ctrl {
public:
	GpuWidgets();
	void Layout() override;
	UiTree tree;
	UiTable table;

private:
	static Image MakePreview();
	UiLabel headings_[3], image_, caption_;
	UiCheckBox image_enabled_;
	UiProgressBar progress_;
};
}
#endif
