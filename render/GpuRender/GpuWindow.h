#pragma once

#include <GpuTopWindow/GpuTopWindow.h>
#include <RenderCanvas/GpuPainter.h>

namespace Upp {

// Full top-level GPU drawing surface for applications that do not want a U++
// control tree inside the client area. For a GPU-composited U++ interface use
// GpuTopWindow; for an embedded accelerated rectangle use GpuCtrl.
class GpuWindow : public GpuTopWindow {
public:
	Function<void(GpuPainter&)> WhenGpuPaint;

protected:
	virtual void GpuPaint(GpuPainter& painter);
	bool BuildGpuFrame(Size size, UiDisplayList& list,
	                   Rgba8& background, String& error) override;
};

}
