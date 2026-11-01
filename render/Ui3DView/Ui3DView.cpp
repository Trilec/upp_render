#include "Ui3DViewContent.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Upp {

Ui3DViewOptions Ui3DViewOptions::Dark() { return Ui3DViewOptions(); }
Ui3DViewOptions Ui3DViewOptions::Light()
{
	Ui3DViewOptions o;
	o.background = Rgba8(224, 230, 235, 255);
	o.grid_color = Rgba8(161, 174, 185, 255);
	o.selection_color = Rgba8(210, 114, 20, 255);
	return o;
}

Ui3DView::Ui3DView() : state(std::make_shared<Ui3DViewState>())
{
	WantFocus();
	SetGpuContent(CreateUi3DViewContent(state));
	SetContinuousRefresh(0);
}

Ui3DView::~Ui3DView()
{
	destroying = true;
	navigation_watch.Kill();
	navigation_notice.Kill();
	pending_navigation_notice.Clear();
	navigation = Ui3DNavigationMode::None;
	click_pending = false;
	if(HasCapture()) ReleaseCapture();
	// GpuCtrl closes its content before its device; content retains state.
}

bool Ui3DView::SetScene(std::shared_ptr<const Scene3DSnapshot> scene, String& error)
{
	error.Clear();
	if(state->scene == scene) return true;
	if(scene_binding_revision == std::numeric_limits<uint64>::max()) {
		error = "scene binding revision exhausted"; return false;
	}
	++scene_binding_revision;
	++binding_generation;
	state->scene = std::move(scene);
	state->error.Clear();
	// Selection is app-owned. Keep requested IDs; missing IDs simply draw no
	// highlight until the application publishes its next selection projection.
	RequestGpuRefresh();
	CancelNavigation("scene rebound");
	return true; // No member access after a possibly destructive callback.
}

std::shared_ptr<const Scene3DSnapshot> Ui3DView::GetScene() const { return state->scene; }

bool Ui3DView::SetCamera(const Scene3DCamera& camera, String& error)
{
	if(!camera.Validate(error)) return false;
	Ptr<Ui3DView> alive(this);
	const uint64 binding = binding_generation;
	CancelNavigation("camera replaced");
	if(!alive) { error = "view destroyed by navigation callback"; return false; }
	if(binding_generation != binding) {
		error = "camera replacement superseded by a navigation callback"; return false;
	}
	Scene3DCamera next = camera;
	if(state->camera.revision == std::numeric_limits<uint64>::max()) {
		error = "camera revision exhausted"; return false;
	}
	next.revision = max(next.revision, state->camera.revision + 1);
	++binding_generation;
	state->camera = next;
	state->error.Clear();
	error.Clear();
	RequestGpuRefresh();
	return true;
}

Scene3DCamera Ui3DView::GetCamera() const { return state->camera; }

bool Ui3DView::SetOptions(const Ui3DViewOptions& options, String& error)
{
	if(options.background.a != 255 || options.grid_color.a != 255 ||
	   options.selection_color.a != 255) {
		error = "Ui3DView currently supports opaque decoration colors only"; return false;
	}
	if(options.grid_extent < 1 || options.grid_extent > 100 ||
	   !std::isfinite(options.grid_spacing) ||
	   options.grid_spacing < 0.001 || options.grid_spacing > 10000) {
		error = "grid extent must be 1..100; spacing must be finite 0.001..10000 metres";
		return false;
	}
	const auto& old = state->options;
	if(old.background == options.background && old.grid_color == options.grid_color &&
	   old.selection_color == options.selection_color && old.show_grid == options.show_grid &&
	   old.show_axes == options.show_axes && old.wireframe == options.wireframe &&
	   old.lighting == options.lighting && old.grid_extent == options.grid_extent &&
	   old.grid_spacing == options.grid_spacing) { error.Clear(); return true; }
	if(old.grid_color != options.grid_color || old.grid_extent != options.grid_extent ||
	   old.grid_spacing != options.grid_spacing)
		++state->decoration_revision;
	state->options = options;
	state->error.Clear();
	error.Clear();
	RequestGpuRefresh();
	return true;
}

Ui3DViewOptions Ui3DView::GetOptions() const { return state->options; }

bool Ui3DView::SetSelection(const std::vector<String>& ids, String& error)
{
	if(ids.size() > 100000) { error = "selection exceeds 100000 IDs"; return false; }
	Index<String> unique;
	for(const String& id : ids) {
		if(!state->scene || !state->scene->FindObject(id)) {
			error = "selection ID is not in the bound scene: " + id; return false;
		}
		if(unique.Find(id) >= 0) { error = "duplicate selection ID: " + id; return false; }
		unique.Add(id);
	}
	if(state->selection == ids) { error.Clear(); return true; }
	state->selection = ids;
	++state->selection_revision;
	error.Clear();
	RequestGpuRefresh();
	return true;
}

std::vector<String> Ui3DView::GetSelection() const { return state->selection; }

bool Ui3DView::FitAll()
{
	if(!state->scene) return false;
	Scene3DCamera camera = state->camera;
	if(!camera.Fit(state->scene->GetBounds(), GetSize())) return false;
	String error;
	return SetCamera(camera, error);
}

bool Ui3DView::FitSelection()
{
	if(!state->scene) return false;
	Scene3DBounds bounds;
	for(const String& id : state->selection) {
		const Scene3DObject *object = state->scene->FindObject(id);
		if(object && object->visible) bounds.Add(object->bounds);
	}
	if(bounds.empty) return FitAll();
	Scene3DCamera camera = state->camera;
	if(!camera.Fit(bounds, GetSize())) return false;
	String error;
	return SetCamera(camera, error);
}

Ui3DViewPick Ui3DView::Pick(Pointf point) const
{
	Ui3DViewPick result;
	result.source_view = state;
	result.scene_binding_revision = scene_binding_revision;
	result.viewport_size = GetSize();
	result.camera_revision = state->camera.revision;
	result.scene_revision = state->scene ? state->scene->GetRevision() : 0;
	Scene3DRay ray;
	if(state->scene && state->camera.WorldRay(point, result.viewport_size, ray))
		static_cast<Scene3DPick&>(result) = state->scene->Pick(ray, state->camera.revision);
	return result;
}

bool Ui3DView::IsCurrentPick(const Ui3DViewPick& pick) const
{
	return pick.source_view.lock().get() == state.get() &&
	       pick.scene_binding_revision == scene_binding_revision &&
	       pick.scene_revision == (state->scene ? state->scene->GetRevision() : 0) &&
	       pick.camera_revision == state->camera.revision &&
	       pick.viewport_size == GetSize();
}

Ui3DViewStats Ui3DView::GetViewStats() const { return state->stats; }
String Ui3DView::GetViewError() const
{
	return state->error.IsEmpty() ? GetGpuError() : state->error;
}

void Ui3DView::EmitNavigation(Ui3DNavigationPhase phase, Ui3DNavigationMode mode,
                             uint64 start_revision, const String& reason)
{
	Ui3DNavigationEvent event;
	event.phase = phase;
	event.mode = mode;
	event.gesture_id = gesture_id;
	event.scene_revision = gesture_scene_revision;
	event.scene_binding_revision = gesture_scene_binding_revision;
	event.starting_camera_revision = start_revision;
	event.camera = state->camera;
	event.reason = reason;
	auto callback = WhenNavigation;
	if(callback) callback(event);
}

void Ui3DView::FlushNavigationNotice()
{
	navigation_notice.Kill();
	auto callback = pending_navigation_notice;
	pending_navigation_notice.Clear();
	if(callback) callback();
}

void Ui3DView::BeginNavigation(Ui3DNavigationMode mode, int button, Point p)
{
	Ptr<Ui3DView> alive(this);
	FlushNavigationNotice();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen() || IsNavigating()) return;
	const uint64 binding = binding_generation, previous_gesture = gesture_id;
	SetFocus();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen() || IsNavigating() ||
	   binding != binding_generation || previous_gesture != gesture_id) return;
	click_pending = false;
	if(!SetCapture() || !alive) return;
	if(!IsEnabled() || !IsVisible() || !IsOpen() || IsNavigating() ||
	   binding != binding_generation || previous_gesture != gesture_id) {
		if(!IsNavigating() && HasCapture()) ReleaseCapture();
		return;
	}
	navigation_start = state->camera;
	gesture_scene_revision = state->scene ? state->scene->GetRevision() : 0;
	gesture_scene_binding_revision = scene_binding_revision;
	navigation = mode;
	navigation_button = button;
	press_point = p;
	++gesture_id;
	// This clock exists only during a drag, so renderer failure can cancel even
	// when the pointer stops. It never draws or drives animation.
	navigation_watch.Set(-50, [this] {
		if(IsNavigating() && !GetViewError().IsEmpty()) CancelNavigation("renderer failure");
	});
	EmitNavigation(Ui3DNavigationPhase::Begin, mode, navigation_start.revision, "");
}

void Ui3DView::FinishNavigation(bool commit, const String& reason)
{
	if(!IsNavigating()) return;
	const Ui3DNavigationMode mode = navigation;
	const uint64 start_revision = navigation_start.revision;
	if(!commit) {
		uint64 revision = state->camera.revision;
		state->camera = navigation_start;
		state->camera.revision = revision == std::numeric_limits<uint64>::max()
		                       ? revision : revision + 1;
	}
	// ReleaseCapture synchronously calls CancelMode in U++. Set idle first.
	navigation_watch.Kill();
	navigation = Ui3DNavigationMode::None;
	navigation_button = 0;
	click_pending = false;
	if(HasCapture()) ReleaseCapture();
	RequestGpuRefresh();
	if(!destroying)
		EmitNavigation(commit ? Ui3DNavigationPhase::Commit : Ui3DNavigationPhase::Cancel,
		               mode, start_revision, reason);
}

void Ui3DView::CancelNavigation(const String& reason)
{
	if(IsNavigating()) { FinishNavigation(false, reason); return; }
	click_pending = false;
	if(HasCapture()) ReleaseCapture();
}

void Ui3DView::LeftDown(Point p, dword flags)
{
	Ptr<Ui3DView> alive(this);
	FlushNavigationNotice();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen()) return;
	if(flags & K_ALT) { BeginNavigation(Ui3DNavigationMode::Orbit, 1, p); return; }
	if(IsNavigating()) return;
	const uint64 binding = binding_generation, previous_gesture = gesture_id;
	SetFocus();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen() || IsNavigating() ||
	   binding != binding_generation || previous_gesture != gesture_id) return;
	if(!SetCapture() || !alive) return;
	if(!IsEnabled() || !IsVisible() || !IsOpen() || IsNavigating() ||
	   binding != binding_generation || previous_gesture != gesture_id) {
		if(!IsNavigating() && HasCapture()) ReleaseCapture();
		return;
	}
	press_point = p;
	click_pending = true;
}

void Ui3DView::LeftUp(Point p, dword flags)
{
	if(navigation_button == 1) { FinishNavigation(true, "released"); return; }
	if(!click_pending) return;
	const bool click = abs(p.x - press_point.x) <= DPI(4) &&
	                   abs(p.y - press_point.y) <= DPI(4);
	click_pending = false;
	if(HasCapture()) ReleaseCapture();
	if(click && IsEnabled()) SelectFromInput(p, flags);
}

void Ui3DView::MiddleDown(Point p, dword flags)
{
	BeginNavigation((flags & (K_SHIFT | K_ALT)) ? Ui3DNavigationMode::Pan
	                                          : Ui3DNavigationMode::Orbit, 2, p);
}
void Ui3DView::MiddleUp(Point, dword)
{
	if(navigation_button == 2) FinishNavigation(true, "released");
}
void Ui3DView::RightDown(Point p, dword flags)
{
	if(flags & K_ALT) BeginNavigation(Ui3DNavigationMode::Dolly, 3, p);
}
void Ui3DView::RightUp(Point, dword)
{
	if(navigation_button == 3) FinishNavigation(true, "released");
}

void Ui3DView::MouseMove(Point p, dword)
{
	if(!IsNavigating()) return;
	if(!GetViewError().IsEmpty()) { CancelNavigation("renderer failure"); return; }
	if(!HasCapture()) { CancelNavigation("capture lost"); return; }
	Scene3DCamera next = navigation_start;
	const double dx = p.x - press_point.x, dy = p.y - press_point.y;
	bool changed = false;
	switch(navigation) {
	case Ui3DNavigationMode::Orbit: changed = next.Orbit(-dx * 0.007, dy * 0.007); break;
	case Ui3DNavigationMode::Pan: changed = next.Pan(dx, dy, GetSize()); break;
	case Ui3DNavigationMode::Dolly: changed = next.Dolly(-dy * 0.015); break;
	default: break;
	}
	if(!changed || state->camera.revision == std::numeric_limits<uint64>::max()) return;
	const auto& current = state->camera;
	if(next.yaw == current.yaw && next.pitch == current.pitch &&
	   next.distance == current.distance && next.ortho_height == current.ortho_height &&
	   next.target.x == current.target.x && next.target.y == current.target.y &&
	   next.target.z == current.target.z) return;
	next.revision = state->camera.revision + 1;
	state->camera = next;
	RequestGpuRefresh();
	EmitNavigation(Ui3DNavigationPhase::Update, navigation, navigation_start.revision, "");
}

void Ui3DView::MouseWheel(Point, int zdelta, dword)
{
	Ptr<Ui3DView> alive(this);
	FlushNavigationNotice();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen() || IsNavigating() || !zdelta) return;
	Scene3DCamera next = state->camera;
	if(!next.Dolly(zdelta / 120.0 * 0.16) || next.revision == state->camera.revision) return;
	const uint64 start = state->camera.revision;
	state->camera = next;
	++gesture_id;
	gesture_scene_revision = state->scene ? state->scene->GetRevision() : 0;
	gesture_scene_binding_revision = scene_binding_revision;
	RequestGpuRefresh();
	EmitNavigation(Ui3DNavigationPhase::Commit, Ui3DNavigationMode::Dolly, start, "wheel");
}

void Ui3DView::FitFromInput(bool selection)
{
	Ptr<Ui3DView> alive(this);
	FlushNavigationNotice();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen()) return;
	const uint64 start = state->camera.revision;
	bool fitted = selection ? FitSelection() : FitAll();
	if(!alive || !fitted) return;
	++gesture_id;
	gesture_scene_revision = state->scene ? state->scene->GetRevision() : 0;
	gesture_scene_binding_revision = scene_binding_revision;
	EmitNavigation(Ui3DNavigationPhase::Commit, Ui3DNavigationMode::Dolly, start, "fit");
}

void Ui3DView::SelectFromInput(Point p, dword flags)
{
	Ptr<Ui3DView> alive(this);
	FlushNavigationNotice();
	if(!alive || !IsEnabled() || !IsVisible() || !IsOpen()) return;
	Ui3DSelectionRequest request;
	request.pick = Pick(Pointf(p));
	request.extend = (flags & K_SHIFT) != 0;
	request.toggle = (flags & K_CTRL) != 0;
	auto callback = WhenSelectionRequest;
	if(callback) callback(request);
}

bool Ui3DView::Key(dword key, int)
{
	if(!IsEnabled()) return false;
	if(key == K_ESCAPE && (IsNavigating() || click_pending)) {
		CancelNavigation("escape"); return true;
	}
	if(key == 'F' || key == 'f') { FitFromInput(true); return true; }
	if(key == K_HOME) { FitFromInput(false); return true; }
	return false;
}

void Ui3DView::CancelMode()
{
	// U++ ReleaseCtrlCapture still dereferences its captured control after this
	// returns. Do not release recursively or call destructive application code.
	click_pending = false;
	if(destroying || !IsNavigating()) return;
	Ui3DNavigationEvent event;
	event.phase = Ui3DNavigationPhase::Cancel;
	event.mode = navigation;
	event.gesture_id = gesture_id;
	event.scene_revision = gesture_scene_revision;
	event.scene_binding_revision = gesture_scene_binding_revision;
	event.starting_camera_revision = navigation_start.revision;
	const uint64 revision = state->camera.revision;
	state->camera = navigation_start;
	state->camera.revision = revision == std::numeric_limits<uint64>::max()
	                       ? revision : revision + 1;
	event.camera = state->camera;
	event.reason = "capture lost";
	navigation = Ui3DNavigationMode::None;
	navigation_button = 0;
	navigation_watch.Kill();
	RequestGpuRefresh();
	auto callback = WhenNavigation;
	pending_navigation_notice = [callback, event] { if(callback) callback(event); };
	navigation_notice.Set(0, [this] { FlushNavigationNotice(); });
}
void Ui3DView::LostFocus()
{
	CancelNavigation("focus lost");
}
void Ui3DView::State(int reason)
{
	Ptr<Ui3DView> alive(this);
	if(reason == CLOSE || (reason == SHOW && !IsVisible()) ||
	   (reason == ENABLE && !IsEnabled()))
		CancelNavigation(reason == CLOSE ? "closed" : "hidden or disabled");
	if(alive) GpuCtrl::State(reason);
}

}
