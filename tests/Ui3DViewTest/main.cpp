#include <Ui3DView/Ui3DView.h>
#include <Ui3DView/Ui3DViewContent.h>
#include <RenderPlatformWin32/Win32ChildSurface.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <windows.h>
#include <cmath>
#include <limits>
#include <memory>

using namespace Upp;

namespace {

int checks = 0, failures = 0;
String failure_details;
void Check(bool ok, const char *message)
{
	++checks;
	if(!ok) {
		++failures;
		failure_details << "FAIL: " << message << "\n";
		Cout() << "FAIL: " << message << EOL;
	}
}
bool Near(double a, double b, double epsilon = 1e-9) { return std::abs(a-b) <= epsilon; }
bool Near(const Scene3DVec3& a, const Scene3DVec3& b)
{
	return Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z);
}
void GridPolicyTests()
{
	Scene3DCamera camera; camera.orthographic=true; camera.ortho_height=8;
	Ui3DViewOptions options;
	Ui3DViewGridLayout grid;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) && Near(grid.spacing,0.5),
	      "adaptive grid chooses independently expected half-metre step");
	auto original=grid;
	camera.Orbit(0.2,0.1);
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) && grid==original,
	      "orthographic orbit does not rebuild unchanged grid layout");
	camera.ortho_height=80;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) && Near(grid.spacing,5),
	      "tenfold orthographic span produces five-metre grid step");
	camera.ortho_height=8;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,400),grid) && Near(grid.spacing,1),
	      "wide viewport keeps bounded grid coverage with one-metre step");
	camera.target={1.1,-0.8,4.1};
	options.grid_plane=Ui3DGridPlane::XY;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) &&
	      Near(grid.center_u,1) && Near(grid.center_v,-1) && Near(grid.ToWorld(1,-1),{1,-1,0}),
	      "XY grid snaps target and maps independent world coordinates");
	options.grid_plane=Ui3DGridPlane::YZ;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) &&
	      Near(grid.center_u,-1) && Near(grid.center_v,4) && Near(grid.ToWorld(-1,4),{0,-1,4}),
	      "YZ grid snaps target and maps independent world coordinates");
	options.grid_plane=Ui3DGridPlane::XZ;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) &&
	      Near(grid.center_u,1) && Near(grid.center_v,4) && Near(grid.ToWorld(1,4),{1,0,4}),
	      "XZ grid snaps target and maps independent world coordinates");
	original=grid; camera.target={1.11,-0.81,4.11};
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) && grid==original,
	      "small pan inside a grid cell preserves retained layout");
	options.adaptive_grid=false; options.grid_spacing=2;
	Check(ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) &&
	      Near(grid.spacing,2) && Near(grid.center_u,0) && Near(grid.center_v,0),
	      "fixed grid retains authored spacing and origin");
	options.grid_plane=static_cast<Ui3DGridPlane>(999);
	Check(!ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid) &&
	      grid.plane==Ui3DGridPlane::XZ && grid.spacing==1 && grid.center_u==0 && grid.center_v==0,
	      "invalid grid plane rejects and clears output");
	options.grid_plane=Ui3DGridPlane::XZ; options.grid_spacing=std::numeric_limits<double>::quiet_NaN();
	Check(!ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid),"nonfinite grid spacing rejected");
	options.grid_spacing=1;
	Check(!ComputeUi3DViewGridLayout(camera,options,Size(0,800),grid),"zero-sized grid viewport rejected");
	camera.revision=0;
	Check(!ComputeUi3DViewGridLayout(camera,options,Size(800,800),grid),"invalid camera rejected by grid policy");
}
void Pump(int count = 10)
{
	// GuiSleep returns early when any Windows message arrives. Measure elapsed
	// wall time; unsigned subtraction also handles the GetTickCount wraparound.
	const DWORD started = GetTickCount();
	const DWORD duration = (DWORD)max(0, count) * 10;
	for(;;) {
		Ctrl::ProcessEvents();
		const DWORD elapsed = GetTickCount() - started;
		if(elapsed >= duration) break;
		Ctrl::GuiSleep(min(10, int(duration - elapsed)));
	}
	Ctrl::ProcessEvents();
}
std::shared_ptr<const Scene3DSnapshot> Scene(uint64 revision, const String& id)
{
	String error;
	Scene3DSceneDesc desc; desc.revision = revision;
	Scene3DObjectDesc object; object.id = id;
	object.mesh = Scene3DMesh::MakeBox(Scene3DVec3(2, 2, 2), error);
	object.r = 0.4f; object.g = 0.65f; object.b = 0.8f;
	desc.objects.push_back(object);
	auto scene = Scene3DSnapshot::Create(desc, error);
	Check(scene != nullptr && error.IsEmpty(), "fixture scene validates");
	return scene;
}
HWND Host(Ui3DView& view)
{
	auto *native = dynamic_cast<Win32ChildSurface *>(view.GetFirstChild());
	return native ? native->GetHWND() : nullptr;
}
struct NativeMotionProbe {
	Point expected, last;
	int after_serial = 0;
	int delivered = 0;
	bool saw_expected = false;
};
class TestView : public Ui3DView {
public:
	int mouse_moves = 0;
	Point last_mouse = Point(0, 0);
	int left_downs = 0, left_ups = 0, left_doubles = 0;
	Point last_left_down = Null, last_left_up = Null;
	dword left_down_flags = 0, left_up_flags = 0;
	void LeftDown(Point p, dword flags) override {
		++left_downs; last_left_down = p; left_down_flags = flags;
		Ui3DView::LeftDown(p, flags);
	}
	void LeftUp(Point p, dword flags) override {
		++left_ups; last_left_up = p; left_up_flags = flags;
		Ui3DView::LeftUp(p, flags);
	}
	void LeftDouble(Point p, dword flags) override {
		++left_doubles;
		Ui3DView::LeftDouble(p, flags);
	}
	std::shared_ptr<NativeMotionProbe> native_motion_probe;
	void MouseMove(Point p, dword flags) override {
		++mouse_moves;
		last_mouse = p;
		const auto probe = native_motion_probe;
		if(probe && mouse_moves > probe->after_serial) {
			++probe->delivered;
			probe->last = p;
			if(abs(p.x - probe->expected.x) <= 1 && abs(p.y - probe->expected.y) <= 1)
				probe->saw_expected = true;
		}
		Ui3DView::MouseMove(p, flags); // A callback may delete this; access nothing after.
	}
};
bool test_middle_held = false, test_left_held = false;
int native_presses = 0, native_releases = 0, cleanup_releases = 0;
int foreground_requests = 0, caption_activations = 0;
void ReleaseButtons()
{
	// Only a successfully inserted down owns a matching up. Cleanup remains
	// valid after focus loss, but must never release unrelated physical input.
	auto release = [&](bool& held, DWORD flag) {
		if(!held) return;
		INPUT input {};
		input.type = INPUT_MOUSE; input.mi.dwFlags = flag;
		const bool inserted = SendInput(1, &input, sizeof(INPUT)) == 1;
		Check(inserted, "paired native cleanup release inserted");
		if(inserted) { held = false; ++native_releases; ++cleanup_releases; }
	};
	release(test_left_held, MOUSEEVENTF_LEFTUP);
	release(test_middle_held, MOUSEEVENTF_MIDDLEUP);
	Pump(2);
}

void ForegroundDiagnostic(HWND root, const char *phase, bool requested)
{
	HWND foreground = GetForegroundWindow();
	DWORD target_pid = 0, foreground_pid = 0;
	GetWindowThreadProcessId(root, &target_pid);
	GetWindowThreadProcessId(foreground, &foreground_pid);
	GUITHREADINFO info {}; info.cbSize = sizeof(info);
	const bool have_info = GetGUIThreadInfo(GetCurrentThreadId(), &info);
	String detail;
	detail << "Native foreground: phase=" << phase << " request_returned=" << requested
	       << " target=" << uint64(reinterpret_cast<UINT_PTR>(root)) << " target_pid=" << target_pid
	       << " foreground=" << uint64(reinterpret_cast<UINT_PTR>(foreground))
	       << " foreground_pid=" << foreground_pid
	       << " visible=" << bool(IsWindowVisible(root)) << " enabled=" << bool(IsWindowEnabled(root))
	       << " iconic=" << bool(IsIconic(root)) << " gui_info=" << have_info
	       << " active=" << uint64(reinterpret_cast<UINT_PTR>(info.hwndActive))
	       << " focus=" << uint64(reinterpret_cast<UINT_PTR>(info.hwndFocus))
	       << " capture=" << uint64(reinterpret_cast<UINT_PTR>(info.hwndCapture));
	Cout() << detail << EOL;
	failure_details << detail << "\n";
}

bool OwnInputWindow(HWND root)
{
	DWORD pid = 0;
	return root && IsWindow(root) && GetAncestor(root, GA_ROOT) == root &&
	       GetWindowThreadProcessId(root, &pid) == GetCurrentThreadId() &&
	       pid == GetCurrentProcessId() && IsWindowVisible(root) &&
	       IsWindowEnabled(root) && !IsIconic(root);
}

bool MakeNativeMotion(INPUT& input, POINT point)
{
	const int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	if(width <= 1 || height <= 1 || point.x < x || point.y < y ||
	   point.x >= x + width || point.y >= y + height) return false;
	input = INPUT{}; input.type = INPUT_MOUSE;
	input.mi.dx = (LONG)(((int64)(point.x - x) * 65536 + 32768) / width);
	input.mi.dy = (LONG)(((int64)(point.y - y) * 65536 + 32768) / height);
	input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
	                   MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE_NOCOALESCE;
	return true;
}

bool AcquireTestForeground(HWND root, const char *phase, bool exercise_caption = false)
{
	if(!OwnInputWindow(root)) return false;
	if(!exercise_caption && GetForegroundWindow() == root) return true;
	bool requested = false;
	if(!exercise_caption) {
		++foreground_requests;
		requested = SetForegroundWindow(root);
		for(int i = 0; i < 50 && OwnInputWindow(root) && GetForegroundWindow() != root; ++i) Pump(1);
		if(OwnInputWindow(root) && GetForegroundWindow() == root) return true;
	}
	ForegroundDiagnostic(root, phase, requested);
	// A background-launched test may be refused SetForegroundWindow. Use the
	// normal activation click on our own caption, never an Alt-key trick,
	// attached input queue, global foreground-policy change or another app.
	if(!OwnInputWindow(root) || test_left_held || test_middle_held) return false;
	const int keys[] = { VK_LBUTTON, VK_MBUTTON, VK_RBUTTON, VK_XBUTTON1, VK_XBUTTON2,
	                     VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN };
	for(int key : keys) if(GetAsyncKeyState(key) & 0x8000) return false;
	GUITHREADINFO foreground {}; foreground.cbSize = sizeof(foreground);
	if(!GetGUIThreadInfo(0, &foreground) || foreground.hwndCapture) return false;
	RECT bounds {}; POINT client = { 0, 0 };
	if(!GetWindowRect(root, &bounds) || !ClientToScreen(root, &client) || client.y <= bounds.top) return false;
	// Separate successive activation clicks so they cannot form a caption
	// double-click and accidentally maximize or restore the fixture.
	const int offset = max(DPI(12), 2 * GetSystemMetrics(SM_CXDOUBLECLK));
	const POINT point = { (bounds.left + bounds.right) / 2 + (caption_activations & 1 ? offset : -offset),
	                      (bounds.top + client.y) / 2 };
	auto own_caption = [&] {
		return OwnInputWindow(root) && GetAncestor(WindowFromPoint(point), GA_ROOT) == root &&
		       SendMessage(root, WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y)) == HTCAPTION;
	};
	INPUT input[3] {};
	if(!MakeNativeMotion(input[0], point) || !own_caption()) return false;
	input[1].type = input[2].type = INPUT_MOUSE;
	input[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	input[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	// One serial batch keeps the move and its paired click together. A partial
	// batch with only the down inserted still owns cleanup, and still fails.
	const UINT inserted = SendInput(3, input, sizeof(INPUT));
	if(inserted >= 2) { test_left_held = true; ++native_presses; }
	if(inserted == 3) { test_left_held = false; ++native_releases; }
	if(inserted != 3) { ReleaseButtons(); return false; }
	for(int i = 0; i < 50 && OwnInputWindow(root) && GetForegroundWindow() != root; ++i) Pump(1);
	const bool acquired = OwnInputWindow(root) && GetForegroundWindow() == root;
	if(acquired) ++caption_activations;
	else ForegroundDiagnostic(root, "caption activation failed", requested);
	return acquired;
}

void Mouse(TestView& view, UINT message, WPARAM, Point p)
{
	Ptr<TestView> alive(&view);
	auto abort = [&](const char *reason) { Check(false, reason); ReleaseButtons(); };
	if((message == WM_MBUTTONDOWN && test_middle_held) ||
	   (message == WM_LBUTTONDOWN && test_left_held)) ReleaseButtons();
	if(!alive) { abort("test control survived paired cleanup"); return; }
	HWND host = Host(view);
	Check(host && IsWindow(host), "real native input host exists");
	if(!host || !IsWindow(host)) { ReleaseButtons(); return; }
	const HWND root = GetAncestor(host, GA_ROOT);
	if(message == WM_MBUTTONDOWN || message == WM_LBUTTONDOWN) {
		if(!AcquireTestForeground(root, "viewport press")) {
			abort("test window foreground required before mouse input"); return;
		}
	}
	if(!alive) { abort("test control survived activation"); return; }
	if(GetForegroundWindow() != root) {
		ForegroundDiagnostic(root, "viewport input lost foreground", false);
		abort("test window foreground required before mouse input"); return;
	}
	// Activation pumps events. Revalidate the host and read its current bounds
	// after that pump instead of injecting coordinates from the old rectangle.
	RECT rect;
	if(Host(view) != host || !GetWindowRect(host, &rect)) { abort("native bounds available"); return; }
	const POINT position = { rect.left + p.x, rect.top + p.y };
	if(GetAncestor(WindowFromPoint(position), GA_ROOT) != root) {
		abort("mouse injection target remains the test window"); return;
	}
	auto move = [&](bool prepare_press) {
		const int moves = view.mouse_moves;
		INPUT motion[2] {};
		const UINT count = prepare_press ? 2 : 1;
		if(prepare_press) {
			// A caption click can leave U++'s IgnoreMouseUp latch set. The first
			// normal client move clears it and is consumed. Two distinct native
			// hover moves prove client delivery has resumed before pressing.
			const POINT lead = { position.x + (position.x > rect.left ? -1 : 1), position.y };
			if(!PtInRect(&rect, lead) || GetAncestor(WindowFromPoint(lead), GA_ROOT) != root ||
			   !MakeNativeMotion(motion[0], lead)) { abort("owned native hover target available"); return false; }
		}
		if(!MakeNativeMotion(motion[count - 1], position)) {
			abort("virtual desktop dimensions available"); return false;
		}
		// Keep observation outside the control's lifetime: the expected native
		// event must still be proved when its navigation callback destroys it.
		auto probe = std::make_shared<NativeMotionProbe>();
		probe->expected = p;
		probe->last = view.last_mouse;
		probe->after_serial = moves;
		view.native_motion_probe = probe;
		const bool inserted = SendInput(count, motion, sizeof(INPUT)) == count;
		Check(inserted, "native motion injected");
		// ProcessEvents can deliver the requested point and a later move in one
		// pump. Latch actual delivery instead of requiring the final global mouse
		// position to remain stationary. The same exact +/-1 tolerance applies.
		for(int i = 0; i < 50 && alive && !probe->saw_expected; ++i) Pump(1);
		Check(probe->delivered > 0, "native motion reached the control");
		const bool coordinates_match = probe->saw_expected;
		if(!coordinates_match) {
			POINT cursor {};
			RECT live_bounds {};
			const bool have_cursor = GetCursorPos(&cursor);
			const bool have_bounds = IsWindow(host) && GetWindowRect(host, &live_bounds);
			const Rect logical_bounds = alive ? view.GetScreenView() : Rect(0, 0, 0, 0);
			String detail = Format("Native motion diagnostic: expected=(%d,%d) observed=(%d,%d) moves=%d previous=%d",
			                       p.x, p.y, probe->last.x, probe->last.y,
			                       moves + probe->delivered, moves);
			detail << Format(" original_host=(%d,%d,%d,%d) live_host_valid=%d live_host=(%d,%d,%d,%d)",
			                 int(rect.left), int(rect.top), int(rect.right), int(rect.bottom), int(have_bounds),
			                 int(live_bounds.left), int(live_bounds.top), int(live_bounds.right), int(live_bounds.bottom));
			detail << Format(" logical_view=(%d,%d,%d,%d) cursor_valid=%d cursor=(%d,%d)",
			                 logical_bounds.left, logical_bounds.top, logical_bounds.right, logical_bounds.bottom,
			                 int(have_cursor), int(cursor.x), int(cursor.y));
			detail << " control_alive=" << bool(alive) << " expected_seen=" << probe->saw_expected
			       << " capture_is_view=" << (alive && Ctrl::GetCaptureCtrl() == &view)
			       << " foreground_is_root=" << (GetForegroundWindow() == root)
			       << " cursor_root_matches=" << (have_cursor && GetAncestor(WindowFromPoint(cursor), GA_ROOT) == root);
			Cout() << detail << EOL;
			failure_details << detail << "\n";
		}
		Check(coordinates_match, "native motion carries the requested control coordinates");
		if(alive && view.native_motion_probe == probe) view.native_motion_probe.reset();
		if(!inserted || !coordinates_match) ReleaseButtons();
		return inserted && coordinates_match;
	};
	if(message == WM_MOUSEMOVE) { move(false); return; }
	if(message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN) {
		if(!move(true)) return;
	}
	RECT live_bounds {};
	if(!alive || GetForegroundWindow() != root ||
	   Host(view) != host || !GetWindowRect(host, &live_bounds) ||
	   live_bounds.left + p.x != position.x || live_bounds.top + p.y != position.y ||
	   GetAncestor(WindowFromPoint(position), GA_ROOT) != root) {
		abort("mouse injection target remains the test window"); return;
	}
	INPUT input {};
	// Carry the absolute position in the button event itself. Event pumping can
	// observe later cursor motion after the requested hover was delivered; a
	// positionless button must not use that unrelated global cursor position.
	if(!MakeNativeMotion(input, position)) { abort("native button position available"); return; }
	bool *held = nullptr, down = false;
	switch(message) {
	case WM_LBUTTONDOWN: input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN; held = &test_left_held; down = true; break;
	case WM_LBUTTONUP: input.mi.dwFlags |= MOUSEEVENTF_LEFTUP; held = &test_left_held; break;
	case WM_MBUTTONDOWN: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN; held = &test_middle_held; down = true; break;
	case WM_MBUTTONUP: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP; held = &test_middle_held; break;
	default: return;
	}
	if(!down && !*held) return; // The matching press was refused; no orphan up.
	const bool inserted = SendInput(1, &input, sizeof(input)) == 1;
	Check(inserted, "native input injected");
	if(inserted) { *held = down; if(down) ++native_presses; else ++native_releases; }
	else ReleaseButtons();
	Pump(2);
}
struct Events {
	int begins = 0, updates = 0, commits = 0, cancels = 0;
	void On(const Ui3DNavigationEvent& event) {
		switch(event.phase) {
		case Ui3DNavigationPhase::Begin: ++begins; break;
		case Ui3DNavigationPhase::Update: ++updates; break;
		case Ui3DNavigationPhase::Commit: ++commits; break;
		case Ui3DNavigationPhase::Cancel: ++cancels; break;
		}
	}
};
class TestWindow : public TopWindow {
public:
	TestView first, second;
	int raw_left_downs = 0, raw_left_ups = 0;
	LRESULT WindowProc(UINT message, WPARAM wparam, LPARAM lparam) override {
		if(message == WM_LBUTTONDOWN) ++raw_left_downs;
		if(message == WM_LBUTTONUP) ++raw_left_ups;
		return TopWindow::WindowProc(message, wparam, lparam);
	}
	TestWindow() {
		Title("Ui3DView finite regression");
		SetRect(60, 60, 820, 420);
		Add(first); Add(second);
		first.SetValidation(); second.SetValidation();
	}
	void Layout() override {
		Size s = GetSize();
		first.SetRect(0, 0, s.cx / 2, s.cy);
		second.SetRect(s.cx / 2, 0, s.cx - s.cx / 2, s.cy);
	}
};

void ViewCommandInputTests(TestWindow& window, const std::shared_ptr<const Scene3DSnapshot>& scene,
                           const std::shared_ptr<const Scene3DSnapshot>& next_scene,
                           const Scene3DCamera& known_camera, Events& events)
{
	String error;
	auto& view=window.first;
	const Point center(view.GetSize().cx/2,view.GetSize().cy/2);
	ReleaseButtons();
	view.SetCamera(known_camera,error);
	int commits=events.commits;
	const auto empty_fit_camera=view.GetCamera();
	Check(!view.FitBounds(Scene3DBounds()) && view.GetCamera().revision==empty_fit_camera.revision,
	      "empty supplied fit bounds reject without camera mutation");
	Check(view.FitBounds(scene->GetBounds()) && events.commits==commits,
	      "programmatic supplied-bounds framing emits no user commit");
	view.SetCamera(known_camera,error);
	Check(view.SetNavigationTool(Ui3DNavigationTool::Orbit,error),"select left orbit tool");
	auto before=view.GetCamera();
	view.LeftDown(center,0);
	Check(view.IsNavigating() && view.HasCapture(),"left orbit tool begins a captured gesture");
	Check(view.SetNavigationTool(Ui3DNavigationTool::Pan,error) && view.IsNavigating(),
	      "tool changes do not cancel an active gesture");
	Check(!view.SetNavigationTool(static_cast<Ui3DNavigationTool>(999),error) && view.IsNavigating(),
	      "invalid tool cannot disturb an active gesture");
	view.MouseMove(center+Point(24,8),0);
	Check(view.GetCamera().yaw!=before.yaw && Near(view.GetCamera().target,before.target),
	      "active left orbit retains its pressed mode after tool changes");
	view.LeftUp(center+Point(24,8),0);
	Check(!view.IsNavigating() && events.commits==commits+1,"left orbit commits once on release");
	view.SetCamera(known_camera,error); before=view.GetCamera(); commits=events.commits;
	view.LeftDown(center,0); // The next press uses the newly selected Pan tool.
	view.SetNavigationTool(Ui3DNavigationTool::Select,error);
	view.MouseMove(center+Point(24,8),0);
	Check(view.GetCamera().yaw==before.yaw && !Near(view.GetCamera().target,before.target),
	      "active left pan remains pan after Select is chosen");
	view.LeftUp(center+Point(24,8),0);
	Check(events.commits==commits+1 && !view.IsNavigating(),"left pan commits once");
	view.SetCamera(known_camera,error); view.SetSelection({},error);
	int pending_selections=0;
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest& request) {
		++pending_selections;
		if(request.pick.hit) view.SetSelection({request.pick.id},error);
	};
	view.LeftDown(center,0);
	view.SetNavigationTool(Ui3DNavigationTool::Orbit,error);
	view.LeftUp(center,0);
	Check(pending_selections==1 && view.GetSelection().size()==1 && !view.IsNavigating(),
	      "pending Select click is not reinterpreted after toolbar tool change");
	view.WhenSelectionRequest.Clear();
	view.SetNavigationTool(Ui3DNavigationTool::Pan,error); before=view.GetCamera();
	view.LeftDown(center,K_ALT); view.MouseMove(center+Point(20,0),0);
	Check(view.GetCamera().yaw!=before.yaw && Near(view.GetCamera().target,before.target),
	      "Alt-left remains orbit regardless of selected left tool");
	view.CancelNavigation("test completed");
	view.SetCamera(known_camera,error);
	commits=events.commits; before=view.GetCamera();
	view.LeftDouble(center,0);
	Check(view.IsNavigating() && events.commits==commits && view.GetCamera().revision==before.revision,
	      "Pan tool double-press begins navigation without framing");
	view.CancelNavigation("test completed");
	view.SetNavigationTool(Ui3DNavigationTool::Select,error);
	view.SetCamera(known_camera,error);

	// Extending a selection can include distant objects. Double-click still frames
	// this exact geometric hit rather than all selected objects or the whole scene.
	Scene3DSceneDesc description; description.revision=10;
	Scene3DObjectDesc central; central.id="test:box"; central.mesh=scene->GetObjects()[0].mesh;
	description.objects.push_back(central);
	Scene3DObjectDesc distant=central; distant.id="test:far";
	distant.local=Scene3DMatrix::Translation({20,0,0}); description.objects.push_back(distant);
	auto double_scene=Scene3DSnapshot::Create(description,error);
	Check(double_scene!=nullptr,"double-click fixture validates");
	if(!double_scene) return;
	view.SetScene(double_scene,error); view.SetSelection({"test:far"},error);
	std::vector<int> ordered;
	view.WhenNavigation=[&](const Ui3DNavigationEvent& event) {
		events.On(event);
		if(event.phase==Ui3DNavigationPhase::Commit) ordered.push_back(2);
	};
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest& request) {
		ordered.push_back(1);
		Check(request.pick.hit && request.pick.id=="test:box" && request.extend,
		      "Select double-click requests the actual hit and modifiers");
		view.SetSelection({"test:box","test:far"},error);
	};
	commits=events.commits;
	view.LeftDouble(center,K_SHIFT);
	Check(ordered.size()==2 && ordered[0]==1 && ordered[1]==2 && events.commits==commits+1,
	      "Select double-click selects before its single camera commit");
	Check(Near(view.GetCamera().target,{0,0,0}) && view.GetCamera().distance<known_camera.distance &&
	      view.GetSelection().size()==2,"double-click frames hit bounds rather than distant multi-selection");
	view.WhenNavigation=[&events](const Ui3DNavigationEvent& event) { events.On(event); };
	view.SetCamera(known_camera,error);
	int miss_requests=0;
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest& request) {
		++miss_requests; Check(!request.pick.hit,"double-click miss remains a selection request");
	};
	commits=events.commits; before=view.GetCamera();
	view.LeftDouble(Point(5,5),0);
	Check(miss_requests==1 && events.commits==commits && view.GetCamera().revision==before.revision,
	      "double-click miss does not frame or commit a camera change");
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest&) { view.SetScene(next_scene,error); };
	commits=events.commits; before=view.GetCamera();
	view.LeftDouble(center,0);
	Check(view.GetScene()==next_scene && view.GetCamera().revision==before.revision && events.commits==commits,
	      "selection callback scene rebind prevents stale double-click framing");
	view.SetScene(scene,error); view.SetCamera(known_camera,error);
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest&) {
		auto replacement=view.GetCamera(); replacement.distance=9; view.SetCamera(replacement,error);
	};
	commits=events.commits;
	view.LeftDouble(center,0);
	Check(view.GetCamera().distance==9 && events.commits==commits,
	      "selection callback camera replacement is not overwritten by double-click fit");
	view.SetCamera(known_camera,error);
	const Rect old_rect=view.GetRect();
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest&) {
		view.SetRect(old_rect.left,old_rect.top,old_rect.Width()+2,old_rect.Height());
	};
	commits=events.commits; before=view.GetCamera();
	view.LeftDouble(center,0);
	Check(view.GetCamera().revision==before.revision && events.commits==commits,
	      "callback resize invalidates the double-click projection before framing");
	view.SetRect(old_rect);
	view.WhenSelectionRequest=[&](const Ui3DSelectionRequest&) { view.MiddleDown(center,0); };
	commits=events.commits; before=view.GetCamera();
	view.LeftDouble(center,0);
	Check(view.IsNavigating() && view.GetCamera().revision==before.revision && events.commits==commits,
	      "callback-started gesture supersedes pending double-click framing");
	view.CancelNavigation("test completed"); view.WhenSelectionRequest.Clear();
	view.SetCamera(known_camera,error);
	view.MiddleDown(center,0);
	view.WhenNavigation=[&](const Ui3DNavigationEvent& event) {
		events.On(event);
		if(event.phase==Ui3DNavigationPhase::Cancel) {
			auto replacement=view.GetCamera(); replacement.distance=9; view.SetCamera(replacement,error);
			auto options=view.GetOptions(); options.grid_plane=Ui3DGridPlane::YZ; view.SetOptions(options,error);
		}
	};
	Check(!view.SetViewDirection(Scene3DViewDirection::Front,error),
	      "named direction detects camera rebind during its cancellation callback");
	Check(view.GetCamera().distance==9 && !view.GetCamera().orthographic &&
	      view.GetOptions().grid_plane==Ui3DGridPlane::YZ,"rejected named direction preserves callback camera and grid atomically");
	view.WhenNavigation=[&events](const Ui3DNavigationEvent& event) { events.On(event); };
	view.SetCamera(known_camera,error); view.SetOptions(Ui3DViewOptions::Dark(),error);

	One<TestView> victim;
	victim.Create(); victim->SetScene(scene,error); victim->SetCamera(known_camera,error);
	window.Add(victim->LeftPos(10,140).TopPos(10,140)); Pump(10);
	int destroyed_commits=0;
	victim->WhenNavigation=[&](const Ui3DNavigationEvent& event) {
		if(event.phase==Ui3DNavigationPhase::Commit) ++destroyed_commits;
	};
	victim->WhenSelectionRequest=[&](const Ui3DSelectionRequest&) { victim.Clear(); };
	victim->LeftDouble(Point(70,70),0);
	Check(!victim && destroyed_commits==0,"selection callback can destroy a double-clicking view before any fit commit");
	victim.Clear();
	view.WhenSelectionRequest.Clear();
	view.SetScene(scene,error); view.SetCamera(known_camera,error);
	view.SetOptions(Ui3DViewOptions::Dark(),error);
	view.SetNavigationTool(Ui3DNavigationTool::Select,error);
	view.SetSelection({"test:box"},error);
	ReleaseButtons();
}
void ToolPointerTests(TestWindow& window, const std::shared_ptr<const Scene3DSnapshot>& scene,
                      const std::shared_ptr<const Scene3DSnapshot>& replacement)
{
	auto& view = window.first;
	String error;
	auto navigation = view.WhenNavigation;
	auto selection = view.WhenSelectionRequest;
	const auto camera = view.GetCamera();
	int nav = 0, picks = 0, begins = 0, updates = 0, commits = 0, cancels = 0;
	uint64 id = 0;
	Ui3DToolPointerEvent last;
	view.WhenNavigation = [&](const Ui3DNavigationEvent&) { ++nav; };
	view.WhenSelectionRequest = [&](const Ui3DSelectionRequest&) { ++picks; };
	auto handler = [&](const Ui3DToolPointerEvent& event) {
		last = event;
		switch(event.phase) {
		case Ui3DToolPointerPhase::Begin: ++begins; id = event.gesture_id; break;
		case Ui3DToolPointerPhase::Update: ++updates; break;
		case Ui3DToolPointerPhase::Commit:
			++commits;
			Check(!view.HasCapture() && !view.IsToolPointerActive(), "tool commit observes released capture");
			Check(view.GetScene() == scene && view.Pick(last.point).id != "test:preview",
			      "commit clears preview before host callback");
			break;
		case Ui3DToolPointerPhase::Cancel: ++cancels; break;
		}
		Check(event.gesture_id == id && event.scene == scene &&
		      event.start_pick.scene_revision == scene->GetRevision(),
		      "tool events retain originating identity and scene");
		return true;
	};
	const Point p(view.GetSize().cx/2, view.GetSize().cy/2);
	view.SetNavigationTool(Ui3DNavigationTool::Orbit, error);
	view.WhenToolPointer = handler;
	view.LeftDown(p, 0);
	Check(view.IsToolPointerActive() && view.HasCapture() && !view.IsNavigating(),
	      "tool accepts before selected orbit navigation");
	const uint64 authoritative_binding = view.GetSceneBindingRevision();
	const auto original_pick = view.Pick(p);
	auto preview = Scene(scene->GetRevision(), "test:preview");
	Check(view.SetToolPointerPreviewScene(last, preview, error) &&
	      view.IsToolPointerActive() && view.GetScene() == scene &&
	      view.GetSceneBindingRevision() == authoritative_binding,
	      "gesture preview preserves authoritative scene and binding");
	const auto preview_pick = view.Pick(p);
	Check(preview_pick.hit && preview_pick.id == "test:preview" &&
	      view.IsCurrentPick(preview_pick) && !view.IsCurrentPick(original_pick),
	      "preview picks current geometry and invalidates equal-revision original pick");
	auto wrong_token = last; ++wrong_token.gesture_id;
	Check(!view.SetToolPointerPreviewScene(wrong_token, replacement, error) &&
	      view.IsCurrentPick(preview_pick), "stale token rejection preserves active preview");
	Check(!window.second.SetToolPointerPreviewScene(last, preview, error),
	      "another view cannot accept the gesture token");
	preview.reset();
	view.MiddleDown(p, 0);
	view.MouseWheel(p, 120, 0);
	view.MouseMove(p+Point(12,3), K_SHIFT);
	Check(begins == 1 && updates == 1 && last.point == p+Point(12,3) &&
	      last.keyflags == K_SHIFT && last.ray_valid &&
	      last.camera.revision == camera.revision, "tool update uses frozen projection and current pointer");
	// Replacing the public callback cannot redirect an in-flight transaction.
	view.WhenToolPointer = [](const Ui3DToolPointerEvent&) { return false; };
	view.LeftUp(p+Point(12,3), 0);
	Check(commits == 1 && cancels == 0 && nav == 0 && picks == 0 &&
	      view.GetCamera().revision == camera.revision, "tool consumes release without navigation or selection");
	Check(!view.IsCurrentPick(preview_pick) && view.IsCurrentPick(original_pick) &&
	      !view.SetToolPointerPreviewScene(last, replacement, error),
	      "terminal preview cleared and ended token rejected");
	view.LeftDown(p, 0); // Declined Begin falls through to the selected Orbit tool.
	Check(view.IsNavigating() && !view.IsToolPointerActive(), "declined tool preserves navigation");
	view.CancelNavigation();
	view.SetNavigationTool(Ui3DNavigationTool::Select, error);
	view.LeftDown(p, 0); view.LeftUp(p, 0);
	Check(picks == 1, "declined tool preserves release selection");

	view.WhenToolPointer = handler;
	view.LeftDown(p, 0);
	Check(view.SetToolPointerPreviewScene(last, replacement, error), "install cancel fixture preview");
	view.Key(K_ESCAPE, 1); view.LeftUp(p, 0);
	Check(view.GetScene() == scene && view.Pick(p).id == "test:box",
	      "Escape restores original displayed geometry");
	Check(cancels == 1 && commits == 1 && !view.HasCapture() && picks == 1,
	      "Escape cancels once and suppresses release selection");
	view.LeftDown(p, 0); view.LostFocus();
	Check(cancels == 2 && !view.IsToolPointerActive(), "focus loss cancels tool");
	view.LeftDown(p, 0); view.ReleaseCapture();
	Check(cancels == 2 && !view.IsToolPointerActive(), "capture-loss notification is deferred");
	Pump(2);
	Check(cancels == 3, "capture loss delivers one deferred tool cancel");
	view.LeftDown(p, 0); view.SetScene(replacement, error);
	Check(cancels == 4 && last.scene == scene && !view.HasCapture(), "rebind cancels with retained old scene");
	view.SetScene(scene, error);
	view.LeftDown(p, 0); view.SetCamera(camera, error);
	Check(cancels == 5, "camera replacement cancels tool");
	view.WhenToolPointer = [&](const Ui3DToolPointerEvent& event) {
		handler(event);
		return event.phase != Ui3DToolPointerPhase::Update;
	};
	view.LeftDown(p, 0); view.MouseMove(p+Point(1,0), 0);
	Check(cancels == 6 && last.reason == "update rejected" && !view.HasCapture(),
	      "rejected update cancels and releases capture");
	view.WhenToolPointer = [&](const Ui3DToolPointerEvent& event) {
		handler(event);
		if(event.phase == Ui3DToolPointerPhase::Begin) view.SetScene(replacement, error);
		return true;
	};
	view.LeftDown(p, 0);
	Check(cancels == 7 && !view.IsToolPointerActive(), "reentrant Begin rebind cancels without starting navigation");
	view.SetScene(scene, error);
	One<TestView> victim;
	victim.Create();
	window.Add(victim->LeftPos(10,100).TopPos(10,100));
	victim->WhenToolPointer = [&](const Ui3DToolPointerEvent& event) {
		if(event.phase == Ui3DToolPointerPhase::Begin) victim.Clear();
		return true;
	};
	victim->LeftDown(Point(50,50), 0);
	Check(!victim, "tool Begin callback can destroy captured control safely");
	// No synthetic terminal callback may escape a partially destroyed control.
	Check(!view.IsToolPointerActive(), "destroyed peer leaves main tool idle");
	view.WhenToolPointer.Clear();
	view.SetCamera(camera, error);
	view.WhenNavigation = navigation;
	view.WhenSelectionRequest = selection;
}
}

GUI_APP_MAIN
{
	POINT original_cursor; GetCursorPos(&original_cursor);
	struct InputCleanup { ~InputCleanup() { ReleaseButtons(); } } input_cleanup;
	String error;
	GridPolicyTests();
	auto scene = Scene(1, "test:box");
	auto next_scene = Scene(2, "test:replacement");
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	{
		TestWindow window;
		Check(window.first.SetScene(scene, error), "bind retained scene");
		Check(window.second.SetScene(scene, error), "two views share immutable CPU mesh");
		Events events;
		window.first.WhenNavigation = [&](const Ui3DNavigationEvent& e) { events.On(e); };
		Scene3DCamera camera = window.first.GetCamera();
		camera.distance = 6; camera.yaw = 0; camera.pitch = 0;
		Check(window.first.SetCamera(camera, error), "set known camera");
		Check(window.second.SetCamera(camera, error), "set second camera");
		const auto valid = window.first.GetCamera();
		camera.near_plane = -1;
		Check(!window.first.SetCamera(camera, error), "reject invalid near plane");
		Check(window.first.GetCamera().near_plane == valid.near_plane &&
		      window.first.GetCamera().revision == valid.revision,
		      "rejected camera preserves usable state and revision");
		camera = valid; camera.distance = std::numeric_limits<double>::quiet_NaN();
		Check(!window.first.SetCamera(camera, error), "reject nonfinite camera");
		auto options = window.first.GetOptions(); options.grid_extent = 0;
		Check(!window.first.SetOptions(options, error), "reject invalid options");
		Check(window.first.GetOptions().grid_extent == 10, "rejected options preserve state");
		Check(window.first.SetSelection({"test:box"}, error), "set app-owned selection");
		Check(!window.first.SetSelection({"test:missing"}, error), "reject absent selection ID");
		Check(window.first.GetSelection().size() == 1 &&
		      window.first.GetSelection()[0] == "test:box", "invalid selection is atomic");
		Check(!window.first.SetSelection({"test:box", "test:box"}, error), "reject duplicate IDs");
		Check(window.first.SetDisplayMode(Ui3DDisplayMode::SolidEdges,error) &&
		      window.first.GetDisplayMode()==Ui3DDisplayMode::SolidEdges &&
		      !window.first.GetOptions().wireframe && window.first.GetOptions().solid_edges,
		      "solid-with-edges mode normalizes legacy options");
		Check(window.first.SetDisplayMode(Ui3DDisplayMode::Wireframe,error) &&
		      window.first.GetDisplayMode()==Ui3DDisplayMode::Wireframe &&
		      !window.first.GetOptions().solid_edges,"wireframe mode clears solid-edge mode");
		options=window.first.GetOptions(); options.solid_edges=true;
		Check(window.first.SetOptions(options,error) && window.first.GetDisplayMode()==Ui3DDisplayMode::Wireframe,
		      "legacy wireframe flag takes precedence when both flags are authored");
		Check(!window.first.SetDisplayMode(static_cast<Ui3DDisplayMode>(999),error) &&
		      window.first.GetDisplayMode()==Ui3DDisplayMode::Wireframe,"invalid display enum preserves mode");
		options=window.first.GetOptions(); const auto good_edge=options.edge_color;
		options.edge_color=Rgba8(10,20,30,128);
		Check(!window.first.SetOptions(options,error) && window.first.GetOptions().edge_color==good_edge,
		      "transparent edge palette rejects atomically");
		options=window.first.GetOptions(); options.grid_plane=static_cast<Ui3DGridPlane>(999);
		Check(!window.first.SetOptions(options,error) && window.first.GetOptions().grid_plane==Ui3DGridPlane::XZ,
		      "invalid grid enum preserves options");
		const Scene3DViewDirection directions[]={Scene3DViewDirection::Front,Scene3DViewDirection::Back,
			Scene3DViewDirection::Right,Scene3DViewDirection::Left,Scene3DViewDirection::Top,Scene3DViewDirection::Bottom};
		const Ui3DGridPlane planes[]={Ui3DGridPlane::XY,Ui3DGridPlane::XY,Ui3DGridPlane::YZ,
			Ui3DGridPlane::YZ,Ui3DGridPlane::XZ,Ui3DGridPlane::XZ};
		for(int i=0;i<6;++i) {
			Check(window.first.SetViewDirection(directions[i],error) &&
			      window.first.GetCamera().GetViewDirection(0)==directions[i] &&
			      window.first.GetCamera().orthographic && window.first.GetOptions().grid_plane==planes[i],
			      "named direction applies exact pose, orthographic projection and matching grid plane");
			Check(Near(window.first.GetCamera().target,valid.target),"named direction preserves target");
		}
		const auto direction_camera=window.first.GetCamera();
		Check(window.first.SetViewDirection(Scene3DViewDirection::Bottom,error) &&
		      window.first.GetCamera().revision==direction_camera.revision,"unchanged named direction preserves revision");
		Check(!window.first.SetViewDirection(Scene3DViewDirection::User,error) &&
		      window.first.GetCamera().revision==direction_camera.revision &&
		      window.first.GetOptions().grid_plane==Ui3DGridPlane::XZ,"User direction is rejected without partial camera/grid changes");
		Check(window.first.SetOrthographic(false,error) && !window.first.GetCamera().orthographic &&
		      Near(window.first.GetCamera().distance,valid.distance),"control projection round trip preserves target-plane framing");
		Check(window.first.SetNavigationTool(Ui3DNavigationTool::Orbit,error) &&
		      window.first.GetNavigationTool()==Ui3DNavigationTool::Orbit,"left navigation tool can be selected programmatically");
		Check(!window.first.SetNavigationTool(static_cast<Ui3DNavigationTool>(999),error) &&
		      window.first.GetNavigationTool()==Ui3DNavigationTool::Orbit,"invalid tool preserves prior tool");
		window.first.SetNavigationTool(Ui3DNavigationTool::Select,error);
		window.first.SetOptions(Ui3DViewOptions::Dark(),error);
		window.first.SetCamera(valid,error);
		Check(events.begins + events.updates + events.commits + events.cancels == 0,
		      "programmatic configuration emits no user navigation events");

		window.Open();
		// This finite native-input fixture owns only its own window. Match the
		// demo qualifier's topmost policy without manipulating other applications.
		window.TopMost();
		// Establish U++'s focused child through its normal activation path before
		// requesting foreground; the later native gesture fixtures do this too.
		window.first.SetFocus();
		const bool foreground_ready = AcquireTestForeground(window.GetHWND(), "startup");
		Check(foreground_ready, "finite native test window becomes foreground");
		Pump(45);
		const int presses_before_guard = native_presses;
		Check(!AcquireTestForeground(GetDesktopWindow(), "unowned-window guard") &&
		      native_presses == presses_before_guard, "activation rejects an unowned window without input");
		if(foreground_ready) {
			// Exercise the actual click-activation route deterministically with a
			// second window belonging to this test. Its body cannot cover our
			// caption. No unrelated application's focus or settings are changed.
			TopWindow activation_peer;
			activation_peer.Title("Ui3DView activation fixture");
			Rect bounds = window.GetRect();
			activation_peer.SetRect(bounds.right - 190, bounds.bottom - 100, 170, 70);
			activation_peer.Open(&window);
			activation_peer.TopMost();
			const bool peer_ready = AcquireTestForeground(activation_peer.GetHWND(), "activation fixture");
			Check(peer_ready && GetForegroundWindow() != window.GetHWND(),
			      "activation fixture establishes a different owned foreground window");
			if(peer_ready)
				Check(AcquireTestForeground(window.GetHWND(), "caption fixture", true),
				      "owned caption click activates the test window before viewport input");
			activation_peer.Close(); Pump(2);
			window.first.SetFocus();
		}
		Check(window.first.IsGpuReady() && window.second.IsGpuReady(), "both Vulkan controls open");
		Check(window.first.GetViewError().IsEmpty(), "first content renders without error");
		Check(window.second.GetViewError().IsEmpty(), "second content renders without error");
		Check(window.first.GetViewStats().depth_active &&
		      window.first.GetViewStats().rendered_frames > 0, "first completes depth-tested frame");
		Check(window.second.GetViewStats().depth_active &&
		      window.second.GetViewStats().rendered_frames > 0, "second completes depth-tested frame");
		if(!window.first.GetViewError().IsEmpty()) Cout() << window.first.GetViewError() << EOL;
		if(!window.second.GetViewError().IsEmpty()) Cout() << window.second.GetViewError() << EOL;
		const uint64 mode_uploads=window.first.GetViewStats().mesh_uploads;
		for(Ui3DDisplayMode mode:{Ui3DDisplayMode::SolidEdges,Ui3DDisplayMode::Wireframe,Ui3DDisplayMode::Solid}) {
			const uint64 frames=window.first.GetViewStats().rendered_frames;
			Check(window.first.SetDisplayMode(mode,error),"live display mode accepted");
			Pump(5);
			Check(window.first.GetViewError().IsEmpty() && window.first.GetViewStats().rendered_frames>frames,
			      "solid, wireframe and solid-edge modes each complete a Vulkan frame");
		}
		Check(window.first.GetViewStats().mesh_uploads==mode_uploads,"display changes retain mesh uploads");
		Check(window.first.SetViewDirection(Scene3DViewDirection::Top,error),"live exact top view accepted");
		Pump(5);
		Check(window.first.GetViewError().IsEmpty(),"exact pole projection renders without error");
		Check(window.first.SetViewDirection(Scene3DViewDirection::Right,error),"live YZ view accepted");
		auto light_options=Ui3DViewOptions::Light(); light_options.grid_plane=Ui3DGridPlane::YZ;
		light_options.show_selection=false;
		Check(window.first.SetOptions(light_options,error),"light palette and selection-overlay switch accepted");
		Pump(5);
		Check(window.first.GetViewError().IsEmpty() && window.first.GetViewStats().mesh_uploads==mode_uploads,
		      "grid-plane and palette changes render without rebuilding scene meshes");
		window.first.SetCamera(valid,error); window.first.SetOptions(Ui3DViewOptions::Dark(),error); Pump(5);

		const auto idle = window.first.GetGpuStats().presented_frames;
		Pump(12);
		Check(window.first.GetGpuStats().presented_frames == idle, "idle viewport has no frame loop");
		const uint64 uploads = window.first.GetViewStats().mesh_uploads;
		const auto second_camera = window.second.GetCamera();
		camera = window.first.GetCamera();
		for(int i = 0; i < 3; ++i) {
			camera.Orbit(0.05, 0.01);
			Check(window.first.SetCamera(camera, error), "camera update accepted");
			Pump(4);
		}
		Check(window.first.GetViewStats().mesh_uploads == uploads, "camera motion reuses mesh uploads");
		Check(window.second.GetCamera().revision == second_camera.revision &&
		      window.second.GetCamera().yaw == second_camera.yaw, "camera state is per-view");

		camera = valid;
		Check(window.first.SetCamera(camera, error), "restore analytic picking camera");
		Pump();
		Point center(window.first.GetSize().cx / 2, window.first.GetSize().cy / 2);
		auto hit = window.first.Pick(Pointf(center));
		Check(hit.hit && hit.id == "test:box", "center ray hits actual mesh");
		Check(hit.scene_revision == scene->GetRevision() &&
		      hit.camera_revision == window.first.GetCamera().revision, "pick has exact revisions");

		Check(window.first.IsCurrentPick(hit), "new geometric result is current");
		Check(!window.second.IsCurrentPick(hit), "pick cannot cross control instances");
		window.first.SetScene(scene, error);
		Check(window.first.IsCurrentPick(hit), "same snapshot no-op preserves pick validity");
		auto colliding_revision = Scene(scene->GetRevision(), "test:box");
		Check(window.first.SetScene(colliding_revision, error), "different snapshot may reuse app revision");
		Check(!window.first.IsCurrentPick(hit), "binding identity rejects same revision and ID collision");
		window.first.SetScene(scene, error);
		hit = window.first.Pick(Pointf(center));
		const Rect old_rect = window.first.GetRect();
		window.first.SetRect(old_rect.left, old_rect.top, old_rect.Width() + 2, old_rect.Height());
		Check(!window.first.IsCurrentPick(hit), "projection resize invalidates old pick");
		window.first.SetRect(old_rect);
		auto miss = window.first.Pick(Pointf(-100000, -100000));
		Check(!miss.hit && window.first.IsCurrentPick(miss), "current miss is a valid selection request");

		// Target-only splat samples share the existing queued-pick provenance.
		{
			const auto saved_camera=window.first.GetCamera();
			Scene3DSplat point; point.covariance[0]=point.covariance[3]=point.covariance[5]=.01;
			Scene3DObjectDesc target; target.id="sample:target";
			target.splats=Scene3DSplats::Create({point},error);
			Scene3DSceneDesc sample_desc; sample_desc.revision=scene->GetRevision();
			sample_desc.objects={target};
			auto sample_scene=Scene3DSnapshot::Create(sample_desc,error);
			Check(sample_scene && window.first.SetScene(sample_scene,error),"surface wrapper fixture is installed");
			auto sample_camera=saved_camera;
			sample_camera.orthographic=true; sample_camera.ortho_height=2;
			sample_camera.SetLookAt({0,0,5},{0,0,0},{0,1,0},error);
			Check(window.first.SetCamera(sample_camera,error),"surface wrapper uses an analytic camera");
			const Pointf sample_centre(window.first.GetSize().cx*.5,window.first.GetSize().cy*.5);
			const auto surface=window.first.SampleSplatSurface(sample_centre,0,target.id);
			Check(surface.pick.hit && surface.pick.id==target.id &&
			      window.first.IsCurrentPick(surface.pick) && !window.second.IsCurrentPick(surface.pick),
			      "target surface result retains source-control and snapshot identity");
			const auto missing_surface=window.first.SampleSplatSurface(sample_centre,10,"sample:missing");
			Check(!missing_surface.pick.hit && window.first.IsCurrentPick(missing_surface.pick),
			      "current target-only surface miss retains valid provenance");
			sample_camera=window.first.GetCamera(); sample_camera.Orbit(.02,.01);
			Check(window.first.SetCamera(sample_camera,error) && !window.first.IsCurrentPick(surface.pick),
			      "camera changes invalidate queued surface samples");
			Check(window.first.SetScene(scene,error) && window.first.SetCamera(saved_camera,error) &&
			      !window.first.IsCurrentPick(surface.pick),"scene rebind invalidates queued surface samples");
		}

		int selections = 0;
		window.first.SetSelection({}, error);
		window.first.WhenSelectionRequest = [&](const Ui3DSelectionRequest& request) {
			++selections;
			Check(window.first.GetSelection().empty(), "click is a request before app selection");
			Check(request.pick.hit && request.pick.id == "test:box", "native click carries geometric hit");
			Check(window.first.SetSelection({request.pick.id}, error), "application accepts selection");
			window.first.WhenSelectionRequest.Clear(); // In-flight callback must remain valid.
		};
		// Reproduce the caption path's suppression state without relying on
		// Windows event timing. Only real client motion may clear it here.
		Ctrl::IgnoreMouseClick();
		Mouse(window.first, WM_LBUTTONDOWN, MK_LBUTTON, center);
		const bool selection_capture = window.first.HasCapture();
		Mouse(window.first, WM_LBUTTONUP, 0, center);
		if(selections != 1 || window.first.GetSelection().size() != 1) {
			String detail = Format("Native selection: raw_down=%d raw_up=%d down=%d up=%d double=%d down_point=(%d,%d) up_point=(%d,%d) down_flags=%u up_flags=%u capture_after_down=%d capture_after_up=%d requests=%d selected=%d navigating=%d focus=%d left_async=%d",
			                       window.raw_left_downs, window.raw_left_ups,
			                       window.first.left_downs, window.first.left_ups, window.first.left_doubles,
			                       window.first.last_left_down.x, window.first.last_left_down.y,
			                       window.first.last_left_up.x, window.first.last_left_up.y,
			                       int(window.first.left_down_flags), int(window.first.left_up_flags),
			                       int(selection_capture), int(window.first.HasCapture()), selections,
			                       int(window.first.GetSelection().size()), int(window.first.IsNavigating()),
			                       int(window.first.HasFocus()), int(GetAsyncKeyState(VK_LBUTTON) & 0x8000));
			Cout() << detail << EOL; failure_details << detail << "\n";
			ForegroundDiagnostic(window.GetHWND(), "selection delivery", false);
		}
		Check(selections == 1 && window.first.GetSelection().size() == 1,
		      "native child forwards selection input once");
		Check(window.first.left_downs == 1 && window.first.left_ups == 1 &&
		      window.first.left_doubles == 0 && selection_capture,
		      "native hover clears click suppression before one captured selection click");
		ViewCommandInputTests(window,scene,next_scene,valid,events);
		ToolPointerTests(window,scene,next_scene);

		const auto start = window.first.GetCamera();
		int cancels = events.cancels, commits = events.commits;
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		Check(window.first.IsNavigating() && window.first.HasCapture(), "native middle press begins captured orbit");
		Mouse(window.first, WM_MOUSEMOVE, MK_MBUTTON, center + Point(30, 12));
		Check(window.first.GetCamera().yaw != start.yaw, "native move changes camera");
		Check(window.first.Key(K_ESCAPE, 1), "Escape is consumed during navigation");
		Check(!window.first.IsNavigating() && !window.first.HasCapture(), "Escape releases capture");
		Check(window.first.GetCamera().yaw == start.yaw && window.first.GetCamera().pitch == start.pitch,
		      "cancel restores initial camera");
		Check(window.first.GetCamera().revision > start.revision, "cancel retains monotonic revision");
		Check(events.cancels == cancels + 1 && events.commits == commits, "cancel emitted exactly once");
		window.first.CancelMode();
		Check(events.cancels == cancels + 1, "repeated capture cancellation is idempotent");

		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		Mouse(window.first, WM_MOUSEMOVE, MK_MBUTTON, center + Point(20, 0));
		Mouse(window.first, WM_MBUTTONUP, 0, center + Point(20, 0));
		Check(events.commits == commits + 1 && events.cancels == cancels + 1,
		      "deliberate ReleaseCapture commits without spurious cancellation");
		Check(window.first.GetCamera().yaw != start.yaw, "commit retains new camera");

		cancels = events.cancels;
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		window.first.ReleaseCapture();
		Pump(2); // Capture-loss notification occurs after U++ finishes releasing.
		Check(!window.first.IsNavigating() && events.cancels == cancels + 1,
		      "external capture release cancels once");

		cancels = events.cancels;
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		window.second.SetFocus();
		Pump(2);
		Check(!window.first.IsNavigating() && events.cancels == cancels + 1, "focus loss cancels once");
		cancels = events.cancels;
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		window.first.Disable();
		Pump(2);
		Check(!window.first.IsNavigating() && events.cancels == cancels + 1, "disable cancels once");
		const auto disabled = window.first.GetCamera();
		window.first.MouseWheel(center, 120, 0);
		Check(window.first.GetCamera().revision == disabled.revision, "disabled wheel cannot navigate");
		Check(!window.first.Key('F', 1), "disabled keyboard action is ignored");
		window.first.Enable();

		cancels = events.cancels;
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		window.first.Hide();
		Pump(2);
		Check(!window.first.IsNavigating() && events.cancels == cancels + 1, "hide cancels navigation");
		Pump(5);
		Check(window.second.IsGpuReady() && window.second.GetViewError().IsEmpty(),
		      "hidden sibling does not disturb another presenter");
		window.first.Show(); Pump();
		Check(window.first.GetViewError().IsEmpty(), "show resumes retained content");

		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		window.first.WhenNavigation = [&](const Ui3DNavigationEvent& event) {
			if(event.phase == Ui3DNavigationPhase::Cancel) {
				auto preferred = window.first.GetCamera(); preferred.distance = 9;
				Check(window.first.SetCamera(preferred, error), "callback camera replacement succeeds");
			}
		};
		auto superseded = window.first.GetCamera(); superseded.distance = 11;
		Check(!window.first.SetCamera(superseded, error), "outer camera setter detects callback rebind");
		Check(window.first.GetCamera().distance == 9, "callback camera is not silently overwritten");
		window.first.WhenNavigation = [&](const Ui3DNavigationEvent& e) { events.On(e); };

		uint64 begin_scene = 0, cancel_scene = 0, begin_id = 0, cancel_id = 0;
		window.first.WhenNavigation = [&](const Ui3DNavigationEvent& e) {
			events.On(e);
			if(e.phase == Ui3DNavigationPhase::Begin) { begin_scene = e.scene_revision; begin_id = e.gesture_id; }
			if(e.phase == Ui3DNavigationPhase::Cancel) { cancel_scene = e.scene_revision; cancel_id = e.gesture_id; }
		};
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		Check(window.first.SetScene(next_scene, error), "scene rebound during navigation");
		Check(begin_id == cancel_id && begin_scene == scene->GetRevision() &&
		      cancel_scene == begin_scene, "rebind cancellation retains originating gesture and scene");
		Check(!window.first.IsNavigating() && window.first.GetScene() == next_scene,
		      "scene rebind cancels drag and retains new snapshot");
		window.first.SetScene(scene, error);
		window.first.SetSelection({}, error);

		std::vector<Ui3DNavigationPhase> ordered;
		window.first.WhenNavigation = [&](const Ui3DNavigationEvent& e) { ordered.push_back(e.phase); };
		Mouse(window.first, WM_MBUTTONDOWN, MK_MBUTTON, center);
		ordered.clear();
		window.first.ReleaseCapture(); // Queue A, then perform B without pumping.
		window.first.MouseWheel(center, 120, 0);
		Check(ordered.size() == 2 && ordered[0] == Ui3DNavigationPhase::Cancel &&
		      ordered[1] == Ui3DNavigationPhase::Commit, "deferred cancel precedes a newer wheel action");
		window.first.WhenNavigation = [&](const Ui3DNavigationEvent& e) { events.On(e); };

		ReleaseButtons();
		window.second.SetFocus();
		window.second.MiddleDown(Point(100, 100), 0);
		Check(window.second.IsNavigating(), "second view begins reentrant focus fixture");
		window.second.WhenNavigation = [&](const Ui3DNavigationEvent& e) {
			if(e.phase == Ui3DNavigationPhase::Cancel) window.first.Disable();
		};
		window.first.MiddleDown(center, 0);
		Check(!window.first.IsEnabled() && !window.first.IsNavigating(),
		      "focus-loss callback can disable target before its new gesture starts");
		window.first.Enable();
		window.second.WhenNavigation.Clear();
		ReleaseButtons();

		// This tests a copied notification and no member access after destruction.
		One<TestView> transient;
		transient.Create();
		transient->SetScene(scene, error);
		window.Add(transient->LeftPos(10, 140).TopPos(10, 140));
		transient->SetCamera(valid, error);
		Pump(12);
		Mouse(*transient, WM_MBUTTONDOWN, MK_MBUTTON, Point(70, 70));
		Check(transient && transient->IsNavigating() && transient->HasCapture(),
		      "destruction fixture begins a native captured gesture before arming its callback");
		int destroying_updates = 0;
		if(transient) {
			// Mouse() pumps Windows input while establishing the press. Arm only
			// afterwards: an incidental queued move during that pump is allowed
			// to notify Update and must not null the One before our next dereference.
			transient->WhenNavigation = [&](const Ui3DNavigationEvent& event) {
				if(event.phase == Ui3DNavigationPhase::Update) { ++destroying_updates; transient.Clear(); }
			};
			Mouse(*transient, WM_MOUSEMOVE, MK_MBUTTON, Point(82, 70));
		}
		Check(destroying_updates == 1, "one native motion Update destroys the fixture");
		Check(!transient, "navigation callback can destroy native control safely");
		transient.Clear();

		int destroyed_pending_callbacks = 0;
		transient.Create();
		transient->SetScene(scene, error);
		window.Add(transient->LeftPos(10, 140).TopPos(10, 140));
		Pump(10);
		transient->WhenNavigation = [&](const Ui3DNavigationEvent& e) {
			if(e.phase == Ui3DNavigationPhase::Cancel) ++destroyed_pending_callbacks;
		};
		transient->MiddleDown(Point(70, 70), 0);
		transient->ReleaseCapture();
		transient.Clear();
		Pump(4);
		Check(destroyed_pending_callbacks == 0, "destruction suppresses pending callback safely");

		window.SetRect(60, 60, 940, 480); Pump(12);
		Check(window.first.GetViewStats().target_size == window.first.GetSize(),
		      "depth target follows native resize");
		Check(window.first.GetViewError().IsEmpty() && window.second.GetViewError().IsEmpty(),
		      "both views remain error-free after resize");
		window.first.SetRect(0, 0, 0, 0); Pump(4);
		Check(window.second.IsGpuReady(), "zero-sized first view preserves sibling");
		window.Layout(); Pump(10);
		Check(window.first.GetViewError().IsEmpty(), "zero-to-positive size recovers");

		const auto before_close = window.first.GetViewStats().mesh_uploads;
		window.Close(); Pump(6);
		window.Open(); Pump(25);
		Check(window.first.IsGpuReady() && window.first.GetViewError().IsEmpty(), "closed view reopens");
		Check(window.first.GetScene() == scene && window.first.GetViewStats().mesh_uploads > before_close,
		      "reopen reuploads retained scene to fresh device resources");
		window.Close(); Pump(5);
	}
	ReleaseButtons();
	SetCursorPos(original_cursor.x, original_cursor.y);
	Check(native_presses > 0 && native_presses == native_releases &&
	      !test_left_held && !test_middle_held, "all successfully injected presses have matching releases");
	Check(caption_activations > 0, "native caption activation path actually exercised");
	const auto final = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Check(final.runtime_live_count == 0 && final.instance_live_count == 0 &&
	      final.debug_messenger_live_count == 0 && final.surface_live_count == 0 &&
	      final.device_live_count == 0 && final.swapchain_live_count == 0,
	      "all native Vulkan ownership released");
	Check(checks >= 60, "nontrivial test count executed");
	String input_report = Format("NativeInput presses=%d releases=%d cleanup_releases=%d foreground_requests=%d caption_activations=%d\n",
	                             native_presses, native_releases, cleanup_releases,
	                             foreground_requests, caption_activations);
	Cout() << input_report;
	String report = Format("Ui3DViewTest %s checks=%d failures=%d\n",
	                       failures ? "FAIL" : "PASS", checks, failures);
	Cout() << report;
	SaveFile(GetExeDirFile("Ui3DViewTest-results.txt"), report + input_report + failure_details);
	if(failures) SetExitCode(1);
}
