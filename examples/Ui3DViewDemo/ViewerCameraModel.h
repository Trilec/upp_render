// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#ifndef _Ui3DViewDemo_ViewerCameraModel_h_
#define _Ui3DViewDemo_ViewerCameraModel_h_

#include <RenderScene/Scene3D.h>
#include <array>
#include <vector>

namespace Upp {

// Application-owned camera catalogue and four stable camera slots. No Ctrl,
// renderer, layout, selection or callback ownership. The host serializes access.
// Bookmarks are copied values and change only through these command methods.
class ViewerCameraModel {
public:
	static constexpr int PaneCount = 4;
	static constexpr int MaxNameScalars = 40;

	struct PaneState {
		Scene3DCamera camera;
		String camera_id; // Empty means Free camera.
		uint64 generation = 1; // Monotonic semantic pose/binding version.
	};
	struct Bookmark {
		String id, name;
		Scene3DCamera camera;
	};

	ViewerCameraModel() = default;
	const PaneState& GetPane(int pane) const; // Bounds checked; slots are 0..3.
	int GetCameraCount() const;
	const Bookmark *GetCamera(int index) const;
	const Bookmark *GetCamera(const String& id) const;
	const Bookmark *FindCamera(const String& id) const { return GetCamera(id); }
	// References/pointers are inspection-only; keep IDs/copies across mutations.

	// Reset is an explicit host operation. It clears catalogue, undo and previews,
	// preserves slot identity/generation monotonicity, and never reuses an old ID.
	// The host supplies Front/Top/Right/Perspective defaults and any seed bookmarks.
	bool Reset(const std::array<Scene3DCamera, PaneCount>& cameras, String& error);
	bool AddBookmark(const String& name, const Scene3DCamera& camera,
	                 String& out_id, String& error);
	bool Save(int pane, const String& name, String& out_id, String& error);
	bool Load(int pane, const String& id, String& error);
	bool Free(int pane, String& error);
	bool Rename(const String& id, const String& name, String& error);
	bool UpdateFrom(int pane, const String& id, String& error);
	bool Remove(const String& id, String& error);

	// Silent programmatic assignment to Free camera. Input revision is validated
	// but not adopted: this model and each native control own separate revisions.
	// Commands affecting a live preview reject; finish/cancel the gesture first.
	bool ApplyCamera(int pane, const Scene3DCamera& camera, String& error);
	// Preview starts without detaching. Actual pose change detaches; cancellation
	// restores the starting pose and its binding only if that snapshot still exists.
	bool BeginPreview(int pane, String& error);
	bool PreviewCamera(int pane, const Scene3DCamera& camera, String& error);
	bool CommitPreview(int pane, String& error); // Idempotent when already idle.
	bool CancelPreview(int pane, String& error); // Idempotent when already idle.
	bool IsPreviewing(int pane) const;

	// One step, update/remove only; the host owns toast lifetime and persistence.
	// Undo restores guarded pane snapshots only if they have not changed since
	// the action. Newer pane edits and later renames survive. A newer pane still
	// bound to the reverted bookmark becomes free without moving. This deliberately
	// improves on the HTML's unconditional camera-update rollback.
	bool CanUndo() const;
	bool Undo(String& error);
	void ClearUndo();

	// Exact semantic equality excludes revision. No approximate comparisons can
	// accidentally preserve a bookmark binding after a real lens/pose edit.
	static bool SameCamera(const Scene3DCamera& a, const Scene3DCamera& b);
	// Names are strict UTF-8: 1..40 Unicode scalar values after trimming Unicode
	// White_Space at both ends. Interior C0/C1 controls and Unicode line/paragraph
	// separators are rejected; combining marks count separately. Duplicate names
	// are allowed; IDs carry identity.
	static bool NormalizeName(const String& name, String& normalized, String& error);

private:
	struct PreviewState {
		bool active = false;
		PaneState before;
	};
	struct PaneUndo {
		bool tracked = false;
		PaneState before;
		uint64 expected_generation = 0;
	};
	enum class UndoKind { None, Update, Remove };
	struct UndoState {
		UndoKind kind = UndoKind::None;
		Bookmark previous;
		int index = 0;
		std::array<PaneUndo, PaneCount> panes;
	};

	std::array<PaneState, PaneCount> panes;
	std::array<PreviewState, PaneCount> previews;
	std::vector<Bookmark> bookmarks;
	uint64 next_camera_id = 1;
	UndoState undo;

	bool CheckPane(int pane, String& error) const;
	bool CheckIdle(int pane, String& error) const;
	int FindIndex(const String& id) const;
	bool NewId(String& id, String& error) const;
	bool PreparePane(int pane, const Scene3DCamera& camera, const String& camera_id,
	                 PaneState& replacement, String& error, bool force_generation = false) const;
};

}

#endif
