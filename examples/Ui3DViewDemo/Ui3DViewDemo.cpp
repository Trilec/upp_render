#include "Ui3DViewDemo.h"
#include <plugin/png/png.h>
#include <windows.h>
#include <cstring>
#include <algorithm>

namespace Upp {

Ui3DViewDemo::Ui3DViewDemo()
{
	Title("Ui3DView | Vulkan viewport");
	Sizeable().Zoomable();
	SetRect(100, 80, DPI(1240), DPI(780));
	SetMinSize(Size(DPI(880), DPI(560)));
	UiTheme::Set(UiThemePreset::Olive, UiThemeMode::Dark);
	title.SetTitle("Ui3DView").SetSubTitle("A native viewport for your applications");
	Add(title);
	for(UiButton *button : { &btn_projection, &btn_fit, &btn_grid, &btn_axes,
	                       &btn_wire, &btn_theme, &btn_help, &btn_exit })
		Add(*button);
	Add(view);
	btn_projection.SetText("Perspective");
	btn_fit.SetText("Fit all").Tip("Home: frame the entire scene");
	btn_grid.SetText("Grid").SetCheckable().SetChecked();
	btn_axes.SetText("Axes").SetCheckable().SetChecked();
	btn_wire.SetText("Wireframe").SetCheckable();
	btn_theme.SetText("Light");
	btn_help.SetText("Help");
	btn_exit.SetText("Exit");
	btn_projection.WhenAction = [this] { SetOrthographic(!view.GetCamera().orthographic); };
	btn_fit.WhenAction = [this] { view.FitAll(); last_event = "Scene framed"; RefreshStatus(); };
	btn_grid.WhenAction = btn_axes.WhenAction = btn_wire.WhenAction = [this] { ApplyOptions(); };
	btn_theme.WhenAction = [this] { SetLight(!light); };
	btn_help.WhenAction = [] {
		PromptOK("Middle drag: orbit&Shift + middle drag: pan&Wheel: zoom&"
		         "Alt + left / middle / right drag: orbit / pan / dolly&"
		         "Click: select. Ctrl-click: toggle. Shift-click: add.&"
		         "F: frame selection. Home: frame scene. Escape: cancel navigation.&"
		         "The host application owns selection and document edits.");
	};
	btn_exit.WhenAction = [this] { Close(); };
	view.WhenSelectionRequest = [this](const Ui3DSelectionRequest& request) {
		// Ignore a request against a replaced scene/camera; the host owns acceptance.
		if(!view.IsCurrentPick(request.pick)) return;
		auto selection = view.GetSelection();
		if(!request.extend && !request.toggle) selection.clear();
		if(request.pick.hit) {
			auto it = std::find(selection.begin(), selection.end(), request.pick.id);
			if(it == selection.end()) selection.push_back(request.pick.id);
			else if(request.toggle) selection.erase(it);
		}
		String error;
		if(!view.SetSelection(selection, error)) last_event = error;
		else {
			selection_name = selection.empty() ? String("No object selected") : selection.back();
			last_event = request.pick.hit ? "Selection accepted by application" : "Selection cleared";
		}
		RefreshStatus();
	};
	view.WhenNavigation = [this](const Ui3DNavigationEvent& event) {
		last_event = event.phase == Ui3DNavigationPhase::Cancel ? "Navigation cancelled" :
		             event.phase == Ui3DNavigationPhase::Commit ? "Camera updated" : "Navigating";
		RefreshStatus();
	};
	BuildScene();
	Layout();
	view.FitAll();
	RefreshStatus();
}

Ui3DViewDemo::~Ui3DViewDemo() { status_refresh.Kill(); }

void Ui3DViewDemo::BuildScene()
{
	String error;
	auto box = Scene3DMesh::MakeBox(Scene3DVec3(2, 2, 2), error);
	auto ground = Scene3DMesh::MakeBox(Scene3DVec3(8, 0.08, 8), error);
	if(!box || !ground) { last_event = error; return; }
	Scene3DSceneDesc description;
	description.revision = 1;
	Scene3DObjectDesc blue;
	blue.id = "demo:blue-box"; blue.mesh = box;
	blue.local = Scene3DMatrix::Translation(Scene3DVec3(-0.8, 1.05, 0)) *
	             Scene3DMatrix::RotationY(0.28);
	blue.r = 0.13f; blue.g = 0.42f; blue.b = 0.68f;
	description.objects.push_back(blue);
	Scene3DObjectDesc orange;
	orange.id = "demo:orange-box"; orange.mesh = box;
	orange.local = Scene3DMatrix::Translation(Scene3DVec3(0.9, 0.85, 0.45)) *
	               Scene3DMatrix::RotationY(-0.42) *
	               Scene3DMatrix::Scale(Scene3DVec3(0.78, 0.85, 0.85));
	orange.r = 0.8f; orange.g = 0.30f; orange.b = 0.08f;
	description.objects.push_back(orange);
	Scene3DObjectDesc column;
	column.id = "demo:column"; column.mesh = box;
	column.local = Scene3DMatrix::Translation(Scene3DVec3(2.1, 1.6, -1.8)) *
	               Scene3DMatrix::Scale(Scene3DVec3(0.38, 1.6, 0.38));
	column.r = 0.34f; column.g = 0.52f; column.b = 0.31f;
	description.objects.push_back(column);
	Scene3DObjectDesc floor;
	floor.id = "demo:ground"; floor.mesh = ground; floor.selectable = false;
	floor.local = Scene3DMatrix::Translation(Scene3DVec3(0, -0.06, 0));
	floor.r = 0.16f; floor.g = 0.19f; floor.b = 0.23f;
	description.objects.push_back(floor);
	auto scene = Scene3DSnapshot::Create(description, error);
	if(!scene || !view.SetScene(scene, error)) last_event = error;
}

void Ui3DViewDemo::ApplyOptions()
{
	auto options = light ? Ui3DViewOptions::Light() : Ui3DViewOptions::Dark();
	options.show_grid = btn_grid.IsChecked();
	options.show_axes = btn_axes.IsChecked();
	options.wireframe = btn_wire.IsChecked();
	options.grid_extent = 10;
	String error;
	if(!view.SetOptions(options, error)) last_event = error;
	RefreshStatus();
}

void Ui3DViewDemo::SetOrthographic(bool on)
{
	Scene3DCamera camera = view.GetCamera();
	camera.orthographic = on;
	String error;
	if(view.SetCamera(camera, error)) {
		btn_projection.SetText(on ? "Orthographic" : "Perspective");
		last_event = on ? "Orthographic view" : "Perspective view";
	}
	else last_event = error;
	RefreshStatus();
}

void Ui3DViewDemo::SetWireframe(bool on)
{
	btn_wire.SetChecked(on);
	ApplyOptions();
}

void Ui3DViewDemo::SetLight(bool on)
{
	light = on;
	UiTheme::Set(UiThemePreset::Olive, on ? UiThemeMode::Light : UiThemeMode::Dark);
	btn_theme.SetText(on ? "Dark" : "Light");
	ApplyOptions();
	Refresh();
}

void Ui3DViewDemo::RefreshStatus()
{
	Refresh();
	status_refresh.Kill();
	status_refresh.Set(100, [this] { Refresh(); });
}

void Ui3DViewDemo::Layout()
{
	const Size size = GetSize();
	const int margin = DPI(16), side = size.cx >= DPI(1020) ? DPI(248) : DPI(210);
	title.SetRect(margin, DPI(8), max(1, size.cx - DPI(330)), DPI(68));
	btn_theme.SetRect(size.cx - DPI(238), DPI(24), DPI(70), DPI(34));
	btn_help.SetRect(size.cx - DPI(160), DPI(24), DPI(64), DPI(34));
	btn_exit.SetRect(size.cx - DPI(88), DPI(24), DPI(70), DPI(34));
	int x = margin;
	UiButton *tools[] = { &btn_projection, &btn_fit, &btn_grid, &btn_axes, &btn_wire };
	const int widths[] = { 136, 92, 70, 70, 108 };
	for(int i = 0; i < 5; ++i) {
		tools[i]->SetRect(x, DPI(86), DPI(widths[i]), DPI(34));
		x += DPI(widths[i] + 8);
	}
	view.SetRect(margin, DPI(134), max(1, size.cx - side - 3 * margin),
	             max(1, size.cy - DPI(178)));
	RefreshStatus();
}

void Ui3DViewDemo::Paint(Draw& draw)
{
	const Size size = GetSize();
	const Color background = light ? Color(242, 244, 246) : Color(24, 28, 33);
	const Color panel = light ? Color(232, 236, 240) : Color(31, 36, 43);
	const Color ink = light ? Color(37, 45, 53) : Color(223, 230, 236);
	const Color muted = light ? Color(95, 108, 118) : Color(151, 164, 177);
	const Color accent = light ? Color(70, 112, 69) : Color(181, 215, 165);
	draw.DrawRect(size, background);
	const int x = view.GetRect().right + DPI(16);
	const int w = max(0, size.cx - x - DPI(16));
	draw.DrawRect(x, DPI(134), w, max(0, size.cy - DPI(178)), panel);
	draw.Clip(x + DPI(16), DPI(148), max(0, w - DPI(32)), max(0, size.cy - DPI(210)));
	int y = DPI(154);
	auto line = [&](const String& text, Color color, int height = 25, bool bold = false) {
		draw.DrawText(x + DPI(16), y, text, bold ? SansSerifZ(11).Bold() : SansSerifZ(10), color);
		y += DPI(height);
	};
	line("VIEW", accent, 30, true);
	line(view.GetCamera().orthographic ? "Orthographic" : "Perspective", ink);
	line("Y up  /  metres", muted, 34);
	line("SELECTION", accent, 28, true);
	line(selection_name, ink, 26);
	line(Format("%d selected", (int)view.GetSelection().size()), muted, 34);
	line("RENDERER", accent, 28, true);
	const auto stats = view.GetViewStats();
	line(stats.depth_active ? "Vulkan  /  depth active" : "Initializing Vulkan", ink);
	line(Format("%d x %d pixels", stats.target_size.cx, stats.target_size.cy), muted);
	line(Format("%d retained meshes", stats.resident_meshes), muted);
	line(Format("%d uploads", (int)stats.mesh_uploads), muted);
	line(Format("%d presented frames", (int)view.GetGpuStats().presented_frames), muted, 34);
	line("NAVIGATION", accent, 28, true);
	line("Middle drag    Orbit", ink);
	line("Shift + middle   Pan", ink);
	line("Wheel              Zoom", ink);
	line("F / Home           Frame", ink);
	line("Escape              Cancel", ink);
	String error = view.GetViewError();
	if(!error.IsEmpty()) { y += DPI(14); line(error, Color(227, 110, 105)); }
	draw.End();
	draw.DrawText(DPI(18), size.cy - DPI(29), last_event, SansSerifZ(9), muted);
	draw.DrawText(max(DPI(280), size.cx - DPI(430)), size.cy - DPI(29),
	              "Retained geometry  /  application-owned scene", SansSerifZ(9), muted);
}

bool Ui3DViewDemo::CheckReady(String& error) const
{
	error = view.GetViewError();
	if(error.IsEmpty() && (!view.IsGpuReady() || !view.GetViewStats().depth_active ||
	                      !view.GetViewStats().rendered_frames))
		error = "viewport has not completed a depth-tested Vulkan frame";
	return error.IsEmpty();
}

// Diagnostic screenshot of this visible client only. This is OS observation,
// not the future renderer capture/depth/ID export API.
bool Ui3DViewDemo::Capture(const String& file)
{
	RECT rect;
	if(GetForegroundWindow() != GetHWND() || !GetClientRect(GetHWND(), &rect)) return false;
	const int width = rect.right, height = rect.bottom;
	if(width <= 0 || height <= 0 || (int64)width * height > 16000000) return false;
	POINT origin = {0, 0};
	if(!ClientToScreen(GetHWND(), &origin)) return false;
	HDC screen = GetDC(nullptr), memory = CreateCompatibleDC(screen);
	BITMAPINFO info {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void *pixels = nullptr;
	HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
	bool ok = memory && bitmap && pixels;
	HGDIOBJ old = nullptr;
	if(ok) {
		old = SelectObject(memory, bitmap);
		ok = BitBlt(memory, 0, 0, width, height, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT);
	}
	if(ok) {
		ImageBuffer image(width, height);
		const byte *source = static_cast<const byte *>(pixels);
		for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x) {
			const byte *p = source + ((int64)y * width + x) * 4;
			RGBA& target = image[y][x];
			target.r = p[2]; target.g = p[1]; target.b = p[0]; target.a = 255;
		}
		image.SetKind(IMAGE_OPAQUE);
		ok = PNGEncoder().SaveFile(file, Image(image));
	}
	if(old) SelectObject(memory, old);
	if(bitmap) DeleteObject(bitmap);
	if(memory) DeleteDC(memory);
	if(screen) ReleaseDC(nullptr, screen);
	return ok;
}

}
