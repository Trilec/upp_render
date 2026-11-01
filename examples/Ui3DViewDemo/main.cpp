#include "Ui3DViewDemo.h"

using namespace Upp;

static void Pump(int count)
{
	for(int i = 0; i < count; ++i) { Ctrl::ProcessEvents(); Ctrl::GuiSleep(10); }
}

GUI_APP_MAIN
{
	Ui3DViewDemo window;
	const auto& args = CommandLine();
	bool capture = false, smoke = false;
	for(const String& arg : args) {
		if(arg == "--capture") capture = true;
		if(arg == "--smoke") smoke = true;
		if(arg == "--validation") window.view.SetValidation();
	}
	if(!capture && !smoke) { window.Run(); return; }
	window.Open();
	Pump(80);
	String error;
	bool ok = window.CheckReady(error);
	if(ok && capture) {
		auto capture_frame = [&](const char *name, uint64 previous_frame) {
			String frame_error;
			if(!window.CheckReady(frame_error)) { error = frame_error; return false; }
			if(window.view.GetViewStats().rendered_frames <= previous_frame) {
				error = "requested view mode did not render a new frame"; return false;
			}
			if(!window.Capture(GetExeDirFile(name))) {
				error = String("could not capture foreground client: ") + name; return false;
			}
			return true;
		};
		ok = capture_frame("Ui3DView-dark.png", 0);
		uint64 previous = window.view.GetViewStats().rendered_frames;
		window.SetLight(true); Pump(30);
		ok = capture_frame("Ui3DView-light.png", previous) && ok;
		previous = window.view.GetViewStats().rendered_frames;
		window.SetLight(false); window.SetWireframe(true); Pump(30);
		ok = capture_frame("Ui3DView-wireframe.png", previous) && ok;
		previous = window.view.GetViewStats().rendered_frames;
		window.SetWireframe(false); window.SetOrthographic(true); Pump(30);
		ok = capture_frame("Ui3DView-orthographic.png", previous) && ok;
	}
	const auto stats = window.view.GetViewStats();
	String report;
	report << "Ui3DViewDemo " << (ok ? "PASS" : "FAIL") << "\n"
	       << "rendered_frames=" << stats.rendered_frames
	       << " mesh_uploads=" << stats.mesh_uploads
	       << " resident_meshes=" << stats.resident_meshes
	       << " depth_rebuilds=" << stats.depth_rebuilds << "\n"
	       << "error=" << error << "\n";
	window.Close();
	Pump(4);
	SaveFile(GetExeDirFile("Ui3DViewDemo-smoke.txt"), report);
	Cout() << report;
	if(!ok) SetExitCode(1);
}
