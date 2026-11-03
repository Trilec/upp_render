// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#include "Ui3DViewport.h"
#include <Ui/UiToolButton.h>
#include <Ui/UiLabel.h>
#include <Ui/UiTextLayout.h>
#include <Ui/UiIcons.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace Upp {

namespace {

void Ui3DViewportShow(Ctrl& control, bool shown)
{
	if(control.IsShown() != shown) control.Show(shown);
}

void Ui3DViewportPlace(Ctrl& control, const Rect& rect)
{
	if(control.GetRect() != rect) control.SetRect(rect);
}

template <class T>
void Ui3DViewportText(T& control, const String& text)
{
	if(control.GetText() != text) control.SetText(text);
}

void Ui3DViewportTip(Ctrl& control, const String& text)
{
	if(control.GetTip() != text) control.Tip(text);
}

void Ui3DViewportCheck(UiToolButton& control, bool checked)
{
	if(control.IsChecked() != checked) control.SetChecked(checked);
}

// This package deliberately owns no global theme or application camera model.
struct Ui3DViewportColors {
	Color panel, bar, control, hot, pressed, ink, muted, accent, border, viewport;
	explicit Ui3DViewportColors(bool dark)
	{
		if(dark) {
			panel = Color(32, 38, 45); bar = Color(32, 38, 44);
			control = Color(43, 50, 58); hot = Color(53, 65, 77);
			pressed = Color(62, 82, 100); ink = Color(228, 233, 238);
			muted = Color(170, 181, 193); accent = Color(145, 180, 207);
			border = Color(104, 139, 167); viewport = Color(34, 40, 47);
		}
		else {
			panel = Color(246, 247, 248); bar = Color(241, 243, 245);
			control = Color(232, 237, 241); hot = Color(218, 230, 240);
			pressed = Color(199, 218, 234); ink = Color(39, 50, 60);
			muted = Color(82, 98, 114); accent = Color(52, 95, 128);
			border = Color(120, 155, 181); viewport = Color(230, 233, 236);
		}
	}
};

const char *Ui3DViewportDirectionName(Scene3DViewDirection direction)
{
	switch(direction) {
	case Scene3DViewDirection::Front: return "Front";
	case Scene3DViewDirection::Back: return "Back";
	case Scene3DViewDirection::Right: return "Right";
	case Scene3DViewDirection::Left: return "Left";
	case Scene3DViewDirection::Top: return "Top";
	case Scene3DViewDirection::Bottom: return "Bottom";
	default: return "User";
	}
}

Scene3DViewDirection Ui3DViewportAxisDirection(int axis)
{
	static const Scene3DViewDirection directions[] = {
		Scene3DViewDirection::Right, Scene3DViewDirection::Left,
		Scene3DViewDirection::Top, Scene3DViewDirection::Bottom,
		Scene3DViewDirection::Front, Scene3DViewDirection::Back
	};
	return directions[axis];
}

void Ui3DViewportMenuItem(UiMenu& menu, const String& text, int command,
	                    bool checkable = false, bool checked = false,
	                    bool separator = false, bool radio = false, bool enabled = true)
{
	UiMenuItem item(text, command);
	item.enabled = enabled;
	item.checkable = checkable;
	item.checked = checked;
	item.separator_before = separator;
	item.radio = radio;
	menu.Model().AddChild(menu.Model().Root(), item);
}

bool Ui3DViewportHasSelection(const Ui3DView& view)
{
	auto scene = view.GetScene();
	if(!scene) return false;
	for(const String& id : view.GetSelection()) {
		const Scene3DObject *object = scene->FindObject(id);
		if(object && object->visible && !object->bounds.empty) return true;
	}
	return false;
}

// Small vector marks stay crisp at the same DPI as their standard UiToolButton.
// Interaction, accessibility text, keyboard and focus remain button behavior.
enum Ui3DViewportIcon { VIEWPORT_DISPLAY, VIEWPORT_OVERLAYS,
	VIEWPORT_MAXIMIZE, VIEWPORT_ORBIT, VIEWPORT_PAN, VIEWPORT_PLUS,
	VIEWPORT_MINUS, VIEWPORT_FIT };

void Ui3DViewportDrawIcon(Draw& draw, const Rect& rect, int icon, Color ink)
{
	const int x = rect.CenterPoint().x, y = rect.CenterPoint().y;
	const int r = DPI(7), s = DPI(3), line = max(1, DPI(1));
	auto segment = [&](int x0, int y0, int x1, int y1) {
		draw.DrawLine(x + x0, y + y0, x + x1, y + y1, line, ink);
	};
	switch(icon) {
	case VIEWPORT_DISPLAY:
		draw.DrawEllipse(x-r, y-r, 2*r+1, 2*r+1, ink);
		segment(0, -r, 0, r); break;
	case VIEWPORT_OVERLAYS:
		for(int a = -1; a <= 1; ++a) {
			segment(-r, a*s, r, a*s); segment(a*s, -r, a*s, r);
		}
		break;
	case VIEWPORT_MAXIMIZE:
		segment(-r, -s, -r, -r); segment(-r, -r, -s, -r);
		segment(s, -r, r, -r); segment(r, -r, r, -s);
		segment(r, s, r, r); segment(r, r, s, r);
		segment(-s, r, -r, r); segment(-r, r, -r, s); break;
	case VIEWPORT_ORBIT:
		draw.DrawEllipse(x-r, y-r+s, 2*r+1, 2*(r-s)+1, Null, line, ink);
		segment(r-s, -r+s, r, -r+s); segment(r, -r+s, r, -r);
		break;
	case VIEWPORT_PAN:
		segment(-r, 0, r, 0); segment(0, -r, 0, r);
		segment(-r, 0, -r+s, -s); segment(-r, 0, -r+s, s);
		segment(r, 0, r-s, -s); segment(r, 0, r-s, s);
		segment(0, -r, -s, -r+s); segment(0, -r, s, -r+s);
		segment(0, r, -s, r-s); segment(0, r, s, r-s); break;
	case VIEWPORT_PLUS: segment(0, -r, 0, r); // fall through
	case VIEWPORT_MINUS: segment(-r, 0, r, 0); break;
	case VIEWPORT_FIT:
		segment(-r, -s, -r, -r); segment(-r, -r, -s, -r);
		segment(s, -r, r, -r); segment(r, -r, r, -s);
		segment(r, s, r, r); segment(r, r, s, r);
		segment(-s, r, -r, r); segment(-r, r, -r, s);
		draw.DrawEllipse(x-s, y-s, 2*s+1, 2*s+1, ink); break;
	}
}

}

struct Ui3DViewport::Impl {
	struct AxisPoint { Point point; double depth = 0; int axis = 0; };
	class AxisCtrl : public Ctrl {
	public:
		Impl *impl = nullptr;
		std::array<AxisPoint, 6> axes;
		std::array<int, 6> order;
		Point center;
		int radius = 1, center_radius = 1, hot = -1, pressed = -1, key_part = 6;
		Size geometry_size = Size(0, 0);
		double geometry_yaw = 0, geometry_pitch = 0;
		int geometry_dpi = 0;
		bool have_geometry = false, geometry_orthographic = false;

		AxisCtrl() { WantFocus(); Transparent(); }
		void Update();
		int Hit(Point point) const;
		void Paint(Draw& draw) override;
		Image MouseEvent(int event, Point point, int delta, dword flags) override;
		void LeftDown(Point point, dword flags) override;
		void LeftUp(Point point, dword flags) override;
		void MouseMove(Point point, dword flags) override;
		void MouseLeave() override { if(hot >= 0) { hot = -1; Refresh(); } }
		void CancelMode() override { if(pressed >= 0) { pressed = -1; Refresh(); } }
		void GotFocus() override { Refresh(); }
		void LostFocus() override { Refresh(); }
		bool Key(dword key, int count) override;
	};

	class OverlayPanel : public UiPanel {
	public:
		Impl *impl = nullptr;
		// Empty shape gaps stay interactive for the underlying renderer.
		Image MouseEvent(int event, Point point, int delta, dword flags) override
		{
			return impl ? impl->ForwardToView(*this, event, point, delta, flags)
			            : UiPanel::MouseEvent(event, point, delta, flags);
		}
	};

	class AnnotationLabel : public UiLabel {
	public:
		Impl *impl = nullptr;
		AnnotationLabel() { NoWantFocus(); NoTransparent(); }
		void Paint(Draw& draw) override
		{
			if(impl) ++impl->chrome_stats.annotation_paints;
			UiLabel::Paint(draw);
		}
		Image MouseEvent(int event, Point point, int delta, dword flags) override
		{
			return impl ? impl->ForwardToView(*this, event, point, delta, flags)
			            : UiLabel::MouseEvent(event, point, delta, flags);
		}
	};

#ifdef GUI_WIN
	class OverlayHost : public DHCtrl {
	public:
		Impl *impl = nullptr;
		OverlayHost() { NoWantFocus(); }
		void State(int reason) override
		{
			// Windows can recycle an HWND value after close/reopen. A matching
			// handle and geometry must never reuse a region owned by the old window.
			if(impl && (reason == OPEN || reason == CLOSE)) impl->region_hwnd = nullptr;
			DHCtrl::State(reason);
			if(reason == OPEN && GetHWND()) {
				// Both native siblings honour their z-order when painting. This is
				// a one-time window-style change, not a camera-frame reorder.
				LONG_PTR style = GetWindowLongPtr(GetHWND(), GWL_STYLE);
				if(!(style & WS_CLIPSIBLINGS)) {
					SetWindowLongPtr(GetHWND(), GWL_STYLE, style | WS_CLIPSIBLINGS);
					SetWindowPos(GetHWND(), nullptr, 0, 0, 0, 0,
					             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
				}
			}
			if(impl && reason != CLOSE) impl->SyncNativeOverlay();
		}
		Image MouseEvent(int event, Point point, int delta, dword flags) override
		{
			return impl ? impl->ForwardToView(*this, event, point, delta, flags)
			            : DHCtrl::MouseEvent(event, point, delta, flags);
		}
		LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override
		{
			GuiLock lock;
			if(message == WM_ERASEBKGND) return 1;
			if(message == WM_PAINT) {
				HWND hwnd = GetHWND();
				PAINTSTRUCT paint;
				HDC dc = BeginPaint(hwnd, &paint);
				if(dc && impl) {
					++impl->chrome_stats.native_paints;
					SystemDraw draw(dc);
					// Draw bounded ordinary children, never the DHCtrl itself.
					// A full-viewport DrawCtrl would allocate a full-size U++
					// backbuffer merely to update these small annotation regions.
					Rect clip(paint.rcPaint.left, paint.rcPaint.top,
					          paint.rcPaint.right, paint.rcPaint.bottom);
					auto paint_child = [&](Ctrl& child) {
						Rect rect = child.GetRect();
						if(child.IsShown() && !rect.IsEmpty() && rect.Intersects(clip) && draw.IsPainting(rect)) {
							++impl->chrome_stats.tile_paints;
							uint64 pixels = uint64(rect.Width()) * uint64(rect.Height());
							impl->chrome_stats.max_tile_pixels = max(impl->chrome_stats.max_tile_pixels, pixels);
							child.DrawCtrl(draw, rect.left, rect.top);
						}
					};
					paint_child(impl->overlay_panel);
					paint_child(impl->projection_label);
					paint_child(impl->view_label);
					paint_child(impl->units_label);
					paint_child(impl->mouse_hint);
				}
				EndPaint(hwnd, &paint);
				return 0;
			}
			return DHCtrl::WindowProc(message, wp, lp);
		}
	};
#else
	class OverlayHost : public Ctrl {
	public:
		Impl *impl = nullptr;
		OverlayHost() { NoWantFocus(); Transparent(); }
		Image MouseEvent(int event, Point point, int delta, dword flags) override
		{
			return impl ? impl->ForwardToView(*this, event, point, delta, flags)
			            : Ctrl::MouseEvent(event, point, delta, flags);
		}
	};
#endif

	Ui3DViewport& owner;
	Ui3DView view;
	UiPanel bar;
	UiToolButton direction, camera, wire, solid, edges, display, overlays, maximize;
	UiMenu direction_menu, display_menu, overlay_menu, camera_menu;
	OverlayHost overlay;
	OverlayPanel overlay_panel;
	AxisCtrl axes;
	UiToolButton orbit, pan, zoom_in, zoom_out, fit;
	AnnotationLabel projection_label, view_label, units_label, mouse_hint;
	bool active = false, dark = true, toolbar_visible = true;
	bool orientation_visible = true, navigation_visible = true, annotations_visible = true, compact = false;
	bool syncing_native = false;
	String camera_label = "Free camera";
	int direction_icon = -1;
	Size requested_minimum = Size(0, 0);
	Size projection_slot = Size(0, 0), view_slot = Size(0, 0);
	Size units_slot = Size(0, 0), hint_slot = Size(0, 0);
	Color overlay_background;
	Ui3DViewportChromeStats chrome_stats;
#ifdef GUI_WIN
	HWND region_hwnd = nullptr;
	uint64 region_key = 0;
#endif

	explicit Impl(Ui3DViewport& viewport);
	~Impl()
	{
		overlay.impl = nullptr; overlay_panel.impl = nullptr; axes.impl = nullptr;
		projection_label.impl = nullptr; view_label.impl = nullptr;
		units_label.impl = nullptr; mouse_hint.impl = nullptr;
	}
	Ptr<Ui3DViewport> Activate();
	Image ForwardToView(Ctrl& source, int event, Point point, int delta, dword flags);
	void CameraCommand(int command, int value = 0);
	void AxisCommand(int axis);
	void DisplayCommand(Ui3DDisplayMode mode);
	void OverlayCommand(int command);
	void ToolCommand(Ui3DNavigationTool tool);
	void DirectionMenu();
	void DisplayMenu();
	void OverlayMenu();
	void CameraMenu();
	void ApplyStyle();
	void UpdateDirectionButton();
	void UpdateAnnotationSlots();
	void UpdateAnnotations();
	void Layout();
	void RefreshChrome();
	void SyncNativeOverlay();
	void SetupIcon(UiToolButton& button, int icon, const char *tip);
};

Ui3DViewport::Impl::Impl(Ui3DViewport& viewport) : owner(viewport)
{
	owner.Add(view); owner.Add(bar); owner.Add(overlay);
	bar.Add(direction); bar.Add(camera); bar.Add(wire); bar.Add(solid); bar.Add(edges);
	bar.Add(display); bar.Add(overlays); bar.Add(maximize);
	overlay.impl = this; overlay_panel.impl = this; axes.impl = this;
	overlay.Add(overlay_panel);
	AnnotationLabel *labels[] = { &projection_label, &view_label, &units_label, &mouse_hint };
	for(AnnotationLabel *label : labels) { label->impl = this; overlay.Add(*label); }
	overlay_panel.Add(axes); overlay_panel.Add(orbit); overlay_panel.Add(pan);
	overlay_panel.Add(zoom_in); overlay_panel.Add(zoom_out); overlay_panel.Add(fit);
	bar.NoWantFocus(); overlay_panel.NoWantFocus();
	for(UiToolButton *button : { &direction, &camera }) {
		button->SetText("").SetAlign(UiAlign::CENTER, UiAlign::CENTER).SetContentInset(0);
		button->SetIconRenderMode(UiIconRenderMode::MonoTint);
		button->SetIconSize(DPI(18), DPI(18));
	}
	direction.Tip("View direction and projection");
	camera.SetIcon(ICON_IMAGE_VIDEO_CAMERA_BACK_OUTLINED()).Tip("Choose a scene camera");
	wire.SetText("Wire").SetCheckable().Tip("Wireframe: all mesh edges");
	solid.SetText("Solid").SetCheckable().Tip("Opaque solid surfaces");
	edges.SetText("Edges").SetCheckable().Tip("Solid surfaces with visible edges");
	SetupIcon(display, VIEWPORT_DISPLAY, "Display mode");
	SetupIcon(overlays, VIEWPORT_OVERLAYS, "Grid, axes and overlays");
	SetupIcon(maximize, VIEWPORT_MAXIMIZE, "Maximize or restore this pane");
	SetupIcon(orbit, VIEWPORT_ORBIT, "Orbit tool (click again for selection)");
	SetupIcon(pan, VIEWPORT_PAN, "Pan tool (click again for selection)");
	SetupIcon(zoom_in, VIEWPORT_PLUS, "Zoom in");
	SetupIcon(zoom_out, VIEWPORT_MINUS, "Zoom out");
	SetupIcon(fit, VIEWPORT_FIT, "Frame all objects");
	orbit.SetCheckable(); pan.SetCheckable();
	direction.WhenAction = [this] { DirectionMenu(); };
	camera.WhenAction = [this] { CameraMenu(); };
	wire.WhenAction = [this] { DisplayCommand(Ui3DDisplayMode::Wireframe); };
	solid.WhenAction = [this] { DisplayCommand(Ui3DDisplayMode::Solid); };
	edges.WhenAction = [this] { DisplayCommand(Ui3DDisplayMode::SolidEdges); };
	display.WhenAction = [this] { DisplayMenu(); };
	overlays.WhenAction = [this] { OverlayMenu(); };
	maximize.WhenAction = [this] {
		Ptr<Ui3DViewport> alive = Activate();
		if(alive) { Event<> event = alive->WhenMaximize; event(); }
	};
	orbit.WhenAction = [this] { ToolCommand(Ui3DNavigationTool::Orbit); };
	pan.WhenAction = [this] { ToolCommand(Ui3DNavigationTool::Pan); };
	zoom_in.WhenAction = [this] { CameraCommand(2, 1); };
	zoom_out.WhenAction = [this] { CameraCommand(2, -1); };
	fit.WhenAction = [this] { CameraCommand(3); };
	direction_menu.WhenAction = [this](UiMenuNodeRef, const UiMenuItem& item) {
		int command = item.data;
		if(command >= 1 && command <= 6) CameraCommand(0, command);
		else if(command == 100) CameraCommand(1);
		else if(command == 103) CameraCommand(5);
		else CameraCommand(command == 101 ? 3 : 4);
	};
	display_menu.WhenAction = [this](UiMenuNodeRef, const UiMenuItem& item) {
		DisplayCommand(static_cast<Ui3DDisplayMode>(int(item.data)));
	};
	overlay_menu.WhenAction = [this](UiMenuNodeRef, const UiMenuItem& item) {
		OverlayCommand(int(item.data));
	};
	ApplyStyle();
}

void Ui3DViewport::Impl::SetupIcon(UiToolButton& button, int icon, const char *tip)
{
	button.Tip(tip);
	button.SetText("").SetContentInset(0);
	button.WhenPaintForeground = [icon](Draw& draw, const Rect& rect,
		const StyledPalette& palette, const StyledMetrics&, const StyledSkin&,
		StyledState state, bool) {
		Ui3DViewportDrawIcon(draw, rect, icon, palette.ink[state]);
	};
}

Ptr<Ui3DViewport> Ui3DViewport::Impl::Activate()
{
	Ptr<Ui3DViewport> alive = &owner;
	Event<> event = owner.WhenActivate;
	event();
	return alive;
}

Image Ui3DViewport::Impl::ForwardToView(Ctrl& source, int event, Point point,
	                                  int delta, dword flags)
{
	Ptr<Ui3DView> target = &view;
	point += source.GetScreenView().TopLeft() - view.GetScreenView().TopLeft();
	// The view takes capture for gestures, so subsequent motion and release go
	// straight to it even if the cursor crosses another native overlay shape.
	return target->MouseEvent(event, point, delta, flags);
}

void Ui3DViewport::Impl::CameraCommand(int command, int value)
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	Ui3DView& target = alive->impl->view;
	String error;
	bool ok = false;
	switch(command) {
	case 0: ok = target.SetViewDirection(static_cast<Scene3DViewDirection>(value), error); break;
	case 1: ok = target.SetOrthographic(!target.GetCamera().orthographic, error); break;
	case 2: {
		Scene3DCamera next = target.GetCamera();
		ok = next.Dolly(value * 0.18) && target.SetCamera(next, error);
		break;
	}
	case 3: ok = target.FitAll(); break;
	case 4: if(Ui3DViewportHasSelection(target)) ok = target.FitSelection(); break;
	case 5: {
		Scene3DCamera next = target.GetCamera(), standard;
		if(!next.SetOrthographic(false)) break;
		next.yaw = standard.yaw; next.pitch = standard.pitch;
		ok = target.SetCamera(next, error);
		break;
	}
	}
	if(!alive) return;
	alive->RefreshChrome();
	if(ok) { Event<> event = alive->WhenCameraCommand; event(); }
}

void Ui3DViewport::Impl::AxisCommand(int axis)
{
	if(axis == 6) { CameraCommand(1); return; }
	Scene3DViewDirection wanted = Ui3DViewportAxisDirection(axis);
	if(view.GetCamera().GetViewDirection() == wanted)
		wanted = Ui3DViewportAxisDirection(axis ^ 1);
	CameraCommand(0, int(wanted));
}

void Ui3DViewport::Impl::DisplayCommand(Ui3DDisplayMode mode)
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	String error;
	bool ok = alive->impl->view.SetDisplayMode(mode, error);
	if(!alive) return;
	alive->RefreshChrome();
	if(ok) { Event<> event = alive->WhenViewOptions; event(); }
}

void Ui3DViewport::Impl::ToolCommand(Ui3DNavigationTool tool)
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	Ui3DView& target = alive->impl->view;
	if(target.GetNavigationTool() == tool) tool = Ui3DNavigationTool::Select;
	String error;
	bool ok = target.SetNavigationTool(tool, error);
	if(!alive) return;
	alive->RefreshChrome();
	if(ok) { Event<> event = alive->WhenViewOptions; event(); }
}

void Ui3DViewport::Impl::OverlayCommand(int command)
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	Impl& self = *alive->impl;
	Ui3DViewOptions options = self.view.GetOptions();
	switch(command) {
	case 0: options.show_grid = !options.show_grid; break;
	case 1: options.show_axes = !options.show_axes; break;
	case 2: options.show_selection = !options.show_selection; break;
	case 3: options.adaptive_grid = !options.adaptive_grid; break;
	case 4: self.orientation_visible = !self.orientation_visible; break;
	case 5: self.navigation_visible = !self.navigation_visible; break;
	case 6: options.grid_plane = Ui3DGridPlane::XY; break;
	case 7: options.grid_plane = Ui3DGridPlane::YZ; break;
	case 8: options.grid_plane = Ui3DGridPlane::XZ; break;
	case 9: self.annotations_visible = !self.annotations_visible; break;
	default: return;
	}
	String error;
	bool ok = command == 4 || command == 5 || command == 9 || self.view.SetOptions(options, error);
	if(!alive) return;
	alive->Layout();
	if(!alive) return;
	alive->RefreshChrome();
	if(ok) { Event<> event = alive->WhenViewOptions; event(); }
}

void Ui3DViewport::Impl::DirectionMenu()
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	Impl& self = *alive->impl;
	UiMenu& menu = self.direction_menu;
	menu.ClearModel();
	Scene3DCamera current = self.view.GetCamera();
	Ui3DViewportMenuItem(menu, "Perspective", 103);
	for(int i = 1; i <= 6; ++i) {
		Scene3DViewDirection value = static_cast<Scene3DViewDirection>(i);
		Ui3DViewportMenuItem(menu, Ui3DViewportDirectionName(value), i, true,
		                    current.GetViewDirection() == value, false, true);
	}
	Ui3DViewportMenuItem(menu, "Orthographic projection", 100, true, current.orthographic, true);
	auto scene = self.view.GetScene();
	Ui3DViewportMenuItem(menu, "Frame all", 101, false, false, true, false,
	                    scene && !scene->GetBounds().empty);
	Ui3DViewportMenuItem(menu, "Frame selection", 102, false, false, false, false,
	                    Ui3DViewportHasSelection(self.view));
	menu.PopUp(alive, self.direction.GetScreenRect().BottomLeft());
}

void Ui3DViewport::Impl::DisplayMenu()
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	Impl& self = *alive->impl;
	UiMenu& menu = self.display_menu;
	menu.ClearModel();
	Ui3DDisplayMode current = self.view.GetDisplayMode();
	Ui3DViewportMenuItem(menu, "Solid", int(Ui3DDisplayMode::Solid), true,
	                    current == Ui3DDisplayMode::Solid, false, true);
	Ui3DViewportMenuItem(menu, "Wireframe", int(Ui3DDisplayMode::Wireframe), true,
	                    current == Ui3DDisplayMode::Wireframe, false, true);
	Ui3DViewportMenuItem(menu, "Solid with visible edges", int(Ui3DDisplayMode::SolidEdges), true,
	                    current == Ui3DDisplayMode::SolidEdges, false, true);
	menu.PopUp(alive, self.display.GetScreenRect().BottomLeft());
}

void Ui3DViewport::Impl::OverlayMenu()
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	Impl& self = *alive->impl;
	Ui3DViewOptions options = self.view.GetOptions();
	UiMenu& menu = self.overlay_menu;
	menu.ClearModel();
	Ui3DViewportMenuItem(menu, "Grid", 0, true, options.show_grid);
	Ui3DViewportMenuItem(menu, "World axes", 1, true, options.show_axes);
	Ui3DViewportMenuItem(menu, "Selection outline", 2, true, options.show_selection);
	Ui3DViewportMenuItem(menu, "Adaptive grid", 3, true, options.adaptive_grid, true);
	Ui3DViewportMenuItem(menu, "Grid plane XY", 6, true, options.grid_plane == Ui3DGridPlane::XY, false, true);
	Ui3DViewportMenuItem(menu, "Grid plane YZ", 7, true, options.grid_plane == Ui3DGridPlane::YZ, false, true);
	Ui3DViewportMenuItem(menu, "Grid plane XZ", 8, true, options.grid_plane == Ui3DGridPlane::XZ, false, true);
	Ui3DViewportMenuItem(menu, "Orientation control", 4, true, self.orientation_visible, true);
	Ui3DViewportMenuItem(menu, "Navigation controls", 5, true, self.navigation_visible);
	Ui3DViewportMenuItem(menu, "View annotations", 9, true, self.annotations_visible);
	menu.PopUp(alive, self.overlays.GetScreenRect().BottomLeft());
}

void Ui3DViewport::Impl::CameraMenu()
{
	Ptr<Ui3DViewport> alive = Activate();
	if(!alive) return;
	if(alive->PopulateCameraMenu) {
		Event<UiMenu&> populate = alive->PopulateCameraMenu;
		alive->impl->camera_menu.ClearModel();
		populate(alive->impl->camera_menu);
		if(!alive) return;
		Impl& self = *alive->impl;
		if(self.camera_menu.Model().GetChildCount(self.camera_menu.Model().Root()))
			self.camera_menu.PopUp(alive, self.camera.GetScreenRect().BottomLeft());
	}
	else { Event<> event = alive->WhenCameraMenu; event(); }
}

void Ui3DViewport::Impl::ApplyStyle()
{
	Ui3DViewportColors colors(dark);
	UiPanel::Style panel_style = UiPanel::StyleDefault();
	panel_style.metrics.frame_enabled = false;
	panel_style.metrics.radius = 0;
	panel_style.skin.enabled = false;
	panel_style.transparent = false;
	for(int i = 0; i < 4; ++i) panel_style.palette.face[i] = colors.bar;
	bar.SetCustomStyle(panel_style);
	Rgba8 background = view.GetOptions().background;
	overlay_background = Color(background.r, background.g, background.b);
	for(int i = 0; i < 4; ++i) panel_style.palette.face[i] = overlay_background;
#ifndef GUI_WIN
	panel_style.transparent = true;
#endif
	overlay_panel.SetCustomStyle(panel_style);
	AnnotationLabel *labels[] = { &projection_label, &view_label, &units_label, &mouse_hint };
	for(AnnotationLabel *label : labels) {
		UiLabel::Style style = label->GetStyle();
		style.skin.enabled = false;
		style.transparent = false; style.nowrap = true;
		style.metrics.frame_enabled = false; style.metrics.face_enabled = true;
		style.metrics.radius = 0;
		style.metrics.content_margin = Rect(DPI(3), DPI(1), DPI(3), DPI(1));
		style.font = StdFont().Height(DPI(label == &view_label ? 12 : 11));
		if(label == &view_label) style.font = style.font.Bold();
		for(int i = 0; i < 4; ++i) {
			style.palette.face[i] = overlay_background;
			style.palette.ink[i] = label == &view_label ? colors.ink : colors.muted;
		}
		label->SetCustomStyle(style);
		// UiLabel normally disables its face for text over a painted parent.
		// Each HUD label is drawn into its own bounded native backbuffer, so it
		// must explicitly paint every pixel with the renderer background color.
		label->NoTransparent();
	}
	UiToolButton *buttons[] = { &direction, &camera, &wire, &solid, &edges,
		&display, &overlays, &maximize, &orbit, &pan, &zoom_in, &zoom_out, &fit };
	for(UiToolButton *button : buttons) {
		UiButton::Style style = button->GetStyle();
		style.skin.enabled = false; style.transparent = false;
		style.metrics.radius = DPI(4);
		style.metrics.frame_width = DPI(1);
		style.metrics.frame_enabled = true;
		style.metrics.focus_enabled = true;
		style.metrics.focus_color = colors.accent;
		style.metrics.content_margin = Rect(DPI(5), 0, DPI(5), 0);
		style.font = StdFont().Height(DPI(12));
		style.palette.face[ST_NORMAL] = colors.control;
		style.palette.face[ST_HOT] = colors.hot;
		style.palette.face[ST_PRESSED] = colors.pressed;
		style.palette.face[ST_DISABLED] = colors.bar;
		for(int i = 0; i < 4; ++i) {
			style.palette.ink[i] = i == ST_DISABLED ? colors.muted : colors.ink;
			style.palette.icon[i] = style.palette.ink[i];
			style.palette.frame[i] = i == ST_PRESSED ? colors.border : colors.control;
		}
		button->SetCustomStyle(style);
	}
	UiMenu *menus[] = { &direction_menu, &display_menu, &overlay_menu, &camera_menu };
	for(UiMenu *menu : menus) {
		UiMenu::Style style = menu->GetStyle();
		style.popup_bg = colors.panel; style.bar_bg = colors.bar;
		style.item_ink = colors.ink; style.disabled_ink = colors.muted;
		style.right_ink = colors.muted; style.separator_color = colors.control;
		style.hot_bg = colors.hot; style.hot_frame = colors.border;
		style.pressed_bg = colors.pressed; style.pressed_frame = colors.border;
		style.active_bar_bg = colors.pressed; style.check_color = colors.accent;
		style.arrow_color = colors.muted;
		menu->SetCustomStyle(style);
	}
	UpdateAnnotationSlots();
	axes.Refresh(); // Palette/font changes repaint even when camera geometry is cached.
}

void Ui3DViewport::Impl::Layout()
{
	Ptr<Ui3DViewport> alive = &owner;
	Size size = owner.GetSize();
	const int border = max(1, DPI(1));
	int header = toolbar_visible ? min(DPI(35), max(0, size.cy - 2*border)) : 0;
	Ui3DViewportPlace(view, RectC(border, border + header, max(0, size.cx - 2*border),
	                              max(0, size.cy - 2*border - header)));
	// Resizing cancels a base-view gesture and may synchronously notify the
	// application. Its callback is allowed to close this pane.
	if(!alive) return;
	Ui3DViewportShow(bar, toolbar_visible);
	Ui3DViewportPlace(bar, RectC(border, border, max(0, size.cx - 2*border), header));
	int width = max(0, bar.GetSize().cx), button = DPI(27), gap = DPI(3);
	int y = max(0, (header - button) / 2), right = width - DPI(5);
	auto place_right = [&](UiToolButton& control, int w) {
		right -= w; Ui3DViewportPlace(control, RectC(max(0, right), y, w, button)); right -= gap;
	};
	place_right(maximize, button); place_right(overlays, button);
	bool collapsed = width < DPI(310);
	Ui3DViewportShow(display, collapsed); Ui3DViewportShow(wire, !collapsed);
	Ui3DViewportShow(solid, !collapsed); Ui3DViewportShow(edges, !collapsed);
	if(collapsed) place_right(display, button);
	else { place_right(edges, DPI(47)); place_right(solid, DPI(42)); place_right(wire, DPI(39)); }
	int direction_width = min(button, max(0, right - DPI(5) - gap - button));
	Ui3DViewportPlace(direction, RectC(DPI(5), y, direction_width, button));
	int camera_x = DPI(5) + direction_width + gap;
	int camera_width = min(button, max(0, right - camera_x));
	Ui3DViewportShow(camera, camera_width >= DPI(20));
	Ui3DViewportPlace(camera, RectC(camera_x, y, camera_width, button));
	Ui3DViewportTip(camera, camera_label + " - choose a scene camera");
	UpdateDirectionButton();
	int side = DPI(compact ? 82 : 98);
	int nav_button = DPI(27), nav_gap = DPI(4), margin = DPI(8);
	bool show_axes = orientation_visible && view.GetSize().cx >= side + 2*margin && view.GetSize().cy >= side + 2*margin;
	int nav_y = show_axes ? side + DPI(3) : 0;
	bool show_nav = navigation_visible && view.GetSize().cx >= side + 2*margin &&
	                view.GetSize().cy >= nav_y + 5*(nav_button + nav_gap) + 2*margin;
	int overlay_height = show_nav ? nav_y + 5*(nav_button + nav_gap) - nav_gap : side;
	Ui3DViewportPlace(overlay, view.GetRect());
	Ui3DViewportShow(overlay_panel, show_axes || show_nav);
	Ui3DViewportPlace(overlay_panel, RectC(view.GetSize().cx - side - margin, margin, side, overlay_height));
	Ui3DViewportShow(axes, show_axes); Ui3DViewportPlace(axes, RectC(0, 0, side, side));
	UiToolButton *nav[] = { &orbit, &pan, &zoom_in, &zoom_out, &fit };
	for(int i = 0; i < 5; ++i) {
		Ui3DViewportShow(*nav[i], show_nav);
		Ui3DViewportPlace(*nav[i], RectC(side - nav_button - DPI(3), nav_y + i*(nav_button + nav_gap), nav_button, nav_button));
	}
	axes.Update();
	UpdateAnnotationSlots();
	UpdateAnnotations();
	SyncNativeOverlay();
	// Geometry changes can move several buttons within one parent paint cycle.
	// Invalidate the complete header after placement so old text is not retained
	// between their old and new rectangles. Camera-only updates skip Layout().
	bar.Refresh();
}

void Ui3DViewport::Impl::UpdateDirectionButton()
{
	Scene3DCamera current = view.GetCamera();
	Scene3DViewDirection dir = current.GetViewDirection();
	String label = dir == Scene3DViewDirection::User ?
	               (current.orthographic ? "User ortho" : "Perspective") :
	               Ui3DViewportDirectionName(dir);
	Ui3DViewportTip(direction, label + (current.orthographic ? " - orthographic" : " - perspective"));
	int icon = dir == Scene3DViewDirection::User ? 0 :
	           (dir == Scene3DViewDirection::Top || dir == Scene3DViewDirection::Bottom ? 2 : 1);
	if(direction_icon != icon) {
		direction_icon = icon;
		direction.SetIcon(icon == 2 ? ICON_DESIGN_BORDER_OUTER_48() :
		                  icon == 1 ? ICON_UI_ACTION_RESPONSIVE_LAYOUT_OUTLINED() :
		                              ICON_UI_ACTION_DEPLOYED_CODE_OUTLINED());
	}
}

void Ui3DViewport::Impl::UpdateAnnotationSlots()
{
	// Measure the finite supported vocabulary, not the current camera label.
	// Camera changes only repaint text inside a stable native footprint.
	auto measure = [](const UiLabel& label, std::initializer_list<const char *> texts) {
		const UiLabel::Style& style = label.GetStyle();
		Font font = style.metrics.use_text_font ? style.metrics.text_font : style.font;
		if(IsNull(font)) font = StdFont();
		Size size(0, 0);
		for(const char *text : texts) {
			Size measured = UiGetTextSize(text, font);
			size.cx = max(size.cx, measured.cx); size.cy = max(size.cy, measured.cy);
		}
		return UiStyledOuterSizeFromContent(size, style.metrics, style.skin);
	};
	projection_slot = measure(projection_label, {
		"Perspective  |  Solid", "Perspective  |  Wireframe", "Perspective  |  Solid + edges",
		"Orthographic  |  Solid", "Orthographic  |  Wireframe", "Orthographic  |  Solid + edges"
	});
	view_slot = measure(view_label, { "Front", "Back", "Right", "Left", "Top", "Bottom",
	                                "Perspective", "User orthographic" });
	units_slot = measure(units_label, { "Y up  \xC2\xB7  metres" });
	hint_slot = measure(mouse_hint, { "MMB orbit  \xC2\xB7  Shift+MMB pan  \xC2\xB7  Wheel zoom" });
}

void Ui3DViewport::Impl::UpdateAnnotations()
{
	Scene3DCamera current = view.GetCamera();
	Scene3DViewDirection direction = current.GetViewDirection();
	String name = direction == Scene3DViewDirection::User ?
	              (current.orthographic ? "User orthographic" : "Perspective") :
	              Ui3DViewportDirectionName(direction);
	String display_name;
	switch(view.GetDisplayMode()) {
	case Ui3DDisplayMode::Wireframe: display_name = "Wireframe"; break;
	case Ui3DDisplayMode::SolidEdges: display_name = "Solid + edges"; break;
	default: display_name = "Solid"; break;
	}
	String projection = current.orthographic ? "Orthographic" : "Perspective";
	String top_text = projection + "  |  " + display_name;
	Size size = view.GetSize();
	int margin = DPI(8), gap = DPI(2);
	int top_width = max(0, size.cx - 2*margin);
	if(overlay_panel.IsShown()) top_width = max(0, overlay_panel.GetRect().left - 2*margin);
	if(GetTextSize(top_text, projection_label.GetStyle().font).cx + DPI(6) > top_width)
		top_text = String(current.orthographic ? "Ortho" : "Persp") + "  |  " + display_name;
	Ui3DViewportText(projection_label, top_text);
	Ui3DViewportText(view_label, name);
	Ui3DViewportText(units_label, "Y up  \xC2\xB7  metres");
	Ui3DViewportText(mouse_hint, "MMB orbit  \xC2\xB7  Shift+MMB pan  \xC2\xB7  Wheel zoom");
	bool show = annotations_visible && size.cx >= DPI(110) && size.cy >= DPI(70);
	auto place_label = [&](AnnotationLabel& label, Size slot, Point origin, int maximum_width, bool visible) {
		Ui3DViewportShow(label, visible && maximum_width >= DPI(30));
		Ui3DViewportPlace(label, RectC(origin.x, origin.y, min(slot.cx, max(0, maximum_width)), slot.cy));
	};
	place_label(projection_label, projection_slot, Point(margin, margin), top_width, show);
	Size units = units_slot, caption = view_slot;
	int baseline = size.cy - margin;
	int left_width = max(0, size.cx - 2*margin);
	// Keep labels clear of the lowest navigation button in short, narrow panes.
	if(overlay_panel.IsShown() && fit.IsShown() &&
	   overlay_panel.GetRect().top + fit.GetRect().bottom > baseline - units.cy - caption.cy - gap)
		left_width = max(0, overlay_panel.GetRect().left + fit.GetRect().left - 2*margin);
	place_label(units_label, units_slot, Point(margin, baseline-units.cy), left_width, show);
	place_label(view_label, view_slot, Point(margin, baseline-units.cy-gap-caption.cy), left_width, show);
	Size hint = hint_slot;
	place_label(mouse_hint, hint_slot, Point(max(margin, size.cx-margin-hint.cx), baseline-hint.cy),
	            max(0, size.cx-2*margin), show && !compact && size.cx >= DPI(620));
	// Annotations can be displayed with both orientation and navigation hidden.
	// When every feature is off the single native child is hidden altogether.
	Ui3DViewportShow(overlay, overlay_panel.IsShown() || projection_label.IsShown() || view_label.IsShown() ||
	                         units_label.IsShown() || mouse_hint.IsShown());
}

void Ui3DViewport::Impl::RefreshChrome()
{
	Rgba8 background = view.GetOptions().background;
	Color color(background.r, background.g, background.b);
	if(overlay_background != color) {
		overlay_background = color;
		UiPanel::Style style = overlay_panel.GetStyle();
		for(int i = 0; i < 4; ++i) style.palette.face[i] = color;
		overlay_panel.SetCustomStyle(style);
		AnnotationLabel *labels[] = { &projection_label, &view_label, &units_label, &mouse_hint };
		for(AnnotationLabel *label : labels) {
			UiLabel::Style label_style = label->GetStyle();
			for(int i = 0; i < 4; ++i) label_style.palette.face[i] = color;
			label->SetCustomStyle(label_style);
		}
		axes.Refresh(); // The axis shading also uses this viewport background.
	}
	UpdateDirectionButton();
	Ui3DDisplayMode mode = view.GetDisplayMode();
	Ui3DViewportCheck(wire, mode == Ui3DDisplayMode::Wireframe);
	Ui3DViewportCheck(solid, mode == Ui3DDisplayMode::Solid);
	Ui3DViewportCheck(edges, mode == Ui3DDisplayMode::SolidEdges);
	Ui3DViewportCheck(orbit, view.GetNavigationTool() == Ui3DNavigationTool::Orbit);
	Ui3DViewportCheck(pan, view.GetNavigationTool() == Ui3DNavigationTool::Pan);
	auto scene = view.GetScene();
	bool can_fit = scene && !scene->GetBounds().empty;
	if(fit.IsEnabled() != can_fit) fit.Enable(can_fit);
	axes.Update();
	UpdateAnnotations();
	SyncNativeOverlay();
}

void Ui3DViewport::Impl::AxisCtrl::Update()
{
	if(!impl) return;
	Size size = GetSize();
	Scene3DCamera camera = impl->view.GetCamera();
	if(have_geometry && geometry_size == size && geometry_dpi == DPI(7) &&
	   geometry_yaw == camera.yaw && geometry_pitch == camera.pitch &&
	   geometry_orthographic == camera.orthographic) return;
	geometry_size = size; geometry_dpi = DPI(7);
	geometry_yaw = camera.yaw; geometry_pitch = camera.pitch;
	geometry_orthographic = camera.orthographic; have_geometry = true;
	center = Point(size.cx/2, size.cy/2);
	radius = max(DPI(7), min(size.cx, size.cy)/10);
	center_radius = DPI(7);
	double length = max(1, min(size.cx, size.cy)/2 - radius - DPI(4));
	Scene3DVec3 right, up, forward;
	camera.GetBasis(right, up, forward);
	std::array<Pointf, 6> positions;
	for(int i = 0; i < 6; ++i) {
		Scene3DVec3 unit;
		double sign = (i & 1) ? -1.0 : 1.0;
		if(i/2 == 0) unit.x = sign;
		if(i/2 == 1) unit.y = sign;
		if(i/2 == 2) unit.z = sign;
		positions[i] = Pointf(length*unit.Dot(right), -length*unit.Dot(up));
		axes[i].depth = -unit.Dot(forward); axes[i].axis = i; order[i] = i;
	}
	// Preserve the projected arrangement while separating very close labels.
	// Exact poles otherwise stack both signed ends on the projection toggle.
	// Pixel separation also covers nearly axial and diagonal camera poses.
	const double center_clearance = center_radius + radius + DPI(2);
	const double pair_clearance = 2*radius + DPI(1);
	for(int iteration = 0; iteration < 8; ++iteration) {
		for(int i = 0; i < 6; ++i) {
			double dx = positions[i].x, dy = positions[i].y;
			double distance = std::sqrt(dx*dx + dy*dy);
			if(distance < center_clearance) {
				if(distance < 0.01) {
					double angle = (i/2)*2.094395102393195 + ((i & 1) ? 3.141592653589793 : 0.0) - 0.7853981633974483;
					dx = std::cos(angle); dy = std::sin(angle); distance = 1;
				}
				positions[i] = Pointf(dx*center_clearance/distance, dy*center_clearance/distance);
			}
		}
		for(int i = 0; i < 6; ++i) for(int j = i+1; j < 6; ++j) {
			double dx = positions[j].x - positions[i].x, dy = positions[j].y - positions[i].y;
			double distance = std::sqrt(dx*dx + dy*dy);
			if(distance >= pair_clearance) continue;
			if(distance < 0.01) { dx = 1; dy = (i & 1) ? -1 : 1; distance = std::sqrt(2.0); }
			double shift = (pair_clearance - distance)*0.5;
			positions[i].x -= dx*shift/distance; positions[i].y -= dy*shift/distance;
			positions[j].x += dx*shift/distance; positions[j].y += dy*shift/distance;
		}
		for(int i = 0; i < 6; ++i) {
			positions[i].x = std::max(-length, std::min(length, positions[i].x));
			positions[i].y = std::max(-length, std::min(length, positions[i].y));
		}
	}
	for(int i = 0; i < 6; ++i)
		axes[i].point = center + Point(int(std::lround(positions[i].x)), int(std::lround(positions[i].y)));
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return axes[a].depth < axes[b].depth; });
	Refresh();
}

int Ui3DViewport::Impl::AxisCtrl::Hit(Point point) const
{
	for(int i = 5; i >= 0; --i) {
		Point delta = point - axes[order[i]].point;
		if(delta.x*delta.x + delta.y*delta.y <= radius*radius) return order[i];
	}
	Point delta = point - center;
	return delta.x*delta.x + delta.y*delta.y <= center_radius*center_radius ? 6 : -1;
}

void Ui3DViewport::Impl::AxisCtrl::Paint(Draw& draw)
{
	if(!impl) return;
	++impl->chrome_stats.axis_paints;
	Ui3DViewportColors colors(impl->dark);
	colors.viewport = impl->overlay_background;
	const Color axis_colors[] = { Color(218, 103, 110), Color(127, 185, 112), Color(104, 157, 219) };
	const char *labels[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
	Font font = StdFont().Height(DPI(10)).Bold();
	for(int index : order) {
		AxisPoint axis = axes[index];
		Color color = axis_colors[index/2];
		if(axis.depth < -0.05) color = Blend(colors.viewport, color, 145);
		draw.DrawLine(center, axis.point, max(1, DPI(index & 1 ? 1 : 2)), color);
	}
	for(int index : order) {
		Point point = axes[index].point;
		Color color = axis_colors[index/2];
		Color face = axes[index].depth < -0.05 ? Blend(colors.control, color, 85) : color;
		bool focus = HasFocus() && key_part == index;
		Color frame = (hot == index || focus) ? colors.ink : Blend(color, Black(), 45);
		if(pressed == index) face = Blend(face, colors.ink, 60);
		draw.DrawEllipse(point.x-radius, point.y-radius, 2*radius+1, 2*radius+1,
		                 face, max(1, DPI(focus ? 2 : 1)), frame);
		Size text_size = GetTextSize(labels[index], font);
		draw.DrawText(point.x-text_size.cx/2, point.y-text_size.cy/2,
		              labels[index], font, axes[index].depth < -0.05 ? colors.ink : Color(24, 33, 41));
	}
	bool center_focus = HasFocus() && key_part == 6;
	draw.DrawEllipse(center.x-center_radius, center.y-center_radius,
	                 2*center_radius+1, 2*center_radius+1, colors.control,
	                 max(1, DPI(center_focus ? 2 : 1)), hot == 6 || center_focus ? colors.ink : colors.muted);
	const char *projection = impl->view.GetCamera().orthographic ? "O" : "P";
	Size text_size = GetTextSize(projection, font);
	draw.DrawText(center.x-text_size.cx/2, center.y-text_size.cy/2, projection, font, colors.ink);
}

Image Ui3DViewport::Impl::AxisCtrl::MouseEvent(int event, Point point, int delta, dword flags)
{
	if(event == MOUSEMOVE && Hit(point) < 0 && hot >= 0) MouseMove(point, flags);
	if(impl && !HasCapture() && (Hit(point) < 0 || event == MOUSEWHEEL || event == MOUSEHWHEEL ||
	   ((event & BUTTON) != LEFT && (event & BUTTON) != 0)))
		return impl->ForwardToView(*this, event, point, delta, flags);
	return Ctrl::MouseEvent(event, point, delta, flags);
}

void Ui3DViewport::Impl::AxisCtrl::LeftDown(Point point, dword)
{
	Ptr<AxisCtrl> alive = this;
	SetFocus();
	if(!alive) return;
	pressed = Hit(point); key_part = pressed >= 0 ? pressed : key_part;
	if(pressed >= 0) SetCapture();
	if(alive) alive->Refresh();
}

void Ui3DViewport::Impl::AxisCtrl::LeftUp(Point point, dword)
{
	Ptr<AxisCtrl> alive = this;
	int action = pressed;
	pressed = -1;
	ReleaseCapture();
	if(!alive) return;
	if(action >= 0) Refresh();
	if(impl && action >= 0 && Hit(point) == action) impl->AxisCommand(action);
}

void Ui3DViewport::Impl::AxisCtrl::MouseMove(Point point, dword)
{
	int next = Hit(point);
	if(next == hot) return;
	hot = next;
	if(hot >= 0 && hot < 6)
		Tip(String(Ui3DViewportDirectionName(Ui3DViewportAxisDirection(hot))) +
		    " view; click again for the opposite view");
	else if(hot == 6) Tip("Toggle perspective / orthographic projection");
	else Tip("");
	Refresh();
}

bool Ui3DViewport::Impl::AxisCtrl::Key(dword key, int count)
{
	if(key == K_LEFT || key == K_UP || key == K_RIGHT || key == K_DOWN) {
		key_part = (key_part + ((key == K_LEFT || key == K_UP) ? 6 : 1)) % 7;
		Refresh(); return true;
	}
	if(key == K_ENTER || key == K_SPACE) { if(impl) impl->AxisCommand(key_part); return true; }
	if(key == K_ESCAPE && impl) { impl->view.SetFocus(); return true; }
	return Ctrl::Key(key, count);
}

void Ui3DViewport::Impl::SyncNativeOverlay()
{
#ifdef GUI_WIN
	if(syncing_native || !overlay.GetHWND()) return;
	syncing_native = true;
	HWND hwnd = overlay.GetHWND();
	uint64 key = 1469598103934665603ULL;
	auto add_key = [&](int value) { key = (key ^ uint64(unsigned(value))) * 1099511628211ULL; };
	auto add_rect = [&](const Rect& rect) {
		add_key(rect.left); add_key(rect.top); add_key(rect.right); add_key(rect.bottom);
	};
	Point panel_origin = overlay_panel.GetRect().TopLeft();
	add_rect(overlay.GetRect());
	add_key(overlay_panel.IsShown()); add_key(panel_origin.x); add_key(panel_origin.y);
	add_key(axes.IsShown()); add_rect(axes.GetRect());
	add_key(DPI(12)); add_key(DPI(7));
	UiToolButton *nav[] = { &orbit, &pan, &zoom_in, &zoom_out, &fit };
	for(UiToolButton *button : nav) {
		Rect rect = button->GetRect();
		add_key(button->IsShown()); add_rect(rect);
	}
	AnnotationLabel *labels[] = { &projection_label, &view_label, &units_label, &mouse_hint };
	for(AnnotationLabel *label : labels) {
		Rect rect = label->GetRect();
		add_key(label->IsShown()); add_rect(rect);
	}
	if(region_hwnd == hwnd && region_key == key) {
		syncing_native = false; return;
	}
	// U++ dispatches input through the top window. A native enabled child would
	// divert it from the ordinary Ctrl descendants used by this facade.
	if(IsWindowEnabled(hwnd)) EnableWindow(hwnd, FALSE);
	HRGN shape = CreateRectRgn(0, 0, 0, 0);
	if(!shape) { syncing_native = false; return; }
	auto add_region = [&](HRGN part) { if(part) { CombineRgn(shape, shape, part, RGN_OR); DeleteObject(part); } };
	if(overlay_panel.IsShown() && axes.IsShown()) {
		// Rotation changes pixels in this small buffered island, never the HWND
		// footprint. Background pixels are composed with the axes before Put().
		Rect rect = axes.GetRect() + panel_origin;
		add_region(CreateRoundRectRgn(rect.left, rect.top, rect.right, rect.bottom, DPI(12), DPI(12)));
	}
	for(UiToolButton *button : nav) if(overlay_panel.IsShown() && button->IsShown()) {
		Rect r = button->GetRect() + panel_origin;
		add_region(CreateRoundRectRgn(r.left, r.top, r.right+1, r.bottom+1, DPI(7), DPI(7)));
	}
	for(AnnotationLabel *label : labels) if(label->IsShown()) {
		Rect r = label->GetRect();
		add_region(CreateRectRgn(r.left, r.top, r.right, r.bottom));
	}
	// On success Windows owns this stable union of bounded islands. Geometry
	// remains visible outside the gizmo, buttons and fixed annotation slots.
	if(SetWindowRgn(hwnd, shape, TRUE)) {
		region_hwnd = hwnd; region_key = key;
		++chrome_stats.region_updates;
	}
	else DeleteObject(shape);
	// Creation/layout can change native sibling order. Ordinary camera refreshes
	// hit the geometry cache above and must not reorder all four pane overlays.
	if(GetWindow(hwnd, GW_HWNDPREV) &&
	   SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
		++chrome_stats.z_order_updates;
	syncing_native = false;
#endif
}

Ui3DViewport::Ui3DViewport()
{
	impl.Create(*this);
	NoWantFocus();
	SetSizeMin(DPI(175), DPI(120));
	impl->RefreshChrome();
}

Ui3DViewport::~Ui3DViewport() {}
Ui3DView& Ui3DViewport::View() { return impl->view; }
const Ui3DView& Ui3DViewport::View() const { return impl->view; }

Ui3DViewport& Ui3DViewport::SetActive(bool active)
{
	if(impl->active != active) { impl->active = active; Refresh(); }
	return *this;
}

Ui3DViewport& Ui3DViewport::SetDark(bool dark)
{
	if(impl->dark != dark) {
		Ptr<Ui3DViewport> alive = this;
		impl->dark = dark; impl->ApplyStyle();
		if(alive) alive->RefreshChrome();
		if(alive) alive->Refresh();
	}
	return *this;
}

Ui3DViewport& Ui3DViewport::ShowToolbar(bool show)
{
	if(impl->toolbar_visible != show) {
		Ptr<Ui3DViewport> alive = this;
		impl->toolbar_visible = show; Layout();
		if(alive) alive->Refresh();
	}
	return *this;
}

Ui3DViewport& Ui3DViewport::ShowOrientation(bool show)
{
	if(impl->orientation_visible != show) { impl->orientation_visible = show; Layout(); }
	return *this;
}

Ui3DViewport& Ui3DViewport::ShowNavigation(bool show)
{
	if(impl->navigation_visible != show) { impl->navigation_visible = show; Layout(); }
	return *this;
}

Ui3DViewport& Ui3DViewport::ShowAnnotations(bool show)
{
	if(impl->annotations_visible != show) { impl->annotations_visible = show; Layout(); }
	return *this;
}

Ui3DViewport& Ui3DViewport::SetCompact(bool compact)
{
	if(impl->compact != compact) { impl->compact = compact; Layout(); }
	return *this;
}

Ui3DViewport& Ui3DViewport::SetCameraLabel(const String& label)
{
	if(impl->camera_label != label) { impl->camera_label = label; Layout(); }
	return *this;
}

Ui3DViewport& Ui3DViewport::SetSizeMin(Size size)
{
	size = Size(max(0, size.cx), max(0, size.cy));
	if(impl->requested_minimum != size) {
		Ptr<Ui3DViewport> alive = this;
		impl->requested_minimum = size;
		RefreshLayout();
		if(alive) alive->Refresh();
	}
	return *this;
}

void Ui3DViewport::RefreshChrome() { impl->RefreshChrome(); }
bool Ui3DViewport::IsToolbarShown() const { return impl->toolbar_visible; }
bool Ui3DViewport::IsOrientationShown() const { return impl->orientation_visible; }
bool Ui3DViewport::IsNavigationShown() const { return impl->navigation_visible; }
bool Ui3DViewport::AreAnnotationsShown() const { return impl->annotations_visible; }
bool Ui3DViewport::IsCompact() const { return impl->compact; }
bool Ui3DViewport::IsActive() const { return impl->active; }
bool Ui3DViewport::IsDark() const { return impl->dark; }
Ui3DViewportChromeStats Ui3DViewport::GetChromeStats() const
{
	return impl ? impl->chrome_stats : Ui3DViewportChromeStats();
}
void Ui3DViewport::Layout() { if(impl) impl->Layout(); }
Size Ui3DViewport::GetMinSize() const
{
	Size requested = impl ? impl->requested_minimum : Size(0, 0);
	return Size(max(DPI(175), requested.cx), max(DPI(120), requested.cy));
}

void Ui3DViewport::Paint(Draw& draw)
{
	if(!impl) return;
	Ui3DViewportColors colors(impl->dark);
	draw.DrawRect(GetSize(), colors.panel);
	Color frame = impl->active ? colors.border : colors.control;
	Size size = GetSize(); int width = max(1, DPI(1));
	draw.DrawRect(0, 0, size.cx, width, frame);
	draw.DrawRect(0, max(0, size.cy-width), size.cx, width, frame);
	draw.DrawRect(0, 0, width, size.cy, frame);
	draw.DrawRect(max(0, size.cx-width), 0, width, size.cy, frame);
}

void Ui3DViewport::ChildGotFocus()
{
	Ptr<Ui3DViewport> alive = this;
	UiPanel::ChildGotFocus();
	if(alive && alive->impl) { Event<> event = alive->WhenActivate; event(); }
}

void Ui3DViewport::LeftDown(Point, dword)
{
	Event<> event = WhenActivate; event();
}

void Ui3DViewport::State(int reason)
{
	Ptr<Ui3DViewport> alive = this;
	UiPanel::State(reason);
	if(alive && alive->impl && reason != CLOSE) alive->impl->SyncNativeOverlay();
}

}
