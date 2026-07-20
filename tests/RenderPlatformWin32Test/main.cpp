#include <CtrlLib/CtrlLib.h>
#include <RenderPlatformWin32/RenderPlatformWin32.h>

using namespace Upp;

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
}

static bool TestUnopenedWindow()
{
	TopWindow win;
	GpuNativeWindowDesc desc;
	String error;
	return Check(GetGpuNativeWindowDesc(win, desc, error) == GpuResult::InvalidState, "unopened window should be rejected");
}

static bool TestOpenedWindow()
{
	TopWindow win;
	win.Title("RenderPlatformWin32Test").SetRect(0, 0, 320, 240);
	win.Open();
	if(!Check(win.IsOpen(), "window should be open")) return false;
	Ctrl::ProcessEvents();

	GpuNativeWindowDesc desc;
	String error;
	if(!Check(GetGpuNativeWindowDesc(win, desc, error) == GpuResult::Ok, "opened window should produce descriptor")) return false;
	if(!Check(desc.kind == GpuNativeWindowKind::Win32, "descriptor kind should be Win32")) return false;
	if(!Check(desc.handle != 0, "descriptor handle should be set")) return false;
	if(!Check(desc.IsValid(), "descriptor should be valid")) return false;
	if(!Check(DumpGpuNativeWindowDesc(desc).Find("handle=set") >= 0, "dump should redact the handle")) return false;
	if(!Check(DumpGpuNativeWindowDesc(desc).Find("0x") < 0, "dump should not print numeric handle")) return false;

	win.Close();
	return Check(GetGpuNativeWindowDesc(win, desc, error) == GpuResult::InvalidState, "closed window should be rejected");
}

GUI_APP_MAIN
{
	bool ok = true;
	ok &= TestUnopenedWindow();
	ok &= TestOpenedWindow();
	if(ok) {
		Cout() << "RenderPlatformWin32Test passed" << EOL;
		return;
	}
	SetExitCode(1);
}
