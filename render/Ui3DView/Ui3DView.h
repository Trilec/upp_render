// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#ifndef _Ui3DView_Ui3DView_h_
#define _Ui3DView_Ui3DView_h_

#include <GpuRender/GpuCtrl.h>
#include <RenderCore/RenderCore.h>
#include <RenderScene/Scene3D.h>
#include <memory>
#include <vector>

namespace Upp {

enum class Ui3DNavigationPhase { Begin, Update, Commit, Cancel };
enum class Ui3DNavigationMode { None, Orbit, Pan, Dolly };

struct Ui3DViewState;

// Value result bound to its originating control, scene binding and projection.
// A current miss is valid too; use IsCurrentPick before accepting queued requests.
struct Ui3DViewPick : Scene3DPick {
	uint64 scene_binding_revision = 0;
	Size viewport_size = Size(0, 0);
private:
	friend class Ui3DView;
	std::weak_ptr<const Ui3DViewState> source_view;
};

struct Ui3DNavigationEvent {
	Ui3DNavigationPhase phase = Ui3DNavigationPhase::Begin;
	Ui3DNavigationMode mode = Ui3DNavigationMode::None;
	uint64 gesture_id = 0;
	uint64 scene_revision = 0;
	uint64 scene_binding_revision = 0;
	uint64 starting_camera_revision = 0;
	Scene3DCamera camera;
	String reason;
};

struct Ui3DSelectionRequest {
	Ui3DViewPick pick;
	bool extend = false;
	bool toggle = false;
};

// View decoration is independent of authored linear scene/material colors.
// These palette bytes are sRGB. Explicit Light/Dark snapshots do not alter a
// global Ui theme. Hosts can map their own theme roles into the same options.
struct Ui3DViewOptions {
	Rgba8 background = Rgba8(34, 39, 47, 255);
	Rgba8 grid_color = Rgba8(79, 87, 101, 255);
	Rgba8 selection_color = Rgba8(255, 182, 67, 255);
	bool show_grid = true;
	bool show_axes = true;
	bool wireframe = false;
	bool lighting = true;
	int grid_extent = 10;     // 1..100 cells on either side of the origin.
	double grid_spacing = 1; // Metres, finite 0.001..10000.
	static Ui3DViewOptions Light();
	static Ui3DViewOptions Dark();
};

struct Ui3DViewStats {
	uint64 rendered_frames = 0;
	uint64 mesh_uploads = 0;
	uint64 uploaded_bytes = 0;
	uint64 depth_rebuilds = 0;
	int resident_meshes = 0;
	int draws_last_frame = 0;
	Size target_size = Size(0, 0);
	bool depth_active = false;
};

// Reusable native Vulkan viewport. All methods/events run on the GUI thread.
// Immutable scene snapshots and their mesh resources are retained by shared
// ownership; the application owns document, selection, undo and playback time.
// The control has no idle animation timer. RequestGpuRefresh() follows changes.
//
// Example:
//   Ui3DView view;
//   Add(view.SizePos());
//   String error;
//   auto candidate = Scene3DSnapshot::Create(description, error);
//   if(candidate) { view.SetScene(candidate, error); view.FitAll(); }
//   // A failed factory must not be passed as null: null explicitly clears.
//   view.WhenSelectionRequest = [&](const Ui3DSelectionRequest& request) {
//       if(!view.IsCurrentPick(request.pick)) return;
//       view.SetSelection(request.pick.hit
//           ? std::vector<String>{request.pick.id} : std::vector<String>{}, error);
//   };
//
// Setters are silent; navigation emits Begin/Update/Commit/Cancel, and clicks
// issue requests only. An event may destroy/rebind this control. Escape and
// capture/focus loss restore the drag's starting camera. Destruction clears
// capture without notifying application code from a partially destroyed Ctrl.
class Ui3DView : public GpuCtrl {
public:
	typedef Ui3DView CLASSNAME;
	Ui3DView();
	~Ui3DView() override;

	Function<void(const Ui3DNavigationEvent&)> WhenNavigation;
	Function<void(const Ui3DSelectionRequest&)> WhenSelectionRequest;

	// Null explicitly clears the view. Same pointer is a no-op. Rebinding to a
	// different snapshot cancels navigation, retaining immutable CPU resources.
	bool SetScene(std::shared_ptr<const Scene3DSnapshot> scene, String& error);
	std::shared_ptr<const Scene3DSnapshot> GetScene() const;
	bool SetCamera(const Scene3DCamera& camera, String& error);
	Scene3DCamera GetCamera() const;
	bool SetOptions(const Ui3DViewOptions& options, String& error);
	Ui3DViewOptions GetOptions() const;
	bool SetSelection(const std::vector<String>& ids, String& error);
	std::vector<String> GetSelection() const;
	bool FitAll();
	bool FitSelection();
	Ui3DViewPick Pick(Pointf point) const;
	bool IsCurrentPick(const Ui3DViewPick& pick) const;
	uint64 GetSceneBindingRevision() const { return scene_binding_revision; }

	Ui3DViewStats GetViewStats() const;
	String GetViewError() const;
	bool IsNavigating() const { return navigation != Ui3DNavigationMode::None; }
	void CancelNavigation(const String& reason = "cancelled");

	void LeftDown(Point p, dword keyflags) override;
	void LeftUp(Point p, dword keyflags) override;
	void MiddleDown(Point p, dword keyflags) override;
	// U++ has separate middle double/triple virtuals; each press can navigate.
	void MiddleDouble(Point p, dword flags) override { MiddleDown(p, flags); }
	void MiddleTriple(Point p, dword flags) override { MiddleDown(p, flags); }
	void MiddleUp(Point p, dword keyflags) override;
	void RightDown(Point p, dword keyflags) override;
	void RightUp(Point p, dword keyflags) override;
	void MouseMove(Point p, dword keyflags) override;
	void MouseWheel(Point p, int zdelta, dword keyflags) override;
	bool Key(dword key, int count) override;
	void CancelMode() override;
	void LostFocus() override;

protected:
	void State(int reason) override;

private:
	std::shared_ptr<Ui3DViewState> state;
	Scene3DCamera navigation_start;
	Ui3DNavigationMode navigation = Ui3DNavigationMode::None;
	Point press_point = Point(0, 0);
	int navigation_button = 0;
	bool click_pending = false;
	uint64 gesture_id = 0;
	uint64 gesture_scene_revision = 0;
	uint64 scene_binding_revision = 0;
	uint64 gesture_scene_binding_revision = 0;
	uint64 binding_generation = 0;
	TimeCallback navigation_watch, navigation_notice;
	Function<void()> pending_navigation_notice;
	bool destroying = false;

	void FlushNavigationNotice();
	void BeginNavigation(Ui3DNavigationMode mode, int button, Point p);
	void FinishNavigation(bool commit, const String& reason);
	void EmitNavigation(Ui3DNavigationPhase phase, Ui3DNavigationMode mode,
	                    uint64 start_revision, const String& reason);
	void FitFromInput(bool selection);
	void SelectFromInput(Point p, dword flags);
};

}
#endif
