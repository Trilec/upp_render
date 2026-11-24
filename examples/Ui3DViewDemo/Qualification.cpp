#include "Ui3DViewDemo.h"
#ifdef GUI_WIN
#include <plugin/png/png.h>
#include <windows.h>
#include <cmath>

namespace Upp {

// Finite application integration checks. Core/model/GPU contracts also have
// independent test packages; these exercise the actual host wiring and HWNDs.
bool Ui3DViewDemo::QualifyWorkspace(String& report)
{
	int checks = 0, failures = 0;
	const Rect workspace_rect = GetRect();
	auto check = [&](bool condition, const char *name) {
		++checks;
		if(!condition) { ++failures; report << "FAIL: " << name << "\n"; }
	};
	auto pump = [](int count = 12) {
		// GuiSleep wakes early for Windows messages; count calls do not imply
		// count * 10 ms. Unsigned subtraction also handles tick wraparound.
		const DWORD started = GetTickCount();
		const DWORD duration = (DWORD)max(0, count) * 10;
		for(;;) {
			Ctrl::ProcessEvents();
			const DWORD elapsed = GetTickCount() - started;
			if(elapsed >= duration) break;
			Ctrl::GuiSleep(min(10, int(duration - elapsed)));
		}
		Ctrl::ProcessEvents();
	};
	ResetDemo(); pump();
	check(active == 3 && !quad && cameras.GetCameraCount() == 2, "initial single perspective and two bookmarks");
	check(scene && scene->GetObjects().size() == 3, "only three authored objects; helpers excluded");
	const String first_id = cameras.GetCamera(0)->id;
	const auto original = defaults;
	SetQuad(true); pump(30);
	String error;
	check(CheckReady(error), "all four native panes complete Vulkan frames");
	for(int i = 0; i < 4; ++i) {
		check(panes[i].View().IsVisible(), "quad pane is visible");
		check(ViewerCameraModel::SameCamera(panes[i].View().GetCamera(), original[i]), "quad retains camera values");
	}
	ActivatePane(0); SetDirection(Scene3DViewDirection::Front);
	check(panes[0].View().GetCamera().GetViewDirection() == Scene3DViewDirection::Front &&
	      panes[0].View().GetOptions().grid_plane == Ui3DGridPlane::XY, "front command owns XY grid");
	const double span = panes[0].View().GetCamera().ortho_height;
	SetProjection(false);
	auto camera = panes[0].View().GetCamera();
	check(std::abs(2 * camera.distance * std::tan(camera.fov_y / 2) - span) < 1e-9,
	      "host projection command preserves target plane span");
	panes[0].View().SetDisplayMode(Ui3DDisplayMode::SolidEdges, error);
	auto options = panes[0].View().GetOptions(); options.show_grid = false;
	panes[0].View().SetOptions(options, error);
	SetQuad(false); pump();
	check(panes[0].View().IsVisible() && !panes[1].View().IsVisible() && !panes[2].View().IsVisible() &&
	      !panes[3].View().IsVisible(), "maximise hides the other native children");
	SetLight(true); SetQuad(true); pump();
	check(panes[0].View().GetDisplayMode() == Ui3DDisplayMode::SolidEdges &&
	      !panes[0].View().GetOptions().show_grid && panes[0].View().GetOptions().grid_plane == Ui3DGridPlane::XY,
	      "layout and theme preserve per-view display and overlays");
	check(ViewerCameraModel::SameCamera(panes[0].View().GetCamera(), camera), "layout and theme preserve camera");
	LoadCamera(first_id, 0); LoadCamera(first_id, 1);
	check(cameras.GetPane(0).camera_id == first_id && cameras.GetPane(1).camera_id == first_id,
	      "two panes bind to one stable bookmark ID");
	ActivatePane(3);
	camera = panes[3].View().GetCamera(); camera.Orbit(.2, .1);
	panes[3].View().SetCamera(camera, error); CameraChanged(3);
	selected_camera_id = first_id; UpdateCamera();
	for(int i : { 0, 1, 3 }) check(ViewerCameraModel::SameCamera(panes[i].View().GetCamera(), camera),
	                              "update propagates through application to every bound pane");
	UndoCamera();
	check(ViewerCameraModel::SameCamera(panes[3].View().GetCamera(), camera) && cameras.GetPane(3).camera_id.IsEmpty(),
	      "update undo restores active source's previous free binding");
	check(ViewerCameraModel::SameCamera(panes[0].View().GetCamera(), original[3]), "update undo restores other bound poses");
	selected_camera_id = first_id; RemoveCamera();
	check(cameras.GetPane(0).camera_id.IsEmpty() && cameras.GetPane(1).camera_id.IsEmpty(), "remove frees every binding");
	UndoCamera();
	check(cameras.GetPane(0).camera_id == first_id && cameras.GetPane(1).camera_id == first_id, "remove undo restores unchanged bindings");
	ActivatePane(0); SetQuad(false); pump();
	auto& view = panes[0].View();
	view.SetFocus();
	const auto bound_pose = view.GetCamera();
	// A synthetic MiddleDown followed by pumping native events has no matching
	// Windows button state and can legitimately lose capture before the assertion.
	// Drive this transaction through the real input queue, including Escape and
	// button release, and observe delivery rather than assuming a fixed delay.
	POINT navigation_cursor_before {};
	GetCursorPos(&navigation_cursor_before);
	const Rect navigation_rect = view.GetScreenRect();
	const POINT navigation_press = { navigation_rect.left + 80, navigation_rect.top + 80 };
	const POINT navigation_move = { navigation_rect.left + 120, navigation_rect.top + 100 };
	auto owns_navigation_point = [&](POINT point) {
		return GetForegroundWindow() == GetHWND() &&
		       navigation_rect.Contains(Point(point.x, point.y)) &&
		       GetAncestor(WindowFromPoint(point), GA_ROOT) == GetHWND();
	};
	bool navigation_input_ready = owns_navigation_point(navigation_press) &&
	                              owns_navigation_point(navigation_move) && GetFocusCtrl() == &view;
	for(int held : { VK_LBUTTON, VK_MBUTTON, VK_RBUTTON, VK_CONTROL, VK_SHIFT, VK_MENU })
		if(GetAsyncKeyState(held) & 0x8000) navigation_input_ready = false;
	bool middle_down_sent = false;
	if(navigation_input_ready && SetCursorPos(navigation_press.x, navigation_press.y)) {
		pump(2);
		INPUT down {};
		down.type = INPUT_MOUSE; down.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
		middle_down_sent = owns_navigation_point(navigation_press) &&
		                   SendInput(1, &down, sizeof(INPUT)) == 1;
	}
	for(int i = 0; i < 20 && middle_down_sent && !view.IsNavigating(); ++i) pump(1);
	check(middle_down_sent && view.IsNavigating() && view.HasCapture() && cameras.GetPane(0).camera_id == first_id,
	      "press alone retains camera binding");
	bool navigation_motion_sent = false;
	const int desktop_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	const int desktop_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	if(middle_down_sent && view.IsNavigating() && owns_navigation_point(navigation_move) &&
	   desktop_width > 1 && desktop_height > 1) {
		INPUT motion {};
		motion.type = INPUT_MOUSE;
		motion.mi.dx = LONG((int64(navigation_move.x - GetSystemMetrics(SM_XVIRTUALSCREEN)) * 65536 + 32768) / desktop_width);
		motion.mi.dy = LONG((int64(navigation_move.y - GetSystemMetrics(SM_YVIRTUALSCREEN)) * 65536 + 32768) / desktop_height);
		motion.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
		                    MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE_NOCOALESCE;
		navigation_motion_sent = SendInput(1, &motion, sizeof(INPUT)) == 1;
	}
	for(int i = 0; i < 20 && navigation_motion_sent && view.IsNavigating() &&
	    !cameras.GetPane(0).camera_id.IsEmpty(); ++i) pump(1);
	check(navigation_motion_sent && view.IsNavigating() && cameras.GetPane(0).camera_id.IsEmpty() &&
	      !ViewerCameraModel::SameCamera(view.GetCamera(), bound_pose) &&
	      ViewerCameraModel::SameCamera(cameras.GetPane(0).camera, view.GetCamera()),
	      "actual drag motion detaches saved camera");
	bool escape_sent = false;
	if(middle_down_sent && GetForegroundWindow() == GetHWND() && GetFocusCtrl() == &view) {
		INPUT escape[2] {};
		escape[0].type = escape[1].type = INPUT_KEYBOARD;
		escape[0].ki.wVk = escape[1].ki.wVk = VK_ESCAPE;
		escape[1].ki.dwFlags = KEYEVENTF_KEYUP;
		escape_sent = SendInput(2, escape, sizeof(INPUT)) == 2;
	}
	for(int i = 0; i < 20 && escape_sent && view.IsNavigating(); ++i) pump(1);
	bool middle_up_sent = false;
	if(middle_down_sent && GetForegroundWindow() == GetHWND()) {
		INPUT up {};
		up.type = INPUT_MOUSE; up.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
		middle_up_sent = SendInput(1, &up, sizeof(INPUT)) == 1;
		pump(2);
	}
	check(escape_sent && middle_up_sent && !view.IsNavigating() && !view.HasCapture() &&
	      cameras.GetPane(0).camera_id == first_id && ViewerCameraModel::SameCamera(view.GetCamera(), bound_pose),
	      "cancel restores pose and valid binding through host callbacks");
	// Cleanup follows assertions so a failed native path cannot be hidden by a
	// programmatic reset, and later checks still start without an active gesture.
	view.CancelNavigation("qualification cleanup");
	if(middle_up_sent && GetForegroundWindow() == GetHWND())
		SetCursorPos(navigation_cursor_before.x, navigation_cursor_before.y);
	view.MouseWheel(Point(80,80), 120, 0); pump();
	check(cameras.GetPane(0).camera_id.IsEmpty() &&
	      ViewerCameraModel::SameCamera(cameras.GetPane(0).camera, view.GetCamera()), "standalone wheel commit reaches model");
	selection = { scene_description.objects[0].id }; SyncSelection(); SyncChrome();
	object_list.SetCursor(0); ToggleVisibility();
	check(!scene->FindObject("box-blue")->visible && selection.empty(), "visibility updates snapshot and selection in all views");
	for(auto& pane : panes) check(pane.View().GetScene() == scene, "all panes share one immutable scene snapshot");
	ShowAll();
	check(scene->FindObject("box-blue")->visible, "show all restores hidden object");
	lens.Field().SetFocus();
	const bool before = quad;
	Key('Q', 1);
	check(quad == before, "viewer shortcuts do not run in the lens field");
	view.SetFocus(); Key('Q', 1); pump();
	check(quad != before, "viewer-focused layout shortcut");
	SetQuad(false); ActivatePane(3); panes[3].View().SetFocus(); pump(20);
	// Native click: the projection button is centered in the bounded 98px
	// orientation overlay. Guard foreground before moving or injecting input.
	POINT cursor_before {};
	GetCursorPos(&cursor_before);
	bool native_click = GetForegroundWindow() == GetHWND();
	const bool was_orthographic = panes[3].View().GetCamera().orthographic;
	if(native_click) {
		const Rect rect = panes[3].View().GetScreenRect();
		native_click = SetCursorPos(rect.right - DPI(8 + 49), rect.top + DPI(8 + 49));
		pump(15);
		INPUT input[2] {};
		input[0].type = input[1].type = INPUT_MOUSE;
		input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
		native_click = native_click && GetForegroundWindow() == GetHWND() && SendInput(2, input, sizeof(INPUT)) == 2;
		pump(25);
	}
	check(native_click, "foreground-guarded native orientation click delivered");
	check(native_click && panes[3].View().GetCamera().orthographic != was_orthographic,
	      "native orientation overlay receives input above Vulkan HWND");
	if(GetForegroundWindow() == GetHWND()) SetCursorPos(cursor_before.x, cursor_before.y);

	// The wrapper's minimum must not grow from its previously assigned header
	// or overlay positions. Check the actual layout after both shrinking and
	// restoring, not only an isolated preferred-size query.
	std::array<Size, 4> pane_minimum;
	for(int i = 0; i < 4; ++i) pane_minimum[i] = panes[i].GetMinSize();
	auto check_layout = [&](int sidebar_width, const char *name) {
		const Rect host = body.GetScreenRect();
		const Rect viewport = views.GetScreenRect();
		const Rect sidebar = inspector.GetScreenRect();
		const Rect client = GetScreenView();
		check(inspector.IsVisible() && sidebar.left >= host.left &&
		      sidebar.right <= host.right - DPI(12) + 1 && sidebar.right <= client.right &&
		      std::abs(sidebar.GetWidth() - sidebar_width) <= 1, name);
		check(std::abs(viewport.left - host.left - DPI(12)) <= 1 &&
		      std::abs(sidebar.left - viewport.right - DPI(9)) <= 1 &&
		      viewport.right <= host.right && viewport.GetWidth() > 0,
		      "viewport and inspector share the bounded body with the configured gap");
		bool stable = true, contained = true;
		for(int i = 0; i < 4; ++i) {
			stable = stable && panes[i].GetMinSize() == pane_minimum[i];
			if(!panes[i].IsVisible()) continue;
			const Rect pane = panes[i].GetScreenRect();
			contained = contained && pane.left >= viewport.left && pane.top >= viewport.top &&
			            pane.right <= viewport.right && pane.bottom <= viewport.bottom;
		}
		check(stable && contained, "pane minimum and allocated bounds survive layout transitions");
		check(inspector_scroll.IsVisible() && inspector_scroll.GetSize().cy > 0 &&
		      inspector_column.IsVisible() && inspector_column.GetSize().cx > 0 && inspector_column.GetSize().cy > 0 &&
		      inspector_column.GetSize().cy >= camera_list.GetRect().bottom &&
		      inspector_column.GetSize().cy >= object_list.GetRect().bottom &&
		      lens.IsVisible() && lens.GetSize().cy > 0 &&
		      camera_list.IsVisible() && camera_list.GetSize().cy > 0 &&
		      object_list.IsVisible() && object_list.GetSize().cy > 0,
		      "inspector has positive content height and visible lens and list allocations");
		const Rect lens_bounds(lens.GetSize());
		const Rect field = lens.Field().GetRect(), slider = lens.Slider().GetRect();
		check(lens.Field().IsVisible() && lens.Slider().IsVisible() &&
		      field.GetWidth() >= DPI(45) && slider.GetWidth() >= DPI(48) &&
		      field.GetHeight() > 0 && slider.GetHeight() > 0 &&
		      (field & lens_bounds) == field && (slider & lens_bounds) == slider &&
		      slider.right + DPI(5) <= field.left,
		      "lens numeric field and slider have usable nonoverlapping bounds inside their composite");
		const Rect field_screen = lens.Field().GetScreenRect();
		const Rect slider_screen = lens.Slider().GetScreenRect();
		const Rect row_screen = lens_row.GetScreenRect();
		const Rect inspector_view = inspector_scroll.GetViewportRect() + inspector_scroll.GetScreenRect().TopLeft();
		check((field_screen & row_screen & inspector_view) == field_screen &&
		      (slider_screen & row_screen & inspector_view) == slider_screen,
		      "both lens children are fully visible within the row and inspector viewport");
		lens.Field().SetFocus();
		check(lens.Field().HasFocusDeep(), "visible lens numeric field accepts keyboard focus");
	};
	SetInspectorVisible(true); SetQuad(false);
	inspector_scroll.SetScrollPos(Point(0, 0));
	SetProjection(false);
	SetRect(workspace_rect.left, workspace_rect.top, DPI(1440), DPI(900)); pump(3);
	check_layout(DPI(246), "1440 single keeps the entire fixed inspector inside the client");
	SetProjection(true);
	SetRect(workspace_rect.left, workspace_rect.top, DPI(720), DPI(600)); SetQuad(true); pump(3);
	check_layout(DPI(202), "720 quad keeps the entire compact inspector inside the client");
	SetQuad(false); pump(2);
	check_layout(DPI(202), "720 single does not inherit a previous larger viewport minimum");
	SetProjection(false);
	SetRect(workspace_rect.left, workspace_rect.top, DPI(1440), DPI(900)); SetQuad(true); pump(3);
	check_layout(DPI(246), "1440 quad restores the inspector and persistent pane bounds");

	// Every native batch starts with this window foreground and the intended
	// control focused. Refuse input if another window or a held user modifier is
	// present; a qualification run must never type into an unrelated application.
	auto send_key = [&](Ctrl& target, WORD key, bool control = false, bool shift = false) {
		if(GetForegroundWindow() != GetHWND() || !target.IsOpen() ||
		   !target.IsVisible() || !target.IsEnabled()) return false;
		for(int modifier : { VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN })
			if(GetAsyncKeyState(modifier) & 0x8000) return false;
		target.SetFocus(); pump(1);
		if(GetForegroundWindow() != GetHWND() || GetFocusCtrl() != &target) return false;
		INPUT input[6] {};
		int count = 0;
		auto append = [&](WORD value, bool up) {
			INPUT& event = input[count++];
			event.type = INPUT_KEYBOARD;
			event.ki.wVk = value;
			event.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
		};
		if(control) append(VK_CONTROL, false);
		if(shift) append(VK_SHIFT, false);
		append(key, false); append(key, true);
		if(shift) append(VK_SHIFT, true);
		if(control) append(VK_CONTROL, true);
		const bool sent = SendInput(count, input, sizeof(INPUT)) == UINT(count);
		pump(2);
		return sent;
	};
	SetQuad(false); ActivatePane(3); panes[3].View().SetFocus(); pump(2);
	struct AxisKeyCase { WORD key; bool control; Scene3DViewDirection direction; const char *name; };
	const AxisKeyCase axis_keys[] = {
		{ '1', false, Scene3DViewDirection::Front, "native 1 selects Front" },
		{ '3', false, Scene3DViewDirection::Right, "native 3 selects Right" },
		{ '7', false, Scene3DViewDirection::Top, "native 7 selects Top" },
		{ '1', true, Scene3DViewDirection::Back, "native Ctrl+1 selects Back through canonical key mapping" },
		{ '3', true, Scene3DViewDirection::Left, "native Ctrl+3 selects Left through canonical key mapping" },
		{ '7', true, Scene3DViewDirection::Bottom, "native Ctrl+7 selects Bottom through canonical key mapping" }
	};
	for(const auto& test : axis_keys) {
		const bool sent = send_key(panes[3].View(), test.key, test.control);
		check(sent && panes[3].View().GetCamera().GetViewDirection() == test.direction &&
		      panes[3].View().GetCamera().orthographic, test.name);
	}
	const bool key_projection_before = panes[3].View().GetCamera().orthographic;
	const bool projection_sent = send_key(panes[3].View(), '5');
	check(projection_sent && panes[3].View().GetCamera().orthographic != key_projection_before,
	      "native 5 toggles projection in the focused view");
	struct ToolKeyCase { WORD key; Ui3DNavigationTool tool; const char *name; };
	const ToolKeyCase tool_keys[] = {
		{ 'O', Ui3DNavigationTool::Orbit, "native O synchronizes Orbit across the workspace" },
		{ 'H', Ui3DNavigationTool::Pan, "native H synchronizes Pan across the workspace" },
		{ 'V', Ui3DNavigationTool::Select, "native V synchronizes Select across the workspace" }
	};
	for(const auto& test : tool_keys) {
		bool matches = send_key(panes[3].View(), test.key) && navigation_tool == test.tool;
		for(auto& pane : panes) matches = matches && pane.View().GetNavigationTool() == test.tool;
		check(matches, test.name);
	}
	const bool quad_before_key = quad;
	bool layout_key_sent = send_key(panes[3].View(), 'Q');
	check(layout_key_sent && quad != quad_before_key && active == 3, "native Q changes layout without changing active identity");
	layout_key_sent = send_key(panes[3].View(), 'Q');
	check(layout_key_sent && quad == quad_before_key, "second native Q restores the original layout");

	// Obtain a real gesture ID from the view. CancelMode makes the view idle but
	// deliberately defers its event, matching native capture loss. The host must
	// reconcile the model now and ignore the queued or replayed old Cancel.
	SetQuad(true); ActivatePane(0); LoadCamera(first_id, 0);
	auto& cancelled_view = panes[0].View();
	cancelled_view.SetFocus();
	const Scene3DCamera before_deferred_cancel = cancelled_view.GetCamera();
	cancelled_view.MiddleDown(Point(80,80), 0);
	cancelled_view.MouseMove(Point(104,92), K_MOUSEMIDDLE);
	Ui3DNavigationEvent stale_cancel;
	stale_cancel.phase = Ui3DNavigationPhase::Cancel;
	stale_cancel.mode = Ui3DNavigationMode::Orbit;
	stale_cancel.gesture_id = navigation_gesture[0];
	stale_cancel.scene_binding_revision = navigation_binding[0];
	stale_cancel.scene_revision = scene->GetRevision();
	stale_cancel.starting_camera_revision = before_deferred_cancel.revision;
	stale_cancel.camera = before_deferred_cancel;
	stale_cancel.reason = "qualification deferred cancellation";
	check(stale_cancel.gesture_id != 0 && cameras.IsPreviewing(0) && cancelled_view.IsNavigating(),
	      "deferred-cancel fixture uses an actual live view gesture");
	cancelled_view.CancelMode();
	if(cancelled_view.HasCapture()) cancelled_view.ReleaseCapture();
	check(!cancelled_view.IsNavigating() && cameras.IsPreviewing(0),
	      "capture loss defers the model cancellation notification");
	CancelGestures();
	check(!cameras.IsPreviewing(0) && navigation_gesture[0] == 0 &&
	      ViewerCameraModel::SameCamera(cameras.GetPane(0).camera, before_deferred_cancel),
	      "application cancellation closes an idle view's still-pending model preview");
	ActivatePane(1);
	lens.WhenBeginEdit();
	Scene3DCamera later_lens = cameras.GetPane(1).camera;
	later_lens.fov_y *= 1.05;
	bool lens_ready = lens_preview_pane == 1 && cameras.IsPreviewing(1) &&
	                  cameras.PreviewCamera(1, later_lens, error) && panes[1].View().SetCamera(later_lens, error);
	pump(2);
	check(lens_ready && active == 1 && lens_preview_pane == 1 && cameras.IsPreviewing(1) &&
	      ViewerCameraModel::SameCamera(cameras.GetPane(1).camera, later_lens),
	      "queued old Cancel neither reactivates its pane nor cancels another pane's lens preview");
	CancelGestures(); ActivatePane(0); lens.WhenBeginEdit();
	later_lens = cameras.GetPane(0).camera; later_lens.fov_y *= 1.03;
	lens_ready = lens_preview_pane == 0 && cameras.IsPreviewing(0) &&
	             cameras.PreviewCamera(0, later_lens, error) && panes[0].View().SetCamera(later_lens, error);
	Navigate(0, stale_cancel);
	check(lens_ready && lens_preview_pane == 0 && cameras.IsPreviewing(0) &&
	      ViewerCameraModel::SameCamera(cameras.GetPane(0).camera, later_lens),
	      "old gesture token cannot cancel a newer lens preview in the same pane");
	CancelGestures();

	// The action rectangles come from the actual scrolled native list renderer.
	// Verify their host callbacks; no callback is replaced with a test double.
	auto send_mouse = [&](Ctrl& target, Point local, DWORD flags) {
		if(GetForegroundWindow() != GetHWND() || !target.IsOpen() ||
		   !target.IsVisible() || !target.IsEnabled() || !Rect(target.GetSize()).Contains(local)) return false;
		const Point screen_point = target.GetScreenRect().TopLeft() + local;
		for(Ctrl *ancestor = &target; ancestor; ancestor = ancestor->GetParent()) {
			if(!ancestor->GetScreenView().Contains(screen_point)) return false;
			if(ancestor == this) break;
		}
		POINT native_point = { screen_point.x, screen_point.y };
		const HWND hit = WindowFromPoint(native_point);
		if(!hit || GetAncestor(hit, GA_ROOT) != GetHWND() ||
		   !SetCursorPos(native_point.x, native_point.y)) return false;
		INPUT input {};
		input.type = INPUT_MOUSE; input.mi.dwFlags = flags;
		if(GetForegroundWindow() != GetHWND()) return false;
		const bool sent = SendInput(1, &input, sizeof(INPUT)) == 1;
		pump(2);
		return sent;
	};
	auto click_action = [&](ViewerActionList& list, const String& id) {
		list.Layout();
		const Rect action = list.GetItemActionRect(id);
		if(action.IsEmpty()) return false;
		const Point point = action.CenterPoint();
		return send_mouse(list, point, MOUSEEVENTF_LEFTDOWN) &&
		       send_mouse(list, point, MOUSEEVENTF_LEFTUP);
	};
	SetQuad(false); ActivatePane(3); SetInspectorVisible(true); ShowAll();
	selection = { scene_description.objects[0].id }; SyncSelection(); SyncChrome(); pump(2);
	const String object_a = scene_description.objects[0].id, object_b = scene_description.objects[1].id;
	const auto selection_before_eye = selection;
	const int cursor_before_eye = object_list.GetCursor();
	const bool eye_sent = click_action(object_list, object_b);
	bool shared_selection_unchanged = selection == selection_before_eye;
	for(auto& pane : panes) shared_selection_unchanged = shared_selection_unchanged &&
	                                                pane.View().GetSelection() == selection_before_eye;
	check(eye_sent && !scene->FindObject(object_b)->visible && shared_selection_unchanged &&
	      object_list.GetCursor() == cursor_before_eye,
	      "native eye on object B changes visibility without selecting it or changing object A selection");
	object_list.Layout();
	const Rect stale_action = object_list.GetItemActionRect(object_b);
	const bool visibility_before_stale_action = scene->FindObject(object_b)->visible;
	bool stale_press_sent = !stale_action.IsEmpty() &&
	                        send_mouse(object_list, stale_action.CenterPoint(), MOUSEEVENTF_LEFTDOWN);
	const bool action_captured = object_list.HasCapture();
	SyncChrome(); // Rebuild advances the model revision between the two native events.
	const bool stale_release_sent = stale_press_sent &&
	                               send_mouse(object_list, stale_action.CenterPoint(), MOUSEEVENTF_LEFTUP);
	check(stale_press_sent && action_captured && stale_release_sent && !object_list.HasCapture() &&
	      scene->FindObject(object_b)->visible == visibility_before_stale_action && selection == selection_before_eye,
	      "native action release after a model rebuild cannot invoke a stale row");
	if(object_list.HasCapture()) { object_list.CancelMode(); object_list.ReleaseCapture(); }
	// The current row remains A; Scene Space invokes its eye action, so hiding the
	// selected object clears shared selection as required by the host policy.
	const bool space_sent = send_key(object_list, VK_SPACE);
	check(space_sent && !scene->FindObject(object_a)->visible && selection.empty(),
	      "native Space in the scene list invokes visibility and clears hidden selection");
	ShowAll();
	const String camera_a = cameras.GetCamera(0)->id, camera_b = cameras.GetCamera(1)->id;
	LoadCamera(camera_a, 3); pump(2);
	const auto camera_before_actions = cameras.GetPane(3);
	const int camera_cursor_before_actions = camera_list.GetCursor();
	const HWND ellipsis_foreground_before = GetForegroundWindow();
	const bool ellipsis_sent = click_action(camera_list, camera_b);
	const HWND ellipsis_foreground_after = GetForegroundWindow();
	const bool ellipsis_menu_open = camera_menu.IsMenuOpen();
	const bool ellipsis_target_matches = selected_camera_id == camera_b;
	const bool ellipsis_binding_matches = cameras.GetPane(3).camera_id == camera_a;
	const bool ellipsis_generation_matches = cameras.GetPane(3).generation == camera_before_actions.generation;
	const bool ellipsis_pose_matches = ViewerCameraModel::SameCamera(panes[3].View().GetCamera(), camera_before_actions.camera);
	const bool ellipsis_cursor_matches = camera_list.GetCursor() == camera_cursor_before_actions;
	const bool ellipsis_ok = ellipsis_sent && ellipsis_menu_open && ellipsis_target_matches &&
	                         ellipsis_binding_matches && ellipsis_generation_matches &&
	                         ellipsis_pose_matches && ellipsis_cursor_matches;
	check(ellipsis_ok,
	      "native camera B ellipsis opens actions without loading B or rebinding camera A");
	if(!ellipsis_ok) {
		report << "  ellipsis diagnostics: sent=" << ellipsis_sent << " menu_open=" << ellipsis_menu_open
		       << " target_matches=" << ellipsis_target_matches << " selected_id=" << selected_camera_id
		       << " expected_target=" << camera_b << " binding_matches=" << ellipsis_binding_matches
		       << " binding_id=" << cameras.GetPane(3).camera_id << " expected_binding=" << camera_a
		       << " generation_matches=" << ellipsis_generation_matches
		       << " generation=" << cameras.GetPane(3).generation
		       << " expected_generation=" << camera_before_actions.generation
		       << " pose_matches=" << ellipsis_pose_matches << " cursor_matches=" << ellipsis_cursor_matches
		       << " cursor=" << camera_list.GetCursor() << " expected_cursor=" << camera_cursor_before_actions
		       << " foreground_before_self=" << (ellipsis_foreground_before == GetHWND())
		       << " foreground_after_self=" << (ellipsis_foreground_after == GetHWND())
		       << " foreground_before_hwnd=" << (uint64)(UINT_PTR)ellipsis_foreground_before
		       << " foreground_after_hwnd=" << (uint64)(UINT_PTR)ellipsis_foreground_after
		       << " workspace_hwnd=" << (uint64)(UINT_PTR)GetHWND() << "\n";
	}
	camera_menu.CloseMenu(); pump(1);
	const bool menu_key_sent = send_key(camera_list, VK_F10, false, true);
	check(menu_key_sent && camera_menu.IsMenuOpen() && selected_camera_id == camera_a &&
	      cameras.GetPane(3).camera_id == camera_a && cameras.GetPane(3).generation == camera_before_actions.generation,
	      "native Shift+F10 opens current camera actions without loading or rebinding");
	camera_menu.CloseMenu(); pump(1);
	bool added_cameras = true;
	String last_camera_id;
	for(int i = 0; i < 22; ++i) {
		Scene3DCamera saved = defaults[3]; saved.yaw += .01 * i;
		if(!cameras.AddBookmark(Format("Qualification camera %02d", i + 3), saved, last_camera_id, error)) {
			added_cameras = false; break;
		}
	}
	SyncChrome(); camera_list.ScrollTo(cameras.GetCameraCount() - 1); pump(2);
	const bool scrolled_action_visible = !camera_list.GetItemActionRect(last_camera_id).IsEmpty();
	const bool scrolled_action_sent = added_cameras && click_action(camera_list, last_camera_id);
	check(added_cameras && camera_list.GetScrollPos() > 0 && scrolled_action_visible && scrolled_action_sent &&
	      camera_menu.IsMenuOpen() && selected_camera_id == last_camera_id &&
	      cameras.GetPane(3).camera_id == camera_a && cameras.GetPane(3).generation == camera_before_actions.generation,
	      "native scrolled camera action uses the visible stable ID without loading it");
	camera_menu.CloseMenu();
	CancelGestures();
	if(GetForegroundWindow() == GetHWND()) SetCursorPos(cursor_before.x, cursor_before.y);
	SetRect(workspace_rect); pump(2);
	SetLight(true); SetInspectorVisible(false); ResetDemo();
	check(light && !inspector_visible, "reset demo preserves theme and inspector visibility");
	SetInspectorVisible(true); SetLight(false); ResetDemo(); pump();
	report << "Ui3DViewDemo workspace: " << checks << " checks, " << failures << " failures\n";
	return failures == 0;
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
#endif
