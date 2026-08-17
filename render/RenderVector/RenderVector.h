#pragma once

#include <RenderCanvas/RenderCanvas.h>
#include <Painter/Painter.h>

namespace Upp {

// U++ Painter is the semantic authority for Stage-5 vector rasterization.
// scale/offset are used by the GPU cache to rasterize local vector intent at
// device-appropriate resolution without changing the neutral display model.
bool ReplayUiVectorOp(Painter& painter, const UiDisplayOp& op, String& error,
                      double scale = 1.0, Pointf offset = Pointf(0, 0));

// Rasterizes one FillPath/StrokePath/DrawSvg operation into an antialiased U++
// Image and returns the local-coordinate rectangle represented by the image.
bool RasterizeUiVectorOp(const UiDisplayOp& op, double scale, Image& out,
                         Rectf& local_rect, String& error);

}
