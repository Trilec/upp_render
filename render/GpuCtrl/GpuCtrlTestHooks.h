#pragma once

#include <RenderCanvas/RenderCanvas.h>

namespace Upp {
namespace GpuCtrlTestHooks {

// Builds the same immutable Stage-4 scene that the live GpuCtrl presents.
bool BuildDefaultFrame(Size size, UiDisplayList& list, Rgba8& background, String& error);

} // namespace GpuCtrlTestHooks
} // namespace Upp
