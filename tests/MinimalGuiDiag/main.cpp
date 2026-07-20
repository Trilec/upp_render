#include <CtrlLib/CtrlLib.h>

using namespace Upp;

GUI_APP_MAIN
{
	Cout() << "entered MinimalGuiDiag GUI_APP_MAIN" << EOL;
	TopWindow win;
	win.Title("Minimal U++ GUI startup test");
	win.SetRect(0, 0, 320, 200);
	PostCallback([&] { win.Close(); });
	win.Run();
	Cout() << "leaving MinimalGuiDiag GUI_APP_MAIN" << EOL;
}
