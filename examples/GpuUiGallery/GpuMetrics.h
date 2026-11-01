#ifndef _GpuUiGallery_GpuMetrics_h_
#define _GpuUiGallery_GpuMetrics_h_

#include <GpuRender/RenderPresentation.h>
#include <Ui/UiPanel.h>
#include <Ui/UiLabel.h>
#include <Ui/UiProgressRing.h>
#include <Ui/UiChartRing.h>

namespace Upp {

// Demo composition only: the existing Ui controls own painting and theme changes.
// Readings arrive from the gallery's existing GUI-thread status timer; no new
// animation thread, timer, renderer API or copy of presentation state is needed.
class GpuMetricsPanel : public UiPanel {
public:
	GpuMetricsPanel();
	void Update(const GpuPresentationStats& stats, double fps, double speed,
	            bool paused, int target_fps);
	void Layout() override;

private:
	enum { FPS, REPLAY, PATHS, SPEED, COUNT };
	UiProgressRing rings_[COUNT];
	UiLabel titles_[COUNT], details_[COUNT];
	int path_peak_ = 1; // Visual scale only; never a hardware-utilization percentage.
};

class GpuCacheChart : public UiPanel {
public:
	GpuCacheChart();
	void Update(const UiRenderer2DStats& stats);
	void Layout() override;

private:
	UiChartRing ring_;
	UiLabel title_, detail_;
};

} // namespace Upp

#endif
