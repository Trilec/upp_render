#pragma once

// Public façade for ordinary upp_render users.
// Add package GpuRender and include this header. Lower Render* packages are
// implementation/advanced layers unless a backend or renderer is being built.

#include <RenderCanvas/GpuPainter.h>
#include <GpuCtrl/GpuCtrl.h>
#include <GpuTopWindow/GpuTopWindow.h>
#include <GpuRender/GpuWindow.h>
