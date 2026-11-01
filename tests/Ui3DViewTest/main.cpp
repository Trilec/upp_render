#include <Ui3DView/Ui3DView.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <windows.h>
#include <cmath>
#include <limits>

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
void Pump(int count = 10)
{
	for(int i = 0; i < count; ++i) { Ctrl::ProcessEvents(); Ctrl::GuiSleep(10); }
	Ctrl::ProcessEvents(); // Dispatch anything that arrived during the final sleep.
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
	auto *native = dynamic_cast<DHCtrl *>(view.GetFirstChild());
	return native ? native->GetHWND() : nullptr;
}
class TestView : public Ui3DView {
public:
	int mouse_moves = 0;
	Point last_mouse = Point(0, 0);
	void MouseMove(Point p, dword flags) override {
		++mouse_moves;
		last_mouse = p;
		Ui3DView::MouseMove(p, flags); // A callback may delete this; access nothing after.
	}
};
bool test_middle_held = false, test_left_held = false;
void ReleaseButtons()
{
	INPUT input[2] {};
	input[0].type = input[1].type = INPUT_MOUSE;
	input[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
	input[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
	SendInput(2, input, sizeof(INPUT));
	test_middle_held = test_left_held = false;
	Pump(2);
}
void Mouse(TestView& view, UINT message, WPARAM, Point p)
{
	Ptr<TestView> alive(&view);
	HWND host = Host(view);
	Check(host && IsWindow(host), "real native input host exists");
	if(!host) return;
	if((message == WM_MBUTTONDOWN && test_middle_held) ||
	   (message == WM_LBUTTONDOWN && test_left_held)) ReleaseButtons();
	RECT rect;
	if(!GetWindowRect(host, &rect)) { Check(false, "native bounds available"); return; }
	const HWND root = GetAncestor(host, GA_ROOT);
	if(message == WM_MBUTTONDOWN || message == WM_LBUTTONDOWN) {
		SetForegroundWindow(root);
		// Activation across input queues can complete asynchronously. Observe it
		// before injecting; never bypass a denied foreground request.
		for(int i = 0; i < 50 && alive && GetForegroundWindow() != root; ++i) Pump(1);
	}
	if(!alive) { Check(false, "test control survived activation"); return; }
	if(GetForegroundWindow() != root) {
		Check(false, "test window foreground required before mouse input"); return;
	}
	const POINT position = { rect.left + p.x, rect.top + p.y };
	if(GetAncestor(WindowFromPoint(position), GA_ROOT) != root) {
		Check(false, "mouse injection target remains the test window"); return;
	}
	if(message == WM_MOUSEMOVE) {
		const int desktop_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
		const int desktop_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
		const int desktop_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		const int desktop_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		if(desktop_width <= 1 || desktop_height <= 1) {
			Check(false, "virtual desktop dimensions available"); return;
		}
		const int moves = view.mouse_moves;
		INPUT motion {};
		motion.type = INPUT_MOUSE;
		motion.mi.dx = (LONG)(((int64)(position.x - desktop_x) * 65536 + 32768) / desktop_width);
		motion.mi.dy = (LONG)(((int64)(position.y - desktop_y) * 65536 + 32768) / desktop_height);
		motion.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE |
		                    MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE_NOCOALESCE;
		Check(SendInput(1, &motion, sizeof(motion)) == 1, "native motion injected");
		// Observe actual delivery, including callback-driven destruction. Do not
		// assume that a fixed sleep means Windows has dispatched the input.
		for(int i = 0; i < 50 && alive && (view.mouse_moves == moves ||
		    abs(view.last_mouse.x - p.x) > 1 || abs(view.last_mouse.y - p.y) > 1); ++i) Pump(1);
		Check(!alive || view.mouse_moves > moves, "native motion reached the control");
		if(alive)
			Check(abs(view.last_mouse.x - p.x) <= 1 && abs(view.last_mouse.y - p.y) <= 1,
			      "native motion carries the requested control coordinates");
		return;
	}
	if(!SetCursorPos(position.x, position.y)) {
		Check(false, "cursor positioned inside test viewport"); return;
	}
	Pump(2);
	if(!alive || GetForegroundWindow() != root ||
	   GetAncestor(WindowFromPoint(position), GA_ROOT) != root) {
		Check(false, "mouse injection target remains the test window"); return;
	}
	INPUT input {};
	input.type = INPUT_MOUSE;
	switch(message) {
	case WM_LBUTTONDOWN: input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; test_left_held = true; break;
	case WM_LBUTTONUP: input.mi.dwFlags = MOUSEEVENTF_LEFTUP; test_left_held = false; break;
	case WM_MBUTTONDOWN: input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; test_middle_held = true; break;
	case WM_MBUTTONUP: input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP; test_middle_held = false; break;
	default: return;
	}
	Check(SendInput(1, &input, sizeof(input)) == 1, "native input injected");
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
}

GUI_APP_MAIN
{
	POINT original_cursor; GetCursorPos(&original_cursor);
	String error;
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
		Check(events.begins + events.updates + events.commits + events.cancels == 0,
		      "programmatic configuration emits no user navigation events");

		window.Open();
		Pump(45);
		Check(window.first.IsGpuReady() && window.second.IsGpuReady(), "both Vulkan controls open");
		Check(window.first.GetViewError().IsEmpty(), "first content renders without error");
		Check(window.second.GetViewError().IsEmpty(), "second content renders without error");
		Check(window.first.GetViewStats().depth_active &&
		      window.first.GetViewStats().rendered_frames > 0, "first completes depth-tested frame");
		Check(window.second.GetViewStats().depth_active &&
		      window.second.GetViewStats().rendered_frames > 0, "second completes depth-tested frame");
		if(!window.first.GetViewError().IsEmpty()) Cout() << window.first.GetViewError() << EOL;
		if(!window.second.GetViewError().IsEmpty()) Cout() << window.second.GetViewError() << EOL;

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

		int selections = 0;
		window.first.SetSelection({}, error);
		window.first.WhenSelectionRequest = [&](const Ui3DSelectionRequest& request) {
			++selections;
			Check(window.first.GetSelection().empty(), "click is a request before app selection");
			Check(request.pick.hit && request.pick.id == "test:box", "native click carries geometric hit");
			Check(window.first.SetSelection({request.pick.id}, error), "application accepts selection");
			window.first.WhenSelectionRequest.Clear(); // In-flight callback must remain valid.
		};
		Mouse(window.first, WM_LBUTTONDOWN, MK_LBUTTON, center);
		Mouse(window.first, WM_LBUTTONUP, 0, center);
		Check(selections == 1 && window.first.GetSelection().size() == 1,
		      "native child forwards selection input once");

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
		transient->WhenNavigation = [&](const Ui3DNavigationEvent& event) {
			if(event.phase == Ui3DNavigationPhase::Update) transient.Clear();
		};
		Mouse(*transient, WM_MBUTTONDOWN, MK_MBUTTON, Point(70, 70));
		Mouse(*transient, WM_MOUSEMOVE, MK_MBUTTON, Point(82, 70));
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
	const auto final = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Check(final.runtime_live_count == 0 && final.instance_live_count == 0 &&
	      final.debug_messenger_live_count == 0 && final.surface_live_count == 0 &&
	      final.device_live_count == 0 && final.swapchain_live_count == 0,
	      "all native Vulkan ownership released");
	Check(checks >= 60, "nontrivial test count executed");
	String report = Format("Ui3DViewTest %s checks=%d failures=%d\n",
	                       failures ? "FAIL" : "PASS", checks, failures);
	Cout() << report;
	SaveFile(GetExeDirFile("Ui3DViewTest-results.txt"), report + failure_details);
	if(failures) SetExitCode(1);
}
