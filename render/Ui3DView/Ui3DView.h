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
enum class Ui3DDisplayMode { Solid, Wireframe, SolidEdges };
enum class Ui3DGridPlane { XY, YZ, XZ };
enum class Ui3DNavigationTool { Select, Orbit, Pan };

struct Ui3DViewState;

// Value result bound to its originating control, scene binding and projection.
// A current miss is valid too; use IsCurrentPick before accepting queued requests.
struct Ui3DViewPick : Scene3DPick {
	uint64 scene_binding_revision = 0;
	Size viewport_size = Size(0, 0);
private:
	friend class Ui3DView;
	std::weak_ptr<const Ui3DViewState> source_view;
	std::weak_ptr<const Scene3DSnapshot> sampled_scene;
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

enum class Ui3DToolPointerPhase { Begin, Update, Commit, Cancel };

// Frozen scene/camera/start_pick describe the press; point/keyflags/ray describe
// this event in that projection. Caller owns handle hit testing and document edits.
struct Ui3DToolPointerEvent {
	Ui3DToolPointerPhase phase = Ui3DToolPointerPhase::Begin;
	uint64 gesture_id = 0;
	std::shared_ptr<const Scene3DSnapshot> scene;
	Scene3DCamera camera;
	Ui3DViewPick start_pick;
	Point start_point = Point(0, 0), point = Point(0, 0);
	dword keyflags = 0;
	Scene3DRay ray;
	bool ray_valid = false;
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
	Rgba8 edge_color = Rgba8(125, 137, 153, 255);
	bool show_grid = true;
	bool show_axes = true;
	bool wireframe = false; // Legacy flag; takes precedence over solid_edges.
	bool solid_edges = false;
	bool show_selection = true;
	bool adaptive_grid = true;
	bool orbit_leaves_axis_view = false; // Middle-drag from an aligned orthographic view matches framing at the pivot.
	Ui3DGridPlane grid_plane = Ui3DGridPlane::XZ;
	bool lighting = true;
	int grid_extent = 10;     // 1..100 cells on either side of the origin.
	double grid_spacing = 1; // Metres, finite 0.001..10000.
	// Film/safe guides use the camera's physical gate, independent of viewport aspect.
	bool show_gate = false, show_safe = false;
	double matte_opacity = 1, action_safe = .90, title_safe = .80;
	Rgba8 matte_color = Rgba8(0, 0, 0, 255);
	Rgba8 gate_color = Rgba8(190, 194, 201, 255);
	Rgba8 safe_color = Rgba8(145, 151, 161, 255);
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
	uint64 texture_uploads = 0, working_target_rebuilds = 0;
	int resident_textures = 0;
	uint64 resident_texture_bytes = 0; // Immutable asset pixels, excluding neutral/working targets.
	int blended_draws_last_frame = 0, textured_draws_last_frame = 0, gate_draws_last_frame = 0;
	uint64 splat_uploads = 0, splat_sort_updates = 0, splat_sort_us_last_frame = 0;
	uint64 resident_splat_bytes = 0; // Actual retained vertex/index buffers.
	int resident_splat_resources = 0, resident_splat_instances = 0;
	int splat_draws_last_frame = 0, splats_last_frame = 0;
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
	// Offered before left navigation/selection, including double press.
	// False at Begin falls through; false at Update cancels. Terminal returns
	// are ignored. Accepted gestures retain the original callback and scene.
	// SetScene/SetCamera/selection change cancels; publish edits at Commit.
	// Destruction releases capture and suppresses pending notifications.
	Function<bool(const Ui3DToolPointerEvent&)> WhenToolPointer;
	bool IsToolPointerActive() const { return tool_pointer_active; }
	void CancelToolPointer(const String& reason = "cancelled");
	// Token is an event from this active gesture. Null clears its preview.
	// Does not change GetScene(), authoritative binding or frozen gesture context.
	// Picking/rendering use the preview; termination clears it before callbacks.
	bool SetToolPointerPreviewScene(const Ui3DToolPointerEvent& gesture,
	                               std::shared_ptr<const Scene3DSnapshot> scene,
	                               String& error);

	// Null explicitly clears the view. Same pointer only cancels an active pointer
	// tool. Rebinding to a
	// different snapshot cancels navigation, retaining immutable CPU resources.
	bool SetScene(std::shared_ptr<const Scene3DSnapshot> scene, String& error);
	std::shared_ptr<const Scene3DSnapshot> GetScene() const;
	bool SetCamera(const Scene3DCamera& camera, String& error);
	Scene3DCamera GetCamera() const;
	bool SetOptions(const Ui3DViewOptions& options, String& error);
	Ui3DViewOptions GetOptions() const;
	bool SetDisplayMode(Ui3DDisplayMode mode, String& error);
	Ui3DDisplayMode GetDisplayMode() const;
	// Named view applies the exact axis pose, matched orthographic projection and
	// its XY/YZ/XZ grid together. User/invalid direction rejects without mutation.
	bool SetViewDirection(Scene3DViewDirection direction, String& error);
	bool SetOrthographic(bool on, String& error);
	// Left-button tool changes affect future presses only. An active gesture or
	// pending selection click retains the mode captured at its original press.
	bool SetNavigationTool(Ui3DNavigationTool tool, String& error);
	Ui3DNavigationTool GetNavigationTool() const;
	bool SetSelection(const std::vector<String>& ids, String& error);
	std::vector<String> GetSelection() const;
	bool FitAll();
	bool FitSelection();
	bool FitBounds(const Scene3DBounds& bounds);
	Ui3DViewPick Pick(Pointf point) const;
	bool IsCurrentPick(const Ui3DViewPick& pick) const;
	uint64 GetSceneBindingRevision() const { return scene_binding_revision; }

	Ui3DViewStats GetViewStats() const;
	String GetViewError() const;
	bool IsNavigating() const { return navigation != Ui3DNavigationMode::None; }
	void CancelNavigation(const String& reason = "cancelled");

	void LeftDown(Point p, dword keyflags) override;
	// Select double-click requests selection, then frames the hit bounds if the
	// originating pick remains current after the callback. Other tools only drag.
	void LeftDouble(Point p, dword keyflags) override;
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
	void Layout() override;

private:
	std::shared_ptr<Ui3DViewState> state;
	Scene3DCamera navigation_start;
	Ui3DNavigationMode navigation = Ui3DNavigationMode::None;
	Ui3DNavigationTool navigation_tool = Ui3DNavigationTool::Select;
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
	bool tool_pointer_active = false;
	Ui3DToolPointerEvent tool_pointer_event;
	Function<bool(const Ui3DToolPointerEvent&)> tool_pointer_callback;
	TimeCallback tool_pointer_watch;

	bool BeginToolPointer(Point p, dword flags);
	void UpdateToolPointer(Point p, dword flags);
	void FinishToolPointer(bool commit, const String& reason);
	bool ToolPointerContextCurrent() const;
	void ClearToolPointerPreview();

	void FlushNavigationNotice();
	bool ApplyCamera(const Scene3DCamera& camera, bool set_grid_plane,
	                 Ui3DGridPlane grid_plane, String& error);
	void BeginNavigation(Ui3DNavigationMode mode, int button, Point p);
	void FinishNavigation(bool commit, const String& reason);
	void EmitNavigation(Ui3DNavigationPhase phase, Ui3DNavigationMode mode,
	                    uint64 start_revision, const String& reason);
	void FitFromInput(bool selection);
	void SelectFromInput(Point p, dword flags);
};

}
#endif
