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

// Records Painter path intent without creating CPU coverage pixels. Unsupported
// masks/patterns fail atomically; out is valid only on success.
bool RecordUiPainter(Size size, const Function<void(Painter&)>& paint,
                     UiDisplayList& out, String& error);
bool RecordUiSvg(const UiDisplayOp& op, UiDisplayList& out, String& error);
// Replays a validated neutral list to another canvas without materialising pixels.
bool ReplayUiPath(Painter& painter, const UiPath& path, String& error);
void ReplayUiDisplayOp(const UiDisplayOp& op, UiCanvas& target);
void ReplayUiDisplayList(const UiDisplayList& list, UiCanvas& target);

}
