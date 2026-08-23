#include "GpuWindow.h"

namespace Upp {

void GpuWindow::GpuPaint(GpuPainter&) {}

bool GpuWindow::BuildGpuFrame(Size size, UiDisplayList& list,
                              Rgba8& background, String& error)
{
	GpuPainter painter(size);
	GpuPaint(painter);
	if(WhenGpuPaint)
		WhenGpuPaint(painter);
	return painter.FinishFrame(list, background, error);
}

}
