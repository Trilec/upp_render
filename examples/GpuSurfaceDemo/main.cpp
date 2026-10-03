#include "GpuSurfaceDemo.h"

using namespace Upp;

GUI_APP_MAIN
{
    GpuSurfaceDemo win;
    const Vector<String>& args = CommandLine();
    if(args.GetCount() && args[0] == "--self-test") {
        SetExitCode(win.RunSmoke() ? 0 : 1);
        return;
    }
    if(args.GetCount() == 2 && args[0] == "--write-usage") {
        SetExitCode(SaveFile(args[1], win.GenerateUsage()) ? 0 : 1);
        return;
    }
    win.Run();
}
