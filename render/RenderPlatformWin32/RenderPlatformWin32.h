#pragma once

#include <RenderRhi/RenderRhi.h>
#include <CtrlCore/CtrlCore.h>

namespace Upp {

GpuResult GetGpuNativeWindowDesc(const TopWindow& window, GpuNativeWindowDesc& out, String& error);

}
