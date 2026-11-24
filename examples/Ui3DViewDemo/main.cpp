#include "Ui3DViewDemo.h"

using namespace Upp;

static void ViewerDemoPump(int count)
{
	const int start = msecs();
	const int duration = max(0, count) * 10;
	for(;;) {
		Ctrl::ProcessEvents();
		const int elapsed = msecs(start);
		if(elapsed >= duration) break;
		Ctrl::GuiSleep(min(10, int(duration - elapsed)));
	}
	Ctrl::ProcessEvents();
}

GUI_APP_MAIN
{
	Ui3DViewDemo window;
	const auto& args = CommandLine();
	bool capture = false, smoke = false, qualify = false, compact_matrix = false;
	bool qualify_overlays = false;
	String overlay_output = AppendFileName(GetCurrentDirectory(), "build/Ui3DViewOverlay");
	for(const String& arg : args) {
		if(arg == "--capture") capture = true;
		if(arg == "--capture-matrix") capture = compact_matrix = true;
		if(arg == "--smoke") smoke = true;
		if(arg == "--qualify") qualify = true;
		if(arg == "--qualify-overlays") qualify_overlays = true;
		if(arg.StartsWith("--overlay-output=")) overlay_output = arg.Mid(17);
		if(arg == "--validation") window.SetValidation();
	}
	if(!capture && !smoke && !qualify && !qualify_overlays) { window.Run(); return; }
#ifndef GUI_WIN
	if(capture || qualify || qualify_overlays) {
		Cout() << "Native capture and input qualification require Windows. Use --smoke on macOS." << EOL;
		SetExitCode(2);
		return;
	}
#endif
	window.Open();
	// Only finite qualification/capture uses topmost, and Close removes it. Do
	// not dismiss or manipulate unrelated applications that may be above us.
	window.TopMost();
	window.SetForeground();
	ViewerDemoPump(70);
	String error, report;
	auto wait_for_frame = [&] {
		const int start = msecs();
		while(!window.CheckReady(error)) {
			if(msecs(start) >= 10000) return false;
			ViewerDemoPump(2);
		}
		return true;
	};
	bool ok = smoke ? wait_for_frame() : window.CheckReady(error);
	if(smoke && ok && !qualify && !qualify_overlays && !capture) {
		window.SetQuad(true);
		ok = wait_for_frame();
	}
#ifdef GUI_WIN
	if(qualify) ok = window.QualifyWorkspace(report) && ok;
	if(qualify_overlays) ok = window.QualifyOverlays(report, overlay_output) && ok;
	if(capture) {
		const Size sizes[] = { Size(1440,900), Size(1222,773), Size(900,650), Size(720,600) };
		for(int size = 0; size < (compact_matrix ? 4 : 1); ++size) {
			window.SetRect(20, 20, DPI(sizes[size].cx), DPI(sizes[size].cy));
			for(bool light : { false, true }) for(bool quad : { false, true }) {
				window.SetLight(light); window.SetQuad(quad);
				ViewerDemoPump(35);
				String frame_error;
				bool ready = window.CheckReady(frame_error);
				const String name = Format("Ui3DView-%s-%s-%dx%d.png", light ? "light" : "dark",
				                           quad ? "quad" : "single", sizes[size].cx, sizes[size].cy);
				const bool saved = ready && window.Capture(GetExeDirFile(name), frame_error);
				report << "capture=" << name << " " << (saved ? "PASS" : "FAIL") << " " << frame_error << "\n";
				ok = saved && ok;
			}
		}
	}
#endif
	for(int i = 0; i < 4; ++i) {
		const auto stats = window.Pane(i).View().GetViewStats();
		report << "pane=" << i << " rendered_frames=" << stats.rendered_frames << " mesh_uploads=" << stats.mesh_uploads
		       << " presented_frames=" << window.Pane(i).View().GetGpuStats().presented_frames
		       << " resident_meshes=" << stats.resident_meshes << " depth_rebuilds=" << stats.depth_rebuilds << "\n";
	}
	report << "Ui3DViewDemo " << (ok ? "PASS" : "FAIL") << "\nerror=" << error << "\n";
	window.Close(); ViewerDemoPump(4);
#ifdef GUI_WIN
	const String report_file = qualify_overlays ? AppendFileName(overlay_output, "Ui3DViewDemo-overlays.txt")
	                                           : GetExeDirFile("Ui3DViewDemo-smoke.txt");
	if(qualify_overlays) RealizeDirectory(overlay_output);
	if(!SaveFile(report_file, report)) { Cout() << "FAIL: cannot save " << report_file << EOL; ok = false; }
#endif
	Cout() << report;
	if(!ok) SetExitCode(1);
}
