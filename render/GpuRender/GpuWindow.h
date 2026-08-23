#pragma once

#include "GpuTopWindow.h"
#include <RenderCanvas/GpuPainter.h>

namespace Upp {

class GpuWindow : public GpuTopWindow {
public:
	Function<void(GpuPainter&)> WhenGpuPaint;
protected:
	virtual void GpuPaint(GpuPainter& painter);
	bool BuildGpuFrame(Size size, UiDisplayList& list, Rgba8& background, String& error) override;
};

}
