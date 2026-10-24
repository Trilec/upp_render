#pragma once

namespace Upp {

// Installs the process-wide U++ state hook that gives owned transient native
// top-level windows (popups and owned resizable/maximizable dialogs) GPU
// presentation when their owner is a ready GpuTopWindow.
void EnsureGpuTransientWindowSupport();

}
