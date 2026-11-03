// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#include "ViewerCameraModel.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace Upp {
namespace {

bool NextScalar(const String& text, int& offset, uint32& scalar)
{
	if(offset >= text.GetCount()) return false;
	const byte first = (byte)text[offset++];
	if(first < 0x80) { scalar = first; return true; }
	int remaining;
	uint32 minimum;
	if(first >= 0xc2 && first <= 0xdf) {
		remaining = 1; minimum = 0x80; scalar = first & 0x1f;
	}
	else if(first >= 0xe0 && first <= 0xef) {
		remaining = 2; minimum = 0x800; scalar = first & 0x0f;
	}
	else if(first >= 0xf0 && first <= 0xf4) {
		remaining = 3; minimum = 0x10000; scalar = first & 7;
	}
	else return false;
	if(remaining > text.GetCount() - offset) return false;
	for(int i = 0; i < remaining; ++i) {
		const byte next = (byte)text[offset++];
		if((next & 0xc0) != 0x80) return false;
		scalar = (scalar << 6) | (next & 0x3f);
	}
	return scalar >= minimum && scalar <= 0x10ffff &&
	       !(scalar >= 0xd800 && scalar <= 0xdfff);
}

bool NameWhitespace(uint32 c)
{
	return (c >= 0x09 && c <= 0x0d) || c == 0x20 || c == 0x85 || c == 0xa0 ||
	       c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 ||
	       c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}

Scene3DCamera BookmarkValue(const Scene3DCamera& camera)
{
	Scene3DCamera result = camera;
	// A saved pose is not a replay of a control's revision authority.
	result.revision = 1;
	return result;
}

}

const ViewerCameraModel::PaneState& ViewerCameraModel::GetPane(int pane) const
{
	return panes.at((size_t)pane);
}

int ViewerCameraModel::GetCameraCount() const { return (int)bookmarks.size(); }

const ViewerCameraModel::Bookmark *ViewerCameraModel::GetCamera(int index) const
{
	return index >= 0 && (size_t)index < bookmarks.size() ? &bookmarks[(size_t)index] : nullptr;
}

int ViewerCameraModel::FindIndex(const String& id) const
{
	for(size_t i = 0; i < bookmarks.size(); ++i)
		if(bookmarks[i].id == id) return (int)i;
	return -1;
}

const ViewerCameraModel::Bookmark *ViewerCameraModel::GetCamera(const String& id) const
{
	return GetCamera(FindIndex(id));
}

bool ViewerCameraModel::CheckPane(int pane, String& error) const
{
	if(pane < 0 || pane >= PaneCount) {
		error = "camera pane must be a stable slot in 0..3";
		return false;
	}
	return true;
}

bool ViewerCameraModel::CheckIdle(int pane, String& error) const
{
	if(!CheckPane(pane, error)) return false;
	if(previews[(size_t)pane].active) {
		error = "finish or cancel the camera preview before this command";
		return false;
	}
	return true;
}

bool ViewerCameraModel::SameCamera(const Scene3DCamera& a, const Scene3DCamera& b)
{
	return a.target.x == b.target.x && a.target.y == b.target.y && a.target.z == b.target.z &&
	       a.yaw == b.yaw && a.pitch == b.pitch && a.distance == b.distance &&
	       a.orthographic == b.orthographic && a.fov_y == b.fov_y &&
	       a.ortho_height == b.ortho_height && a.near_plane == b.near_plane &&
	       a.far_plane == b.far_plane;
}

bool ViewerCameraModel::NormalizeName(const String& name, String& normalized, String& error)
{
	auto fail = [&](const char *message) {
		normalized.Clear(); error = message; return false;
	};
	int first = -1, last = 0, position = 0;
	while(position < name.GetCount()) {
		const int start = position;
		uint32 scalar;
		if(!NextScalar(name, position, scalar)) return fail("camera name must be valid UTF-8");
		if(!NameWhitespace(scalar)) {
			if(first < 0) first = start;
			last = position;
		}
	}
	if(first < 0) return fail("camera name must contain 1..40 Unicode scalar values after trimming");
	int count = 0;
	position = first;
	while(position < last) {
		uint32 scalar;
		if(!NextScalar(name, position, scalar)) return fail("camera name must be valid UTF-8");
		if(scalar < 0x20 || (scalar >= 0x7f && scalar <= 0x9f) || scalar == 0x2028 || scalar == 0x2029)
			return fail("camera name cannot contain control characters");
		if(++count > MaxNameScalars)
			return fail("camera name must contain at most 40 Unicode scalar values");
	}
	normalized = String(name.Begin() + first, last - first);
	error.Clear();
	return true;
}

bool ViewerCameraModel::NewId(String& id, String& error) const
{
	if(bookmarks.size() >= (size_t)std::numeric_limits<int>::max() ||
	   next_camera_id == std::numeric_limits<uint64>::max()) {
		error = "camera catalogue identity or count limit reached";
		return false;
	}
	id = String("camera:") + AsString(next_camera_id);
	return true;
}

bool ViewerCameraModel::PreparePane(int pane, const Scene3DCamera& camera,
                                   const String& camera_id, PaneState& replacement,
                                   String& error, bool force_generation) const
{
	if(!CheckPane(pane, error) || !camera.Validate(error)) return false;
	const PaneState& current = panes[(size_t)pane];
	const bool pose_changed = !SameCamera(current.camera, camera);
	if(!pose_changed && current.camera_id == camera_id && !force_generation) {
		replacement = current; error.Clear(); return true;
	}
	if(current.generation == std::numeric_limits<uint64>::max() ||
	   (pose_changed && current.camera.revision == std::numeric_limits<uint64>::max())) {
		error = "camera pane generation or revision exhausted";
		return false;
	}
	replacement = current;
	replacement.camera = camera;
	replacement.camera.revision = current.camera.revision + (pose_changed ? 1 : 0);
	replacement.camera_id = camera_id;
	replacement.generation = current.generation + 1;
	error.Clear();
	return true;
}

bool ViewerCameraModel::Reset(const std::array<Scene3DCamera, PaneCount>& cameras, String& error)
{
	std::array<PaneState, PaneCount> replacement;
	for(int i = 0; i < PaneCount; ++i)
		if(!PreparePane(i, cameras[(size_t)i], String(), replacement[(size_t)i], error, true))
			return false;
	panes = std::move(replacement);
	bookmarks.clear();
	previews = std::array<PreviewState, PaneCount>();
	ClearUndo();
	error.Clear();
	return true;
}

bool ViewerCameraModel::AddBookmark(const String& name, const Scene3DCamera& camera,
                                   String& out_id, String& error)
{
	out_id.Clear();
	String normalized, id;
	if(!camera.Validate(error) || !NormalizeName(name, normalized, error) || !NewId(id, error))
		return false;
	Bookmark bookmark;
	bookmark.id = id; bookmark.name = normalized; bookmark.camera = BookmarkValue(camera);
	bookmarks.push_back(std::move(bookmark));
	++next_camera_id;
	out_id = id;
	error.Clear();
	return true;
}

bool ViewerCameraModel::Save(int pane, const String& name, String& out_id, String& error)
{
	out_id.Clear();
	if(!CheckIdle(pane, error)) return false;
	String normalized, id;
	PaneState replacement;
	if(!NormalizeName(name, normalized, error) || !NewId(id, error) ||
	   !PreparePane(pane, panes[(size_t)pane].camera, id, replacement, error)) return false;
	Bookmark bookmark;
	bookmark.id = id; bookmark.name = normalized;
	bookmark.camera = BookmarkValue(panes[(size_t)pane].camera);
	bookmarks.push_back(std::move(bookmark));
	panes[(size_t)pane] = std::move(replacement);
	++next_camera_id;
	out_id = id;
	error.Clear();
	return true;
}

bool ViewerCameraModel::Load(int pane, const String& id, String& error)
{
	if(!CheckIdle(pane, error)) return false;
	const Bookmark *bookmark = GetCamera(id);
	if(!bookmark) { error = "saved camera does not exist"; return false; }
	PaneState replacement;
	if(!PreparePane(pane, bookmark->camera, bookmark->id, replacement, error)) return false;
	panes[(size_t)pane] = std::move(replacement);
	return true;
}

bool ViewerCameraModel::Free(int pane, String& error)
{
	if(!CheckIdle(pane, error)) return false;
	PaneState replacement;
	if(!PreparePane(pane, panes[(size_t)pane].camera, String(), replacement, error)) return false;
	panes[(size_t)pane] = std::move(replacement);
	return true;
}

bool ViewerCameraModel::Rename(const String& id, const String& name, String& error)
{
	const int index = FindIndex(id);
	if(index < 0) { error = "saved camera does not exist"; return false; }
	String normalized;
	if(!NormalizeName(name, normalized, error)) return false;
	bookmarks[(size_t)index].name = normalized;
	return true;
}

bool ViewerCameraModel::UpdateFrom(int pane, const String& id, String& error)
{
	if(!CheckIdle(pane, error)) return false;
	const int index = FindIndex(id);
	if(index < 0) { error = "saved camera does not exist"; return false; }
	const Scene3DCamera source = panes[(size_t)pane].camera;
	UndoState next;
	next.kind = UndoKind::Update; next.previous = bookmarks[(size_t)index]; next.index = index;
	auto replacement = panes;
	bool changed = !SameCamera(source, next.previous.camera);
	for(int i = 0; i < PaneCount; ++i) {
		if(i != pane && panes[(size_t)i].camera_id != id) continue;
		if(!CheckIdle(i, error) || !PreparePane(i, source, id, replacement[(size_t)i], error))
			return false;
		PaneUndo& saved = next.panes[(size_t)i];
		saved.tracked = true; saved.before = panes[(size_t)i];
		saved.expected_generation = replacement[(size_t)i].generation;
		changed = changed || saved.expected_generation != saved.before.generation;
	}
	if(!changed) { error.Clear(); return true; }
	bookmarks[(size_t)index].camera = BookmarkValue(source);
	panes = std::move(replacement);
	undo = std::move(next);
	error.Clear();
	return true;
}

bool ViewerCameraModel::Remove(const String& id, String& error)
{
	const int index = FindIndex(id);
	if(index < 0) { error = "saved camera does not exist"; return false; }
	UndoState next;
	next.kind = UndoKind::Remove; next.previous = bookmarks[(size_t)index]; next.index = index;
	auto replacement = panes;
	for(int i = 0; i < PaneCount; ++i) {
		if(panes[(size_t)i].camera_id != id) continue;
		if(!CheckIdle(i, error) ||
		   !PreparePane(i, panes[(size_t)i].camera, String(), replacement[(size_t)i], error))
			return false;
		PaneUndo& saved = next.panes[(size_t)i];
		saved.tracked = true; saved.before = panes[(size_t)i];
		saved.expected_generation = replacement[(size_t)i].generation;
	}
	bookmarks.erase(bookmarks.begin() + index);
	panes = std::move(replacement);
	undo = std::move(next);
	error.Clear();
	return true;
}

bool ViewerCameraModel::ApplyCamera(int pane, const Scene3DCamera& camera, String& error)
{
	if(!CheckIdle(pane, error)) return false;
	PaneState replacement;
	if(!PreparePane(pane, camera, String(), replacement, error)) return false;
	panes[(size_t)pane] = std::move(replacement);
	return true;
}

bool ViewerCameraModel::BeginPreview(int pane, String& error)
{
	if(!CheckIdle(pane, error)) return false;
	PreviewState& preview = previews[(size_t)pane];
	preview.before = panes[(size_t)pane]; preview.active = true;
	error.Clear();
	return true;
}

bool ViewerCameraModel::PreviewCamera(int pane, const Scene3DCamera& camera, String& error)
{
	if(!CheckPane(pane, error)) return false;
	if(!previews[(size_t)pane].active) { error = "camera preview has not begun"; return false; }
	if(!camera.Validate(error)) return false;
	if(SameCamera(panes[(size_t)pane].camera, camera)) { error.Clear(); return true; }
	PaneState replacement;
	if(!PreparePane(pane, camera, String(), replacement, error)) return false;
	panes[(size_t)pane] = std::move(replacement);
	return true;
}

bool ViewerCameraModel::CommitPreview(int pane, String& error)
{
	if(!CheckPane(pane, error)) return false;
	previews[(size_t)pane] = PreviewState();
	error.Clear();
	return true;
}

bool ViewerCameraModel::CancelPreview(int pane, String& error)
{
	if(!CheckPane(pane, error)) return false;
	PreviewState& preview = previews[(size_t)pane];
	if(!preview.active) { error.Clear(); return true; }
	String binding;
	if(const Bookmark *bookmark = GetCamera(preview.before.camera_id))
		if(SameCamera(bookmark->camera, preview.before.camera)) binding = bookmark->id;
	PaneState replacement;
	if(!PreparePane(pane, preview.before.camera, binding, replacement, error)) return false;
	panes[(size_t)pane] = std::move(replacement);
	preview = PreviewState();
	return true;
}

bool ViewerCameraModel::IsPreviewing(int pane) const
{
	return pane >= 0 && pane < PaneCount && previews[(size_t)pane].active;
}

bool ViewerCameraModel::CanUndo() const { return undo.kind != UndoKind::None; }
void ViewerCameraModel::ClearUndo() { undo = UndoState(); }

bool ViewerCameraModel::Undo(String& error)
{
	if(!CanUndo()) { error = "no camera update or removal to undo"; return false; }
	auto replacement = panes;
	if(undo.kind == UndoKind::Update) {
		const int index = FindIndex(undo.previous.id);
		if(index < 0) { error = "updated camera no longer exists"; return false; }
		for(int i = 0; i < PaneCount; ++i) {
			const PaneUndo& saved = undo.panes[(size_t)i];
			const PaneState& current = panes[(size_t)i];
			if(saved.tracked && current.generation == saved.expected_generation) {
				if(!CheckIdle(i, error) ||
				   !PreparePane(i, saved.before.camera, saved.before.camera_id, replacement[(size_t)i], error))
					return false;
			}
			else if(current.camera_id == undo.previous.id && !SameCamera(current.camera, undo.previous.camera)) {
				// A later load/edit keeps its pose but cannot remain falsely bound to
				// the older saved snapshot restored by this undo.
				if(!CheckIdle(i, error) ||
				   !PreparePane(i, current.camera, String(), replacement[(size_t)i], error)) return false;
			}
		}
		// Rename is a separate intervening edit: undo only the saved pose.
		bookmarks[(size_t)index].camera = undo.previous.camera;
	}
	else {
		if(FindIndex(undo.previous.id) >= 0 || bookmarks.size() >= (size_t)std::numeric_limits<int>::max()) {
			error = "removed camera cannot be restored into this catalogue";
			return false;
		}
		for(int i = 0; i < PaneCount; ++i) {
			const PaneUndo& saved = undo.panes[(size_t)i];
			const PaneState& current = panes[(size_t)i];
			if(!saved.tracked || current.generation != saved.expected_generation || !current.camera_id.IsEmpty())
				continue;
			if(!CheckIdle(i, error) ||
			   !PreparePane(i, saved.before.camera, saved.before.camera_id, replacement[(size_t)i], error))
				return false;
		}
		const size_t index = std::min((size_t)undo.index, bookmarks.size());
		bookmarks.insert(bookmarks.begin() + index, undo.previous);
	}
	panes = std::move(replacement);
	ClearUndo();
	error.Clear();
	return true;
}

}
