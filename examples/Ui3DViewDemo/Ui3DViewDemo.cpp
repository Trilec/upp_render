#include "Ui3DViewDemo.h"
#include <Ui/UiIcons.h>
#include <plugin/png/png.h>
#include <windows.h>
#include <algorithm>
#include <cmath>

namespace Upp {
namespace {

constexpr double ViewerPi = 3.14159265358979323846;
const char *ViewerObjectNames[] = { "Blue cube", "Orange cube", "Green column" };

String ViewerDirectionName(const Scene3DCamera& camera)
{
	switch(camera.GetViewDirection()) {
	case Scene3DViewDirection::Front: return "Front";
	case Scene3DViewDirection::Back: return "Back";
	case Scene3DViewDirection::Right: return "Right";
	case Scene3DViewDirection::Left: return "Left";
	case Scene3DViewDirection::Top: return "Top";
	case Scene3DViewDirection::Bottom: return "Bottom";
	default: return camera.orthographic ? "User orthographic" : "Perspective";
	}
}

void ViewerPalette(StyledPalette& palette, Color face, Color ink, Color border)
{
	for(int state = 0; state < 4; ++state) {
		palette.face[state] = face;
		palette.ink[state] = palette.icon[state] = ink;
		palette.frame[state] = border;
	}
}

float ViewerLinear(byte channel)
{
	const float value = channel / 255.0f;
	return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

}

Ui3DViewDemo::Ui3DViewDemo()
{
	Title("Ui3DView - Viewport control");
	Sizeable().Zoomable();
	SetRect(60, 35, DPI(1222), DPI(773));
	SetMinSize(Size(DPI(720), DPI(600)));
	BuildLayout();
	ConnectEvents();
	ResetDemo();
}

Ui3DViewDemo::~Ui3DViewDemo()
{
	// Member controls outlive the application callbacks they capture. Disconnect
	// before parent/member destruction can deliver focus/capture cancellation.
	for(auto& pane : panes) {
		pane.View().WhenNavigation.Clear();
		pane.View().WhenSelectionRequest.Clear();
		pane.WhenActivate.Clear();
		pane.WhenCameraCommand.Clear();
		pane.WhenViewOptions.Clear();
		pane.WhenMaximize.Clear();
		pane.PopulateCameraMenu.Clear();
	}
}

void Ui3DViewDemo::BuildLayout()
{
	Add(shell.SizePos());
	shell.Add(root.SizePos());
	root.SetGap(0).SetInset(0);
	root.Add(header).Fixed(DPI(64));
	root.Add(toolbar).Fixed(DPI(45));
	root.Add(body).Expand();
	root.Add(status_panel).Fixed(DPI(29));
	header.Add(header_row.SizePos());
	header_row.SetInset(Rect(DPI(16), DPI(7), DPI(12), DPI(7))).SetGap(DPI(10));
	logo.SetIcon(ViewerCubeGlyph(Color(145,180,207), DPI(26)));
	header_row.Add(logo).Fixed(DPI(38));
	header_row.Add(title_column).Expand();
	title_column.SetGap(0);
	title_column.Add(brand_row).Fixed(DPI(27));
	brand_row.SetGap(DPI(10));
	brand_row.Add(title).Fixed(DPI(108));
	brand_row.Add(badge).Fixed(DPI(126));
	brand_row.AddSpacer();
	subtitle_item = title_column.Add(subtitle).Expand();
	title.SetText("Ui3DView");
	subtitle.SetText("A native viewport for your applications");
	badge.SetText("VIEWPORT CONTROL");
	for(UiToolButton *button : { &btn_theme, &btn_help })
		header_row.Add(*button).Fit().AlignSelf(UiCrossAlign::Center).MinMaxCross(DPI(30), DPI(30));
	btn_theme.SetText("Light").SetIcon(ICON_ACTION_LIGHT_MODE_48()).Tip("Switch between light and dark themes");
	btn_help.SetText("Help").SetIcon(ICON_DESIGN_HELP_48()).Tip("Navigation and keyboard shortcuts");
	btn_inspector.SetIcon(ICON_DESIGN_RIGHT_PANEL_CLOSE_48()).SetCheckable().SetChecked();
	btn_inspector.Tip("Show or hide the inspector (I)");
	header_row.Add(btn_inspector).Fixed(DPI(30)).AlignSelf(UiCrossAlign::Center).MinMaxCross(DPI(30), DPI(30));
	toolbar.Add(tools.SizePos());
	tools.SetInset(Rect(DPI(12), DPI(7), DPI(12), DPI(7))).SetGap(DPI(4));
	sample_scene.SetText("Sample scene  03").SetIcon(ViewerCubeGlyph(Color(170,181,193)));
	tools.Add(sample_scene).Fixed(DPI(162));
	btn_select.Tip("Select objects (V)");
	btn_orbit.Tip("Orbit with the left mouse button (O)");
	btn_pan.Tip("Pan with the left mouse button (H)");
	btn_single.SetText("Single").SetIcon(ICON_DESIGN_PANEL_48()).SetCheckable();
	btn_quad.SetText("Four views").SetIcon(ICON_DESIGN_GRID_LAYOUT_48()).SetCheckable();
	btn_single.Tip("Show the active view (Q)");
	btn_quad.Tip("Four independent views (Q)");
	tools.Add(btn_single).Fit(); tools.Add(btn_quad).Fit();
	tools.AddSpacer(0).Fixed(DPI(12));
	for(UiToolButton *button : { &btn_select, &btn_orbit, &btn_pan }) {
		button->SetCheckable(); tools.Add(*button).Fixed(DPI(30));
	}
	tools.AddSpacer();
	btn_frame_all.SetText("Frame all").Tip("Frame all visible scene objects (Home)");
	btn_frame_selection.SetText("Frame selected").Tip("Frame the selected objects (F)");
	frame_selection_item = tools.Add(btn_frame_selection).Fit();
	tools.Add(btn_frame_all).Fit();
	body.SetGap(DPI(9)).SetInset(Rect(DPI(12), DPI(10), DPI(12), 0));
	body.Add(views).Expand();
	inspector_item = body.Add(inspector).Fixed(DPI(246));
	views.Set(panes[0], panes[1], panes[2], panes[3]);
	views.SetColumnSplitPercent(50).SetRowSplitPercent(50);
	for(int i = 0; i < 4; ++i) views.SetMinPixels(i, DPI(150));
	inspector.Add(inspector_root.SizePos());
	inspector_root.SetGap(0);
	inspector_root.Add(inspector_header).Fixed(DPI(35));
	inspector_root.Add(inspector_scroll).Expand();
	inspector_header.Add(inspector_title_row.SizePos());
	inspector_title_row.SetInset(Rect(DPI(13),0,DPI(13),0)).SetGap(DPI(4));
	inspector_title.SetText("Inspector"); inspector_badge.SetText("ACTIVE VIEW").SetAlignH(UiAlign::RIGHT);
	inspector_title_row.Add(inspector_title).Expand(); inspector_title_row.Add(inspector_badge).Fixed(DPI(80));
	inspector_scroll.SetScrollMode(UIPANELSCROLL_VERTICAL);
	inspector_scroll.Content().Add(inspector_column);
	inspector_column.SetGap(DPI(3)).SetInset(0);
	heading_view.SetText("VIEW");
	inspector_column.Add(view_heading).Fixed(DPI(24));
	view_heading.Add(heading_view).Expand();
	btn_reset.SetIcon(ICON_CONTENT_UNDO_OUTLINED_48()).Tip("Reset the active view to its original preset");
	view_heading.Add(btn_reset).Fixed(DPI(24));
	inspector_column.Add(view_name).Fixed(DPI(21));
	inspector_column.Add(view_detail).Fixed(DPI(21));
	inspector_column.Add(camera_binding).Fixed(DPI(21));
	inspector_column.Add(lens_row).Fixed(DPI(28));
	lens_row.SetGap(DPI(6));
	lens_row.Add(lens_label).Fixed(DPI(36));
	lens_row.Add(lens).Expand();
	lens.SetFieldWidth(DPI(45)).SetGap(DPI(5));
	// Horizontal Expand starts from the control's preferred width. The compact
	// inspector has 170 px minus its 36 px label and 6 px gap for this composite.
	lens.SetSizeMin(DPI(128), DPI(28));
	lens.Field().ShowSpin(false);
	inspector_column.AddSpacer(0).Fixed(DPI(13));
	inspector_column.Add(camera_heading).Fixed(DPI(25));
	camera_heading.Add(heading_cameras).Expand();
	heading_cameras.SetText("CAMERAS  2");
	btn_save.SetText("+").Tip("Save the active view as a new camera");
	camera_heading.Add(btn_save).Fixed(DPI(26));
	camera_list.EnableRenameOnDblClick(false).ShowDragHandle(false);
	inspector_column.Add(camera_list).Fixed(DPI(86));
	btn_camera_undo.SetText("Undo").SetIcon(ICON_CONTENT_UNDO_OUTLINED_48());
	btn_camera_undo.Tip("Undo the last camera update or removal");
	inspector_column.AddSpacer(0).Fixed(DPI(13));
	heading_scene.SetText("SCENE  3 visible");
	inspector_column.Add(scene_heading).Fixed(DPI(24));
	scene_heading.Add(heading_scene).Expand();
	btn_show_all.SetText("Show all").Tip("Show every object");
	show_all_item = scene_heading.Add(btn_show_all).Fixed(DPI(55));
	object_list.EnableRenameOnDblClick(false).ShowDragHandle(false);
	inspector_column.Add(object_list).Fixed(DPI(92));
	inspector_column.Add(selection_panel).Fixed(DPI(56));
	selection_panel.Add(selection_column.SizePos());
	selection_column.SetInset(DPI(9)).SetGap(DPI(3));
	selection_column.Add(selection_label).Expand(); selection_column.Add(selection_hint).Expand();
	selection_hint.SetText("Click an object in any view.");
	inspector_column.AddSpacer(0).Fixed(DPI(13));
	heading_navigation.SetText("NAVIGATION");
	inspector_column.Add(heading_navigation).Fixed(DPI(24));
	nav_text.SetText("Middle drag         Orbit\nShift + middle      Pan\nWheel                   Zoom\nF / Home             Frame\n1 / 3 / 7               Axis view\n5                           Projection\nEsc                       Cancel drag");
	nav_text.SetAlignV(UiAlign::TOP);
	inspector_column.Add(nav_text).Fixed(DPI(126));
	status_panel.Add(status_row.SizePos());
	status_row.SetInset(Rect(DPI(12), 0, DPI(12), 0)).SetGap(DPI(10));
	status_row.Add(status_text).Expand(2);
	undo_item = status_row.Add(btn_camera_undo).Fixed(DPI(62));
	status_row.Add(status_detail).Expand();
	status_detail.SetAlignH(UiAlign::RIGHT);
}

void Ui3DViewDemo::ConnectEvents()
{
	btn_theme.WhenAction = [this] { SetLight(!light); };
	btn_help.WhenAction = [this] { ShowHelp(); };
	btn_inspector.WhenAction = [this] { SetInspectorVisible(!inspector_visible); };
	btn_select.WhenAction = [this] { SetTool(Ui3DNavigationTool::Select); };
	btn_orbit.WhenAction = [this] { SetTool(Ui3DNavigationTool::Orbit); };
	btn_pan.WhenAction = [this] { SetTool(Ui3DNavigationTool::Pan); };
	btn_single.WhenAction = [this] { SetQuad(false); };
	btn_quad.WhenAction = [this] { SetQuad(true); };
	btn_frame_all.WhenAction = [this] { Frame(false); };
	btn_frame_selection.WhenAction = [this] { Frame(true); };
	btn_reset.WhenAction = [this] { ResetView(); };
	btn_save.WhenAction = [this] { SaveCamera(); };
	btn_camera_undo.WhenAction = [this] { UndoCamera(); };
	btn_show_all.WhenAction = [this] { ShowAll(); };
	camera_list.WhenItemAction = [this](const String& id) { OpenCameraActions(id); };
	object_list.WhenItemAction = [this](const String& id) { ToggleVisibility(id); };
	camera_list.WhenSelection = [this] {
		if(syncing) return;
		const int index = camera_list.GetCursor();
		if(index >= 0 && index < camera_list.Model().GetCount()) {
			const String id = AsString(camera_list.Model().Get(index).data);
			LoadCamera(id, active);
		}
	};
	camera_list.WhenAction = [this] {
		if(syncing) return;
		const int index = camera_list.GetCursor();
		if(index >= 0 && index < camera_list.Model().GetCount())
			LoadCamera(AsString(camera_list.Model().Get(index).data), active);
	};
	object_list.WhenSelection = [this] {
		if(syncing) return;
		selection.clear();
		for(int row : object_list.GetSelection())
			if(row >= 0 && row < (int)scene_description.objects.size() && scene_description.objects[row].visible)
				selection.push_back(scene_description.objects[row].id);
		SyncSelection();
		SyncChrome(false);
	};
	object_list.WhenAction = [this] { if(!syncing) Frame(true); };
	lens.WhenBeginEdit = [this] {
		if(syncing) return;
		CancelGestures();
		String error;
		if(cameras.BeginPreview(active, error)) lens_preview_pane = active;
		else SetStatus(error);
	};
	lens.WhenChanging = [this] { ChangeLens(false); };
	lens.WhenAction = [this] { ChangeLens(true); };
	lens.WhenCancelEdit = [this] {
		if(lens_preview_pane < 0) return;
		String error;
		cameras.CancelPreview(lens_preview_pane, error);
		lens_preview_pane = -1;
		SyncCameras(); SyncChrome(); SetStatus("Lens change cancelled");
	};
	for(int i = 0; i < 4; ++i) {
		auto& pane = panes[i];
		pane.WhenActivate = [this, i] { ActivatePane(i); };
		pane.WhenMaximize = [this, i] { ActivatePane(i); SetQuad(!quad); };
		pane.WhenCameraCommand = [this, i] { CameraChanged(i); };
		pane.WhenViewOptions = [this, i] {
			if(panes[i].View().GetNavigationTool() != navigation_tool)
				SetTool(panes[i].View().GetNavigationTool());
			else SyncChrome(false);
		};
		pane.PopulateCameraMenu = [this, i](UiMenu& menu) { PopulateCameraMenu(menu, i); };
		pane.View().WhenNavigation = [this, i](const Ui3DNavigationEvent& event) { Navigate(i, event); };
		pane.View().WhenSelectionRequest = [this, i](const Ui3DSelectionRequest& request) { Select(i, request); };
	}
}

void Ui3DViewDemo::BuildScene()
{
	String error;
	scene_description.objects.clear();
	++scene_description.revision;
	const Scene3DVec3 sizes[] = { {2.1,2.1,2.1}, {1.8,1.8,1.8}, {0.86,3.5,0.86} };
	const Scene3DVec3 positions[] = { {-1.45,1.05,-0.45}, {0.65,0.9,1.35}, {2.05,1.75,-1.4} };
	const Color colors[] = { Color(105,168,189), Color(207,146,91), Color(149,174,142) };
	const char *ids[] = { "box-blue", "box-orange", "column-green" };
	for(int i = 0; i < 3; ++i) {
		Scene3DObjectDesc object;
		object.id = ids[i];
		object.mesh = Scene3DMesh::MakeBox(sizes[i], error);
		if(!object.mesh) { SetStatus(error); return; }
		object.local = Scene3DMatrix::Translation(positions[i]);
		object.r = ViewerLinear(colors[i].GetR());
		object.g = ViewerLinear(colors[i].GetG());
		object.b = ViewerLinear(colors[i].GetB());
		scene_description.objects.push_back(object);
	}
	if(!PublishScene(error)) SetStatus(error);
}

bool Ui3DViewDemo::PublishScene(String& error)
{
	auto candidate = Scene3DSnapshot::Create(scene_description, error);
	if(!candidate) return false;
	CancelGestures();
	for(auto& pane : panes) if(!pane.View().SetScene(candidate, error)) return false;
	scene = std::move(candidate);
	return true;
}

void Ui3DViewDemo::ResetDemo()
{
	CancelGestures();
	Scene3DCamera perspective;
	perspective.target = Scene3DVec3(0, 1, 0);
	perspective.yaw = .68; perspective.pitch = .54; perspective.distance = 14.6;
	perspective.ortho_height = 8.8; perspective.fov_y = ViewerPi / 4;
	defaults.fill(perspective);
	const Scene3DViewDirection directions[] = { Scene3DViewDirection::Front,
		Scene3DViewDirection::Top, Scene3DViewDirection::Right };
	for(int i = 0; i < 3; ++i) {
		defaults[i].AlignTo(directions[i]);
		defaults[i].orthographic = true;
		defaults[i].ortho_height = 8.8;
	}
	String error, id;
	cameras.Reset(defaults, error);
	cameras.AddBookmark("Camera 01", perspective, id, error);
	selected_camera_id = overview_camera_id = id;
	Scene3DCamera low = perspective;
	low.yaw = -.5; low.pitch = .2; low.distance = 13.5; low.target = Scene3DVec3(0, 1.5, 0);
	cameras.AddBookmark("Camera 02", low, id, error);
	low_camera_id = id;
	selection.clear(); active = 3;
	BuildScene();
	for(int i = 0; i < 4; ++i) {
		auto options = light ? Ui3DViewOptions::Light() : Ui3DViewOptions::Dark();
		options.grid_plane = i == 0 ? Ui3DGridPlane::XY : i == 2 ? Ui3DGridPlane::YZ : Ui3DGridPlane::XZ;
		panes[i].View().SetOptions(options, error);
		panes[i].ShowToolbar().ShowOrientation().ShowNavigation();
	}
	SyncCameras();
	SyncSelection();
	SetTool(Ui3DNavigationTool::Select);
	SetQuad(false);
	ApplyTheme();
	SyncChrome();
	SetStatus("Ready - select an object or explore the view");
}

void Ui3DViewDemo::Layout()
{
	TopWindow::Layout();
	const int width = GetSize().cx;
	const int side = width < DPI(800) ? DPI(202) : width < DPI(1000) ? DPI(214) :
	                 width < DPI(1300) ? DPI(226) : DPI(246);
	inspector_item.Fixed(side).Visible(inspector_visible);
	subtitle_item.Visible(width >= DPI(800));
	// The narrow layout keeps every command reachable; only redundant text goes.
	btn_single.SetText(width < DPI(900) ? "" : "Single");
	btn_quad.SetText(width < DPI(900) ? "" : "Four views");
	tools.SetItemVisible(0, width >= DPI(900));
	const int content_width = max(1, side - DPI(32));
	inspector_column.SetRect(DPI(12), DPI(10), content_width,
	                         inspector_column.MeasureHeightForWidth(content_width));
	inspector_scroll.Layout();
	for(auto& pane : panes) pane.SetCompact(quad);
}

void Ui3DViewDemo::ApplyTheme()
{
	UiTheme::Set(UiThemePreset::Minimal, light ? UiThemeMode::Light : UiThemeMode::Dark);
	const Color window = light ? Color(233,237,240) : Color(24,29,34);
	const Color panel = light ? Color(246,247,248) : Color(32,38,45);
	const Color bar = light ? Color(241,243,245) : Color(32,38,44);
	const Color control = light ? Color(232,237,241) : Color(43,50,58);
	const Color ink = light ? Color(39,50,60) : Color(228,233,238);
	const Color muted = light ? Color(82,98,114) : Color(170,181,193);
	const Color accent = light ? Color(52,95,128) : Color(145,180,207);
	const Color section = light ? Color(83,110,76) : Color(185,204,174);
	const Color line = light ? Color(204,213,220) : Color(54,65,75);
	logo.SetIcon(ViewerCubeGlyph(accent, DPI(26)));
	sample_scene.SetIcon(ViewerCubeGlyph(muted));
	auto panel_style = UiTheme::ResolvePanel(UiRole::Standard);
	panel_style.metrics.radius = 0; panel_style.metrics.frame_width = 0;
	panel_style.metrics.content_margin = Rect(0,0,0,0);
	panel_style.metrics.focus_enabled = panel_style.metrics.shadow.enabled = false;
	ViewerPalette(panel_style.palette, window, ink, line); shell.SetCustomStyle(panel_style);
	ViewerPalette(panel_style.palette, window, ink, line); header.SetCustomStyle(panel_style);
	ViewerPalette(panel_style.palette, bar, ink, line);
	toolbar.SetCustomStyle(panel_style); status_panel.SetCustomStyle(panel_style); inspector_header.SetCustomStyle(panel_style);
	ViewerPalette(panel_style.palette, panel, ink, line); inspector.SetCustomStyle(panel_style);
	ViewerPalette(panel_style.palette, window, muted, line);
	panel_style.metrics.frame_width = DPI(1); panel_style.metrics.radius = DPI(4);
	selection_panel.SetCustomStyle(panel_style);
	auto scroll_style = UiTheme::ResolveScrollPanel();
	ViewerPalette(scroll_style.palette, panel, ink, line);
	scroll_style.metrics.radius = 0; scroll_style.metrics.frame_width = 0;
	scroll_style.metrics.content_margin = Rect(0,0,0,0);
	inspector_scroll.SetCustomStyle(scroll_style);
	auto label_style = UiTheme::ResolveLabel(UiRole::Standard);
	label_style.font = SansSerifZ(12);
	label_style.metrics.content_margin = Rect(0,0,0,0);
	ViewerPalette(label_style.palette, panel, ink, line);
	for(UiLabel *label : { &view_name, &view_detail, &camera_binding, &lens_label, &selection_label })
		label->SetCustomStyle(label_style);
	sample_scene.SetCustomStyle(label_style);
	inspector_title.SetCustomStyle(label_style);
	for(int i = 0; i < 4; ++i) label_style.palette.ink[i] = muted;
	label_style.font = SansSerifZ(11);
	subtitle.SetCustomStyle(label_style); nav_text.SetCustomStyle(label_style);
	selection_hint.SetCustomStyle(label_style);
	status_text.SetCustomStyle(label_style); status_detail.SetCustomStyle(label_style);
	label_style.font = SansSerifZ(11).Bold();
	for(int i = 0; i < 4; ++i) label_style.palette.ink[i] = section;
	for(UiLabel *label : { &heading_view, &heading_cameras, &heading_scene, &heading_navigation })
		label->SetCustomStyle(label_style);
	label_style.font = SansSerifZ(10);
	for(int i = 0; i < 4; ++i) label_style.palette.ink[i] = muted;
	badge.SetCustomStyle(label_style);
	inspector_badge.SetCustomStyle(label_style);
	label_style.font = SansSerifZ(20).Bold();
	for(int i = 0; i < 4; ++i) label_style.palette.ink[i] = ink;
	title.SetCustomStyle(label_style);
	auto tool_style = UiTheme::ResolveToolButton(UiRole::Standard);
	tool_style.metrics.text_font = SansSerifZ(12); tool_style.metrics.use_text_font = true;
	tool_style.metrics.radius = DPI(4); tool_style.metrics.content_margin = Rect(DPI(8),0,DPI(8),0);
	tool_style.metrics.frame_width = DPI(1); tool_style.metrics.focus_color = accent;
	ViewerPalette(tool_style.palette, bar, muted, bar);
	tool_style.palette.face[1] = control; tool_style.palette.face[2] = control;
	tool_style.palette.frame[1] = line; tool_style.palette.frame[2] = accent;
	tool_style.palette.ink[2] = tool_style.palette.icon[2] = accent;
	for(UiToolButton *button : { &btn_theme, &btn_help, &btn_inspector, &btn_select,
		&btn_orbit, &btn_pan, &btn_single, &btn_quad, &btn_frame_all, &btn_frame_selection, &btn_save,
		&btn_reset, &btn_camera_undo, &btn_show_all }) {
		button->SetCustomStyle(tool_style);
		button->SetIconSize(DPI(16), DPI(16)).SetIconRenderMode(UiIconRenderMode::MonoTint)
		       .SetContentGap(DPI(5)).SetContentInset(Rect(DPI(6),0,DPI(6),0));
	}
	auto list_style = UiTheme::ResolveList();
	ViewerPalette(list_style.palette, panel, ink, line);
	list_style.metrics.radius = DPI(4); list_style.metrics.frame_width = 0;
	list_style.metrics.content_margin = Rect(0,0,0,0);
	list_style.font = SansSerifZ(12); list_style.row_height = DPI(38);
	list_style.ink = ink; list_style.muted_ink = muted;
	list_style.hot_face = control; list_style.hot_frame = line; list_style.hot_ink = ink;
	list_style.selected_face = control; list_style.selected_frame = accent; list_style.selected_ink = accent;
	list_style.show_drag_handle = false; list_style.show_checks = true;
	list_style.h_padding = DPI(3); list_style.v_padding = DPI(3);
	camera_list.SetCustomStyle(list_style);
	list_style.row_height = DPI(29); object_list.SetCustomStyle(list_style);
	btn_theme.SetIcon(light ? ICON_ACTION_DARK_MODE_48() : ICON_ACTION_LIGHT_MODE_48());
	btn_theme.SetText(light ? "Dark" : "Light");
	btn_select.SetIcon(ViewerToolGlyph(0, muted));
	btn_orbit.SetIcon(ViewerToolGlyph(1, muted));
	btn_pan.SetIcon(ViewerToolGlyph(2, muted));
	btn_frame_all.SetIcon(ViewerToolGlyph(3, muted));
	btn_frame_selection.SetIcon(ViewerToolGlyph(3, muted));
	for(auto& pane : panes) {
		pane.SetDark(!light);
		auto options = pane.View().GetOptions();
		const auto palette = light ? Ui3DViewOptions::Light() : Ui3DViewOptions::Dark();
		options.background = light ? Rgba8(230,233,236,255) : Rgba8(34,40,47,255);
		options.grid_color = palette.grid_color; options.selection_color = palette.selection_color;
		options.edge_color = palette.edge_color;
		String error; if(!pane.View().SetOptions(options, error)) SetStatus(error);
	}
	Refresh();
}

void Ui3DViewDemo::SetLight(bool on)
{
	light = on; ApplyTheme(); SyncChrome();
}

void Ui3DViewDemo::SetQuad(bool on)
{
	CancelGestures();
	quad = on;
	if(quad) {
		views.TopSplitter().NoZoom(); views.BottomSplitter().NoZoom(); views.RootSplitter().NoZoom();
	}
	else {
		views.TopSplitter().Zoom(active % 2); views.BottomSplitter().Zoom(active % 2);
		views.RootSplitter().Zoom(active / 2);
	}
	for(auto& pane : panes) pane.SetCompact(quad);
	SyncChrome(false);
	SetStatus(quad ? "Four views - each pane keeps its own camera" : "Single view");
}

void Ui3DViewDemo::SetInspectorVisible(bool on)
{
	inspector_visible = on; inspector_item.Visible(on); btn_inspector.SetChecked(on);
	btn_inspector.SetIcon(on ? ICON_DESIGN_RIGHT_PANEL_CLOSE_48() : ICON_DESIGN_RIGHT_PANEL_OPEN_48());
	Layout();
}

void Ui3DViewDemo::ActivatePane(int pane)
{
	if(pane < 0 || pane >= 4 || active == pane) return;
	if(lens_preview_pane >= 0) CancelGestures();
	active = pane;
	if(!quad) SetQuad(false);
	SyncChrome();
}

void Ui3DViewDemo::SetTool(Ui3DNavigationTool tool)
{
	navigation_tool = tool;
	String error;
	for(auto& pane : panes) if(!pane.View().SetNavigationTool(tool, error)) SetStatus(error);
	SyncChrome(false);
}

void Ui3DViewDemo::SyncCameras()
{
	String error;
	for(int i = 0; i < 4; ++i) {
		if(!ViewerCameraModel::SameCamera(panes[i].View().GetCamera(), cameras.GetPane(i).camera))
			if(!panes[i].View().SetCamera(cameras.GetPane(i).camera, error)) SetStatus(error);
		panes[i].RefreshChrome();
	}
}

void Ui3DViewDemo::SyncSelection()
{
	String error;
	for(auto& pane : panes) if(!pane.View().SetSelection(selection, error)) SetStatus(error);
}

void Ui3DViewDemo::SyncChrome(bool rebuild_lists)
{
	if(syncing) return;
	syncing = true;
	auto set_label = [](UiLabel& label, const String& text) {
		// UiLabel::SetText reparses and invalidates even an unchanged string.
		if(label.GetText() != text) label.SetText(text);
	};
	for(int i = 0; i < 4; ++i) {
		const auto *bookmark = cameras.GetCamera(cameras.GetPane(i).camera_id);
		panes[i].SetActive(i == active).SetCameraLabel(bookmark ? bookmark->name : String("Free camera"));
		panes[i].RefreshChrome();
	}
	const auto camera = panes[active].View().GetCamera();
	const auto *bound = cameras.GetCamera(cameras.GetPane(active).camera_id);
	set_label(view_name, "Direction       " + ViewerDirectionName(camera));
	set_label(view_detail, String("Projection     ") + (camera.orthographic ? "Orthographic" : "Perspective"));
	set_label(camera_binding, "Camera         " + (bound ? bound->name : String("Free camera")));
	set_label(lens_label, camera.orthographic ? "Scale" : "FOV");
	lens_label.Tip(camera.orthographic ? "Vertical view span in metres" : "Vertical field of view in degrees");
	if(lens_preview_pane < 0) {
		const double value = camera.orthographic ? camera.ortho_height : camera.fov_y * 180 / ViewerPi;
		const double minimum = min(value, camera.orthographic ? 1.0 : 20.0);
		const double maximum = max(value, camera.orthographic ? 150.0 : 80.0);
		const double step = camera.orthographic ? .1 : 1;
		if(lens.Slider().GetMin() != minimum || lens.Slider().GetMax() != maximum)
			lens.SetRange(minimum, maximum);
		if(lens.Slider().GetStep() != step) lens.SetStep(step);
		if(lens.GetValue() != value) lens.SetValue(value);
	}
	btn_select.SetChecked(navigation_tool == Ui3DNavigationTool::Select);
	btn_orbit.SetChecked(navigation_tool == Ui3DNavigationTool::Orbit);
	btn_pan.SetChecked(navigation_tool == Ui3DNavigationTool::Pan);
	btn_single.SetChecked(!quad); btn_quad.SetChecked(quad);
	btn_inspector.SetChecked(inspector_visible);
	btn_frame_selection.Enable(!selection.empty());
	set_label(heading_cameras, Format("CAMERAS  %d", cameras.GetCameraCount()));
	if(rebuild_lists) {
		const int old_scroll = camera_list.GetScrollPos();
		camera_list.ClearModel();
		for(int i = 0; i < cameras.GetCameraCount(); ++i) {
			const auto *bookmark = cameras.GetCamera(i);
			UiModelItem item(bookmark->name, bookmark->id);
			item.description = bookmark->camera.orthographic ? "Orthographic" : "Perspective";
			item.description = (bookmark->id == overview_camera_id ? "Overview - " :
			                    bookmark->id == low_camera_id ? "Low angle - " : "Saved view - ") + item.description;
			item.icon = ViewerCameraGlyph(light ? Color(82,98,114) : Color(170,181,193));
			camera_list.Model().Add(item);
			if(bookmark->id == cameras.GetPane(active).camera_id) camera_list.Select(i).SetCursor(i);
		}
		camera_list.SetScrollPos(old_scroll);
		const int old_cursor = object_list.GetCursor();
		object_list.ClearModel();
		for(int i = 0; i < (int)scene_description.objects.size(); ++i) {
			const auto& object = scene_description.objects[i];
			UiModelItem item(ViewerObjectNames[i], object.id);
			item.checked = object.visible;
			const Color colors[] = { Color(105,168,189), Color(207,146,91), Color(149,174,142) };
			item.icon = ViewerCubeGlyph(colors[i]);
			object_list.Model().Add(item);
			if(std::find(selection.begin(), selection.end(), object.id) != selection.end()) object_list.Select(i, true);
		}
		if(old_cursor >= 0 && old_cursor < object_list.Model().GetCount()) object_list.SetCursor(old_cursor);
	}
	btn_camera_undo.Enable(cameras.CanUndo());
	undo_item.Visible(cameras.CanUndo());
	int visible = 0;
	for(const auto& object : scene_description.objects) visible += object.visible;
	set_label(heading_scene, Format("SCENE  %d visible", visible));
	show_all_item.Visible(visible != (int)scene_description.objects.size());
	set_label(selection_label, selection.empty() ? String("No object selected") : Format("%d selected", (int)selection.size()));
	set_label(status_detail, quad ? "4 views   |   Y up   |   metres" : "Y up   |   metres");
	syncing = false;
}

void Ui3DViewDemo::SetStatus(const String& text)
{
	last_event = text;
	if(status_text.GetText() != text) status_text.SetText(text);
}

void Ui3DViewDemo::CameraChanged(int pane)
{
	String error;
	if(!cameras.ApplyCamera(pane, panes[pane].View().GetCamera(), error)) SetStatus(error);
	else SetStatus("Free camera - saved cameras are unchanged");
	SyncChrome();
}

void Ui3DViewDemo::Navigate(int pane, const Ui3DNavigationEvent& event)
{
	String error;
	bool ok = true;
	if(event.phase == Ui3DNavigationPhase::Begin) {
		if(event.gesture_id <= last_navigation_gesture[pane] ||
		   event.scene_binding_revision != panes[pane].View().GetSceneBindingRevision()) return;
		last_navigation_gesture[pane] = navigation_gesture[pane] = event.gesture_id;
		navigation_binding[pane] = event.scene_binding_revision;
		ok = cameras.BeginPreview(pane, error);
		if(quad) active = pane;
	}
	else if(event.gesture_id == navigation_gesture[pane] &&
	        event.scene_binding_revision == navigation_binding[pane]) {
		if(event.phase == Ui3DNavigationPhase::Cancel) ok = cameras.CancelPreview(pane, error);
		else {
			ok = cameras.PreviewCamera(pane, event.camera, error);
			if(ok && event.phase == Ui3DNavigationPhase::Commit) ok = cameras.CommitPreview(pane, error);
		}
		if(event.phase == Ui3DNavigationPhase::Commit || event.phase == Ui3DNavigationPhase::Cancel)
			navigation_gesture[pane] = navigation_binding[pane] = 0;
	}
	else if(event.phase == Ui3DNavigationPhase::Commit && event.gesture_id > last_navigation_gesture[pane] &&
	        event.scene_binding_revision == panes[pane].View().GetSceneBindingRevision()) {
		last_navigation_gesture[pane] = event.gesture_id;
		ok = cameras.ApplyCamera(pane, event.camera, error);
		if(quad) active = pane;
	}
	else return; // A cancelled gesture cannot modify a newer model/lens preview.
	if(!ok) SetStatus(error);
	else if(event.phase == Ui3DNavigationPhase::Cancel) SetStatus("Navigation cancelled");
	else if(event.phase == Ui3DNavigationPhase::Commit) SetStatus("Camera updated");
	SyncChrome(event.phase != Ui3DNavigationPhase::Update);
}

void Ui3DViewDemo::Select(int pane, const Ui3DSelectionRequest& request)
{
	ActivatePane(pane);
	if(!panes[pane].View().IsCurrentPick(request.pick)) return;
	if(!request.extend && !request.toggle) selection.clear();
	if(request.pick.hit) {
		auto it = std::find(selection.begin(), selection.end(), request.pick.id);
		if(it == selection.end()) selection.push_back(request.pick.id);
		else if(request.toggle) selection.erase(it);
	}
	SyncSelection(); SyncChrome();
	SetStatus(selection.empty() ? "Selection cleared" : "Selection updated");
}

void Ui3DViewDemo::CancelGestures()
{
	for(int i = 0; i < 4; ++i) {
		panes[i].View().CancelNavigation("application command");
		// Capture loss makes the control idle before its queued Cancel callback.
		// End the corresponding model transaction now; the token rejects that
		// old callback if an application command starts another preview first.
		if(navigation_gesture[i]) {
			String error; cameras.CancelPreview(i, error);
			navigation_gesture[i] = navigation_binding[i] = 0;
		}
	}
	if(lens_preview_pane >= 0) {
		String error;
		cameras.CancelPreview(lens_preview_pane, error);
		lens_preview_pane = -1;
		SyncCameras();
	}
}

void Ui3DViewDemo::Frame(bool selected)
{
	CancelGestures();
	if(selected ? panes[active].View().FitSelection() : panes[active].View().FitAll()) {
		CameraChanged(active);
		SetStatus(selected ? "Selection framed" : "Visible scene framed");
	}
}

void Ui3DViewDemo::ResetView()
{
	CancelGestures();
	String error;
	if(cameras.ApplyCamera(active, defaults[active], error)) {
		auto options = panes[active].View().GetOptions();
		options.grid_plane = active == 0 ? Ui3DGridPlane::XY : active == 2 ? Ui3DGridPlane::YZ : Ui3DGridPlane::XZ;
		panes[active].View().SetOptions(options, error);
		SyncCameras(); SyncChrome(); SetStatus("Active view reset");
	}
	else SetStatus(error);
}

void Ui3DViewDemo::SetProjection(bool orthographic)
{
	CancelGestures();
	String error;
	if(panes[active].View().SetOrthographic(orthographic, error)) CameraChanged(active);
	else SetStatus(error);
}

void Ui3DViewDemo::SetDirection(Scene3DViewDirection direction)
{
	CancelGestures();
	String error;
	if(panes[active].View().SetViewDirection(direction, error)) CameraChanged(active);
	else SetStatus(error);
}

void Ui3DViewDemo::ChangeLens(bool commit)
{
	if(syncing) return;
	const int pane = lens_preview_pane >= 0 ? lens_preview_pane : active;
	Scene3DCamera camera = panes[pane].View().GetCamera();
	if(camera.orthographic) camera.ortho_height = lens.GetValue();
	else camera.fov_y = lens.GetValue() * ViewerPi / 180;
	String error;
	if(!camera.Validate(error)) { SetStatus(error); return; }
	bool ok = lens_preview_pane >= 0 ? cameras.PreviewCamera(pane, camera, error) : cameras.ApplyCamera(pane, camera, error);
	if(ok) ok = panes[pane].View().SetCamera(camera, error);
	if(ok && commit && lens_preview_pane >= 0) {
		ok = cameras.CommitPreview(pane, error);
		lens_preview_pane = -1;
	}
	if(!ok) SetStatus(error);
	else SetStatus(camera.orthographic ? "View span updated" : "Field of view updated");
	SyncChrome(commit);
}

bool Ui3DViewDemo::EditName(const String& heading, String& name)
{
	CancelGestures();
	TopWindow dialog;
	UiPanel panel;
	UiBoxLayout column(UiDirection::V), row(UiDirection::H);
	UiLabel label, feedback;
	UiLineEdit edit;
	UiButton accept, cancel;
	dialog.Title(heading).SetRect(0, 0, DPI(390), DPI(180));
	dialog.Add(panel.SizePos()); panel.Add(column.SizePos());
	column.SetInset(DPI(16)).SetGap(DPI(9));
	label.SetText("Camera name"); column.Add(label).Fixed(DPI(22));
	edit.SetData(name); column.Add(edit).Fixed(DPI(32));
	feedback.SetText("1-40 characters. Duplicate names are allowed.");
	column.Add(feedback).Expand(); column.Add(row).Fixed(DPI(30));
	row.SetGap(DPI(8)); row.AddSpacer(); row.Add(cancel).Fixed(DPI(80)); row.Add(accept).Fixed(DPI(80));
	accept.SetText("Save"); cancel.SetText("Cancel");
	accept.WhenAction = [&] {
		String normalized, error;
		if(ViewerCameraModel::NormalizeName(AsString(edit.GetData()), normalized, error)) {
			name = normalized; dialog.Break(IDOK);
		}
		else { feedback.SetText(error); edit.SetFocus(); }
	};
	edit.WhenAction = accept.WhenAction;
	cancel.WhenAction = [&] { dialog.Break(IDCANCEL); };
	dialog.WhenClose = [&] { dialog.Break(IDCANCEL); };
	edit.SetFocus();
	return dialog.Run() == IDOK;
}

void Ui3DViewDemo::SaveCamera()
{
	String name = Format("Camera %02d", cameras.GetCameraCount() + 1);
	if(!EditName("Save camera", name)) return;
	String id, error;
	if(cameras.Save(active, name, id, error)) { selected_camera_id = id; SyncChrome(); SetStatus(name + " saved"); }
	else SetStatus(error);
}

void Ui3DViewDemo::RenameCamera()
{
	const String id = selected_camera_id;
	const auto *bookmark = cameras.GetCamera(id);
	if(!bookmark) return;
	String name = bookmark->name;
	if(!EditName("Rename camera", name)) return;
	String error;
	if(cameras.Rename(id, name, error)) { SyncChrome(); SetStatus("Camera renamed"); }
	else SetStatus(error);
}

void Ui3DViewDemo::LoadCamera(const String& id, int pane)
{
	CancelGestures();
	String error;
	if(cameras.Load(pane, id, error)) {
		selected_camera_id = id; SyncCameras(); SyncChrome(); SetStatus("Saved camera loaded in the active view");
	}
	else SetStatus(error);
}

void Ui3DViewDemo::UpdateCamera()
{
	CancelGestures();
	String error;
	if(cameras.UpdateFrom(active, selected_camera_id, error)) {
		SyncCameras(); SyncChrome(); SetStatus("Camera updated in bound views - Undo is available");
	}
	else SetStatus(error);
}

void Ui3DViewDemo::RemoveCamera()
{
	CancelGestures();
	String error;
	if(cameras.Remove(selected_camera_id, error)) {
		selected_camera_id.Clear(); SyncChrome(); SetStatus("Camera removed; views keep their positions - Undo is available");
	}
	else SetStatus(error);
}

void Ui3DViewDemo::UndoCamera()
{
	CancelGestures();
	String error;
	if(cameras.Undo(error)) { SyncCameras(); SyncChrome(); SetStatus("Camera action undone"); }
	else SetStatus(error);
}

void Ui3DViewDemo::PopulateCameraMenu(UiMenu& menu, int pane)
{
	menu.ClearModel();
	auto add = [&](const String& text, const String& id, bool checked) {
		UiMenuItem item(text, id); item.checkable = item.radio = true; item.checked = checked;
		menu.Model().AddChild(menu.Model().Root(), item);
	};
	add("Free camera", "", cameras.GetPane(pane).camera_id.IsEmpty());
	for(int i = 0; i < cameras.GetCameraCount(); ++i) {
		const auto *bookmark = cameras.GetCamera(i);
		add(bookmark->name, bookmark->id, bookmark->id == cameras.GetPane(pane).camera_id);
	}
	UiMenuItem save("Save current view...", "@save"); save.separator_before = true;
	menu.Model().AddChild(menu.Model().Root(), save);
	menu.WhenAction = [this, pane](UiMenuNodeRef, const UiMenuItem& item) {
		const String id = AsString(item.data);
		ActivatePane(pane);
		if(id == "@save") SaveCamera();
		else if(id.IsEmpty()) {
			CancelGestures(); String error;
			if(cameras.Free(pane, error)) { SyncChrome(); SetStatus("Free camera - current position retained"); }
			else SetStatus(error);
		}
		else LoadCamera(id, pane);
	};
}

void Ui3DViewDemo::OpenCameraActions(const String& id)
{
	if(!cameras.GetCamera(id)) return;
	selected_camera_id = id;
	camera_menu.ClearModel();
	const char *labels[] = { "Load in active view", "Rename...", "Update from active view", "Remove camera" };
	for(int command = 0; command < 4; ++command) {
		UiMenuItem item(labels[command], command);
		item.separator_before = command == 3;
		camera_menu.Model().AddChild(camera_menu.Model().Root(), item);
	}
	camera_menu.WhenAction = [this, id](UiMenuNodeRef, const UiMenuItem& item) {
		const int command = int(item.data);
		selected_camera_id = id;
		switch(command) {
		case 0: LoadCamera(id, active); break;
		case 1: RenameCamera(); break;
		case 2: UpdateCamera(); break;
		case 3: RemoveCamera(); break;
		}
	};
	const Rect action = camera_list.GetItemActionRect(id);
	const Point anchor = camera_list.GetScreenRect().TopLeft() +
	                     (action.IsEmpty() ? Point(0,0) : action.BottomLeft());
	camera_menu.PopUp(&camera_list, anchor);
}

void Ui3DViewDemo::ToggleVisibility()
{
	const int row = object_list.GetCursor();
	if(row < 0 || row >= (int)scene_description.objects.size()) return;
	ToggleVisibility(scene_description.objects[row].id);
}

void Ui3DViewDemo::ToggleVisibility(const String& id)
{
	auto found = std::find_if(scene_description.objects.begin(), scene_description.objects.end(),
	                         [&](const Scene3DObjectDesc& object) { return object.id == id; });
	if(found == scene_description.objects.end()) return;
	auto& object = *found;
	object.visible = !object.visible;
	++scene_description.revision;
	String error;
	if(!PublishScene(error)) { object.visible = !object.visible; SetStatus(error); return; }
	if(!object.visible) selection.erase(std::remove(selection.begin(), selection.end(), object.id), selection.end());
	SyncSelection(); SyncChrome(); SetStatus(object.visible ? "Object shown" : "Object hidden");
}

void Ui3DViewDemo::ShowAll()
{
	for(auto& object : scene_description.objects) object.visible = true;
	++scene_description.revision;
	String error;
	if(!PublishScene(error)) SetStatus(error);
	SyncChrome(); SetStatus("All objects visible");
}

bool Ui3DViewDemo::ViewerHasFocus() const
{
	Ctrl *focus = GetFocusCtrl();
	if(!focus) return false;
	for(const auto& pane : panes)
		if(focus == &pane || pane.HasChildDeep(focus)) return true;
	return false;
}

bool Ui3DViewDemo::Key(dword key, int count)
{
	if(!ViewerHasFocus()) return TopWindow::Key(key, count);
	if(key >= 'a' && key <= 'z') key -= 'a' - 'A';
	if(key == 'V') SetTool(Ui3DNavigationTool::Select);
	else if(key == 'O') SetTool(Ui3DNavigationTool::Orbit);
	else if(key == 'H') SetTool(Ui3DNavigationTool::Pan);
	else if(key == 'Q' || key == K_SPACE) SetQuad(!quad);
	else if(key == 'I') SetInspectorVisible(!inspector_visible);
	else if(key == 'F') Frame(true);
	else if(key == K_HOME) Frame(false);
	else if(key == '5' || key == K_NUMPAD5) SetProjection(!panes[active].View().GetCamera().orthographic);
	else if(key == '1' || key == K_NUMPAD1) SetDirection(Scene3DViewDirection::Front);
	else if(key == '3' || key == K_NUMPAD3) SetDirection(Scene3DViewDirection::Right);
	else if(key == '7' || key == K_NUMPAD7) SetDirection(Scene3DViewDirection::Top);
	else if(key == K_CTRL_1 || key == K_CTRL_NUMPAD1) SetDirection(Scene3DViewDirection::Back);
	else if(key == K_CTRL_3 || key == K_CTRL_NUMPAD3) SetDirection(Scene3DViewDirection::Left);
	else if(key == K_CTRL_7 || key == K_CTRL_NUMPAD7) SetDirection(Scene3DViewDirection::Bottom);
	else if(key == K_ESCAPE) {
		bool navigating = lens_preview_pane >= 0;
		for(const auto& pane : panes) navigating |= pane.View().IsNavigating();
		if(navigating) CancelGestures();
		else { selection.clear(); SyncSelection(); SyncChrome(); SetStatus("Selection cleared"); }
	}
	else return TopWindow::Key(key, count);
	return true;
}

void Ui3DViewDemo::ShowHelp()
{
	CancelGestures();
	TopWindow dialog;
	UiPanel panel; UiBoxLayout column(UiDirection::V), row(UiDirection::H);
	UiLabel heading, text; UiButton reset, close;
	dialog.Title("Find your way around").SetRect(0, 0, DPI(560), DPI(390));
	dialog.Add(panel.SizePos()); panel.Add(column.SizePos());
	column.SetInset(DPI(20)).SetGap(DPI(14));
	heading.SetText("Each pane has its own camera, display mode and overlays.");
	column.Add(heading).Fixed(DPI(28));
	text.SetText("Middle drag: orbit.  Shift + middle: pan.  Wheel: zoom.\n"
	             "V / O / H: Select / Orbit / Pan with the left mouse button.\n"
	             "F / Home: frame selection / all visible objects.\n"
	             "Q: single / four views.  Space: maximise / restore.  I: inspector.\n"
	             "1 / 3 / 7: Front / Right / Top.  Ctrl chooses the opposite side.\n"
	             "5: perspective / orthographic.  Esc: cancel drag / clear selection.\n\n"
	             "Click an orientation axis to align the active view.\n"
	             "Save a camera with +. Navigating detaches from its saved pose.\n"
	             "Update changes bound views; Remove keeps their positions.\n"
	             "Undo restores an update or removal without erasing newer edits.");
	text.SetAlignV(UiAlign::TOP); column.Add(text).Expand();
	column.Add(row).Fixed(DPI(32)); row.SetGap(DPI(8));
	row.Add(reset).Fixed(DPI(100)); row.AddSpacer(); row.Add(close).Fixed(DPI(90));
	reset.SetText("Reset demo"); close.SetText("Got it");
	reset.WhenAction = [&] { ResetDemo(); dialog.Break(); };
	close.WhenAction = [&] { dialog.Break(); };
	dialog.Run();
}

void Ui3DViewDemo::SetValidation()
{
	for(auto& pane : panes) pane.View().SetValidation();
}

bool Ui3DViewDemo::CheckReady(String& error) const
{
	for(int i = 0; i < 4; ++i) if(quad || i == active) {
		const auto& view = panes[i].View();
		error = view.GetViewError();
		if(error.IsEmpty() && (!view.IsGpuReady() || !view.GetViewStats().depth_active || !view.GetViewStats().rendered_frames))
			error = Format("pane %d has not completed a depth-tested Vulkan frame", i + 1);
		if(!error.IsEmpty()) return false;
	}
	return true;
}

// OS observation of this foreground client, including native child HWNDs. This
// is a diagnostic screenshot, not a renderer export/readback API.
bool Ui3DViewDemo::Capture(const String& file, String& error)
{
	error.Clear();
	RECT rect;
	if(GetForegroundWindow() != GetHWND()) { error = "capture requires the demo foreground window"; return false; }
	if(!GetClientRect(GetHWND(), &rect)) { error = "cannot read native client rectangle"; return false; }
	const int width = rect.right, height = rect.bottom;
	if(width <= 0 || height <= 0 || (int64)width * height > 16000000) return false;
	POINT origin = {0,0};
	if(!ClientToScreen(GetHWND(), &origin)) return false;
	// Foreground alone does not exclude an unrelated topmost window. Sample the
	// actual hit-tested surface (invisible/cloaked HWND bounds are not evidence
	// of occlusion). Captures are additionally inspected before publication.
	for(int row = 0; row <= 10; ++row) for(int column = 0; column <= 10; ++column) {
		POINT point = { origin.x + 2 + (width - 5) * column / 10,
		                origin.y + 2 + (height - 5) * row / 10 };
		if(GetAncestor(WindowFromPoint(point), GA_ROOT) != GetHWND()) {
			error = Format("capture client is occluded or off-screen at sample %d,%d", column, row);
			return false;
		}
	}
	HDC screen = GetDC(nullptr), memory = CreateCompatibleDC(screen);
	BITMAPINFO info {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
	void *pixels = nullptr;
	HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
	bool ok = memory && bitmap && pixels;
	HGDIOBJ old = nullptr;
	if(ok) { old = SelectObject(memory, bitmap); ok = BitBlt(memory,0,0,width,height,screen,origin.x,origin.y,SRCCOPY|CAPTUREBLT); }
	if(ok) {
		ImageBuffer image(width,height);
		const byte *source = static_cast<const byte *>(pixels);
		for(int y = 0; y < height; ++y) for(int x = 0; x < width; ++x) {
			const byte *p = source + ((int64)y * width + x) * 4;
			RGBA& target = image[y][x];
			target.r = p[2]; target.g = p[1]; target.b = p[0]; target.a = 255;
		}
		image.SetKind(IMAGE_OPAQUE); ok = PNGEncoder().SaveFile(file, Image(image));
	}
	if(old) SelectObject(memory, old);
	if(bitmap) DeleteObject(bitmap);
	if(memory) DeleteDC(memory);
	if(screen) ReleaseDC(nullptr, screen);
	if(!ok) error = "native client capture or PNG write failed";
	return ok;
}

}
