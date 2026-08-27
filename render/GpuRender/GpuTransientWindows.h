#pragma once

namespace Upp {

// Installs the process-wide U++ state hook that gives owned transient native
// top-level windows (DropList/menu/tool-tip style WS_POPUP windows) GPU
// presentation when their owner is a ready GpuTopWindow.
void EnsureGpuTransientWindowSupport();

}
