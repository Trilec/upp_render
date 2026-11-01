#include "GpuMetrics.h"
#include <Ui/UiTheme.h>

namespace Upp {

GpuMetricsPanel::GpuMetricsPanel()
{
	const char *titles[] = { "Presented FPS", "CPU replay", "GPU paths", "Motion speed" };
	const char *tips[] = {
		"Presented frames per second, measured over one second; arc relative to the animation target.",
		"CPU elapsed replay time relative to the target frame interval. This is not GPU execution time.",
		"Paths in the last frame; arc relative to this window's observed peak, not GPU utilization.",
		"Scene motion multiplier, from 0.25x to 3.00x. Pause stops motion without changing this setting."
	};
	for(int i = 0; i < COUNT; ++i) {
		rings_[i].SetRole(UiRole::Accent);
		titles_[i].SetText(titles[i]);
		Add(rings_[i]); Add(titles_[i]); Add(details_[i]);
		rings_[i].Tip(tips[i]); titles_[i].Tip(tips[i]); details_[i].Tip(tips[i]);
	}
}

void GpuMetricsPanel::Update(const GpuPresentationStats& stats, double fps,
                            double speed, bool paused, int target_fps)
{
	const double frame_ms = 1000.0 / max(1, target_fps);
	path_peak_ = max(path_peak_, stats.renderer.gpu_path_count);
	rings_[FPS].Set(int(fps * 10), max(1, target_fps * 10)).SetText(Format("%.0f", fps));
	details_[FPS].SetText(Format("Target %d FPS", target_fps));
	rings_[REPLAY].Set(int(stats.replay_ms * 100), max(1, int(frame_ms * 100)))
	              .SetText(Format("%.1f", stats.replay_ms));
	details_[REPLAY].SetText(Format("ms / %.2f ms budget", frame_ms));
	rings_[PATHS].Set(stats.renderer.gpu_path_count, path_peak_)
	             .SetText(AsString(stats.renderer.gpu_path_count));
	details_[PATHS].SetText(Format("Peak %d paths / frame", path_peak_));
	rings_[SPEED].Set(int(speed * 100), 300).SetText(Format("%.2fx", speed));
	details_[SPEED].SetText(paused ? "Paused" : "Live / up to 3.00x");
}

void GpuMetricsPanel::Layout()
{
	UiPanel::Layout();
	const Size s = GetSize();
	const int pad = DPI(10), gap = DPI(10);
	const int cell = max(0, (s.cx - 2 * pad) / COUNT);
	const int diameter = max(0, min(DPI(72), min(s.cy - 2 * pad, cell / 2)));
	for(int i = 0; i < COUNT; ++i) {
		const int x = pad + i * cell;
		const int text_x = x + diameter + gap;
		const int text_w = max(0, cell - diameter - gap - DPI(4));
		rings_[i].SetRect(x, (s.cy - diameter) / 2, diameter, diameter);
		titles_[i].SetRect(text_x, s.cy / 2 - DPI(24), text_w, DPI(24));
		details_[i].SetRect(text_x, s.cy / 2 + DPI(2), text_w, DPI(22));
	}
}

GpuCacheChart::GpuCacheChart()
{
	title_.SetText("Retained cache payload");
	ring_.SetRole(UiRole::Accent);
	Tip("Image, vector and glyph cache payload bytes. Excludes driver allocations and total VRAM.");
	Add(ring_); Add(title_); Add(detail_);
}

void GpuCacheChart::Update(const UiRenderer2DStats& stats)
{
	// These categories share a byte unit and do not overlap. The chart presents
	// their composition, rather than inventing a total-GPU-memory percentage.
	Vector<UiChartRingSegment> segments;
	segments.Add(UiChartRingSegment(double(stats.image_cache_bytes), "Images"));
	segments.Add(UiChartRingSegment(double(stats.vector_cache_bytes), "Vectors"));
	segments.Add(UiChartRingSegment(double(stats.glyph_cache_bytes), "Glyphs"));
	ring_.SetSegments(segments);
	ring_.SetCenterText(Format("%.1f", ring_.GetDataSum() / (1024 * 1024)));
	detail_.SetText(Format("MiB  /  %d images", stats.image_cache_entry_count));
}

void GpuCacheChart::Layout()
{
	UiPanel::Layout();
	const Size s = GetSize();
	const int pad = DPI(10), diameter = max(0, min(DPI(76), s.cy - 2 * pad));
	const int x = pad + diameter + DPI(10), width = max(0, s.cx - x - pad);
	ring_.SetRect(pad, (s.cy - diameter) / 2, diameter, diameter);
	title_.SetRect(x, s.cy / 2 - DPI(24), width, DPI(24));
	detail_.SetRect(x, s.cy / 2 + DPI(2), width, DPI(22));
}

} // namespace Upp
