#include <CtrlLib/CtrlLib.h>
#include <RenderPlatformWin32/RenderPlatformWin32.h>
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>

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

static bool TestFrameClock()
{
	Win32GpuFrameClock clock;
	bool ok = Check(!clock.Start(nullptr), "frame clock rejects an invalid window");
	TopWindow window;
	window.SetRect(0, 0, 160, 100);
	window.Open();
	ok &= Check(clock.Start(window.GetHWND()) && clock.IsActive(), "precise frame clock starts");
	Sleep(70); // deliberately stall consumption to exercise the one-message bound
	MSG message {};
	int queued = 0;
	WPARAM old_generation = 0;
	while(PeekMessageW(&message, window.GetHWND(), clock.Message(), clock.Message(), PM_REMOVE)) {
		queued++;
		old_generation = message.wParam;
	}
	ok &= Check(queued == 1, "a stalled UI retains one frame wake, not an unbounded queue");
	clock.Stop();
	ok &= Check(!clock.IsActive() && !clock.Consume(old_generation), "stopped clock rejects queued wakes");
	ok &= Check(clock.Start(window.GetHWND()), "clock restarts on the surviving window");
	ok &= Check(!clock.Consume(old_generation), "restarted clock rejects the preceding generation");
	Sleep(40);
	ok &= Check(PeekMessageW(&message, window.GetHWND(), clock.Message(), clock.Message(), PM_REMOVE) &&
	            clock.Consume(message.wParam), "new generation produces a consumable wake");
	clock.Stop();
	window.Close();
	ok &= Check(!clock.Start(window.GetHWND()), "closed HWND cannot restart the clock");
	return ok;
}

GUI_APP_MAIN
{
	bool ok = true;
	ok &= TestUnopenedWindow();
	ok &= TestOpenedWindow();
	ok &= TestFrameClock();
	if(ok) {
		Cout() << "RenderPlatformWin32Test passed" << EOL;
		return;
	}
	SetExitCode(1);
}
