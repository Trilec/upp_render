#include "../../examples/Ui3DViewDemo/ViewerCameraModel.h"
#include <limits>

using namespace Upp;

namespace {

int checks = 0, failures = 0;
using Model = ViewerCameraModel;
using Pane = ViewerCameraModel::PaneState;

void Check(bool condition, const char *message)
{
	++checks;
	if(!condition) { ++failures; Cout() << "FAIL: " << message << EOL; }
}

bool Step(bool success, const char *message, const String& error)
{
	Check(success, message);
	if(!success) Cout() << "  " << error << EOL;
	return success;
}

std::array<Scene3DCamera, Model::PaneCount> InitialCameras()
{
	Scene3DCamera overview;
	overview.yaw = 0.68; overview.pitch = 0.54; overview.distance = 14.6;
	overview.target = {0, 1, 0}; overview.ortho_height = 8.8;
	std::array<Scene3DCamera, Model::PaneCount> cameras;
	for(auto& camera : cameras) camera = overview;
	cameras[0].yaw = 0; cameras[0].pitch = 0; cameras[0].orthographic = true;
	cameras[1].yaw = 0; cameras[1].pitch = 1.5707963267948966; cameras[1].orthographic = true;
	cameras[2].yaw = 1.5707963267948966; cameras[2].pitch = 0; cameras[2].orthographic = true;
	return cameras;
}

Scene3DCamera Different(const Scene3DCamera& from, double amount = 0.17)
{
	Scene3DCamera result = from;
	result.yaw += amount; result.distance += 1.25; result.target.x += 0.5;
	return result;
}

bool Initialize(Model& model)
{
	String error;
	return Step(model.Reset(InitialCameras(), error), "initialize four axis/perspective slots", error);
}

std::array<Pane, Model::PaneCount> Snapshot(const Model& model)
{
	std::array<Pane, Model::PaneCount> out;
	for(int i = 0; i < Model::PaneCount; ++i) out[(size_t)i] = model.GetPane(i);
	return out;
}

bool Exactly(const Pane& a, const Pane& b)
{
	return Model::SameCamera(a.camera, b.camera) && a.camera.revision == b.camera.revision &&
	       a.camera_id == b.camera_id && a.generation == b.generation;
}

bool AllUnchanged(const Model& model, const std::array<Pane, Model::PaneCount>& before)
{
	for(int i = 0; i < Model::PaneCount; ++i)
		if(!Exactly(model.GetPane(i), before[(size_t)i])) return false;
	return true;
}

bool BookmarkIs(const Model& model, const String& id, const Scene3DCamera& camera)
{
	const auto *saved = model.GetCamera(id);
	return saved && Model::SameCamera(saved->camera, camera);
}

void BasicCommands()
{
	Model model;
	if(!Initialize(model)) return;
	String error, first, second;
	const auto initial = Snapshot(model);
	Check(model.GetCameraCount() == 0 && !model.CanUndo(), "reset starts an empty catalogue without undo");
	Check(!model.GetCamera(-1) && !model.GetCamera(0) && !model.GetCamera("absent"),
	      "camera getters reject absent indices and IDs");
	for(int i = 0; i < Model::PaneCount; ++i)
		Check(model.GetPane(i).generation > 1 && model.GetPane(i).camera_id.IsEmpty(),
		      "reset retains stable slots with free cameras and advanced generations");
	Step(model.AddBookmark("  Overview\t", initial[3].camera, first, error), "add standalone seed bookmark", error);
	Check(AllUnchanged(model, initial), "adding a bookmark does not bind or move any pane");
	Check(model.GetCamera(first) && model.GetCamera(first)->name == "Overview", "bookmark name is trimmed");
	Step(model.Save(3, "Overview", second, error), "save active pane with duplicate display name", error);
	Check(first != second && model.GetCameraCount() == 2 && model.GetPane(3).camera_id == second,
	      "duplicate names have independent stable identities and save binds source");
	Check(Model::SameCamera(model.GetPane(3).camera, initial[3].camera), "save does not move source pane");
	Step(model.Load(0, first, error), "load into one target pane", error);
	Check(model.GetPane(0).camera_id == first && Model::SameCamera(model.GetPane(0).camera, initial[3].camera),
	      "load copies pose and projection while binding ID");
	Check(Exactly(model.GetPane(1), initial[1]) && Exactly(model.GetPane(2), initial[2]),
	      "loading one pane leaves sibling cameras untouched");
	const auto loaded = model.GetPane(0);
	Step(model.Load(0, first, error), "reload identical bookmark succeeds", error);
	Check(Exactly(model.GetPane(0), loaded), "identical load has no semantic generation churn");
	Step(model.Free(0, error), "detach without moving", error);
	Check(model.GetPane(0).camera_id.IsEmpty() && Model::SameCamera(model.GetPane(0).camera, loaded.camera) &&
	      model.GetPane(0).generation > loaded.generation, "free changes binding only");
	const auto free = model.GetPane(0);
	Step(model.Free(0, error), "already-free command succeeds", error);
	Check(Exactly(model.GetPane(0), free), "already-free is a no-op");
	auto changed = Different(model.GetPane(3).camera);
	Step(model.ApplyCamera(3, changed, error), "apply silent programmatic camera", error);
	Check(model.GetPane(3).camera_id.IsEmpty() && BookmarkIs(model, second, initial[3].camera),
	      "programmatic edit detaches without modifying saved pose");
	const auto applied = model.GetPane(3);
	changed = applied.camera; changed.revision = std::numeric_limits<uint64>::max();
	Step(model.ApplyCamera(3, changed, error), "foreign revision is accepted as nonsemantic input", error);
	Check(Exactly(model.GetPane(3), applied), "foreign revision does not replace model revision authority");
	Step(model.Rename(first, " New name ", error), "rename existing bookmark", error);
	Check(model.GetCamera(first) && model.GetCamera(first)->name == "New name" &&
	      BookmarkIs(model, first, initial[3].camera), "rename preserves saved camera and ID");
	Check(!model.CanUndo(), "save/load/free/rename/programmatic assignment do not manufacture undo");
}

String Repeated(const char *scalar, int count)
{
	String out;
	for(int i = 0; i < count; ++i) out += scalar;
	return out;
}

void NamesAndRejectedCommands()
{
	String normalized, error;
	const String surrounded = String(" \xc2\xa0") + "\xe3\x80\x80" + "Look \xc3\xa9" + "\xe3\x80\x80\t ";
	Check(Model::NormalizeName(surrounded, normalized, error) && normalized == "Look \xc3\xa9",
	      "UTF-8 names trim Unicode whitespace without damaging characters");
	Check(Model::NormalizeName(Repeated("\xf0\x9f\x8e\xa5", 40), normalized, error) && normalized.GetCount() == 160,
	      "40 supplementary Unicode scalars accepted independently of byte count");
	Check(!Model::NormalizeName(Repeated("\xf0\x9f\x8e\xa5", 41), normalized, error), "41 Unicode scalars rejected");
	Check(Model::NormalizeName(Repeated("a\xcc\x81", 20), normalized, error), "combining marks use documented scalar count");
	Check(!Model::NormalizeName(Repeated("a\xcc\x81", 21), normalized, error), "combining sequences exceeding scalar limit rejected");
	Check(!Model::NormalizeName(" \xc2\xa0\xe3\x80\x80\n", normalized, error), "whitespace-only name rejected");
	Check(!Model::NormalizeName("A\nB", normalized, error), "interior newline rejected");
	Check(!Model::NormalizeName(String("A\0B", 3), normalized, error), "embedded NUL rejected");
	Check(!Model::NormalizeName("A\xe2\x80\xa8" "B", normalized, error), "interior Unicode line separator rejected");
	for(const char *bad : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\xc3("})
		Check(!Model::NormalizeName(bad, normalized, error), "malformed/non-scalar UTF-8 rejected");

	Model model;
	if(!Initialize(model)) return;
	String id, result;
	Step(model.Save(3, "Keep", id, error), "save for failure atomicity", error);
	const auto before = Snapshot(model);
	const int count = model.GetCameraCount();
	Check(!model.Rename(id, " \t", error) && model.GetCamera(id)->name == "Keep", "bad rename preserves current name");
	Check(!model.Save(3, "", result, error) && result.IsEmpty(), "bad save produces no identity");
	Check(!model.Load(0, "missing", error), "unknown camera load rejected");
	Check(!model.Free(-1, error) && !model.ApplyCamera(4, before[0].camera, error), "invalid pane indices rejected");
	auto invalid = before[0].camera; invalid.distance = std::numeric_limits<double>::quiet_NaN();
	Check(!model.ApplyCamera(0, invalid, error), "nonfinite camera assignment rejected");
	Check(!model.AddBookmark("Invalid", invalid, result, error), "invalid standalone snapshot rejected");
	Check(!model.UpdateFrom(0, "missing", error) && !model.Remove("missing", error), "unknown update/remove rejected");
	Check(model.GetCameraCount() == count && AllUnchanged(model, before) && !model.CanUndo(),
	      "all rejected commands leave catalogue/panes/undo untouched");
}

void UpdateUndo()
{
	Model model;
	if(!Initialize(model)) return;
	String error, a, b;
	const auto camera_a = model.GetPane(0).camera, camera_b = model.GetPane(2).camera;
	Step(model.AddBookmark("A", camera_a, a, error), "seed A for update", error);
	Step(model.AddBookmark("B", camera_b, b, error), "seed B for update", error);
	Step(model.Load(0, a, error), "bind first A pane", error);
	Step(model.Load(1, a, error), "bind second A pane", error);
	Step(model.Load(2, b, error), "bind unrelated B pane", error);
	Step(model.ApplyCamera(3, Different(model.GetPane(3).camera), error), "prepare unbound active source", error);
	const auto before = Snapshot(model);
	Step(model.UpdateFrom(3, a, error), "update A from free active source", error);
	Check(model.CanUndo() && BookmarkIs(model, a, before[3].camera), "update publishes saved snapshot and undo");
	for(int i : {0, 1, 3})
		Check(model.GetPane(i).camera_id == a && Model::SameCamera(model.GetPane(i).camera, before[3].camera),
		      "update synchronizes previous bindings and binds active source");
	Check(Exactly(model.GetPane(2), before[2]), "update preserves unrelated bound pane");
	const auto after = Snapshot(model);
	Step(model.Rename(a, "Renamed after update", error), "rename while update undo remains pending", error);
	Step(model.Undo(error), "undo saved-camera update", error);
	Check(BookmarkIs(model, a, camera_a) && model.GetCamera(a)->name == "Renamed after update",
	      "update undo restores pose but preserves later rename");
	for(int i : {0, 1, 3})
		Check(model.GetPane(i).camera_id == before[(size_t)i].camera_id &&
		      Model::SameCamera(model.GetPane(i).camera, before[(size_t)i].camera) &&
		      model.GetPane(i).generation > after[(size_t)i].generation,
		      "undo restores complete guarded pane state without rewinding generation");
	Check(model.GetPane(0).camera.revision > after[0].camera.revision,
	      "undo restores geometry with a new camera revision");
	Check(!model.CanUndo() && !model.Undo(error), "one-step update undo is consumed once");
	Step(model.Load(3, b, error), "bind active source to a different bookmark", error);
	const auto source_before = model.GetPane(3);
	Step(model.UpdateFrom(3, a, error), "update from source bound elsewhere", error);
	Step(model.Undo(error), "undo source binding replacement", error);
	Check(model.GetPane(3).camera_id == b && Model::SameCamera(model.GetPane(3).camera, source_before.camera),
	      "update undo restores active source's previous foreign binding");
}

void UndoPreservesInterveningChanges()
{
	Model model;
	if(!Initialize(model)) return;
	String error, a, extra;
	const auto original = model.GetPane(0).camera;
	Step(model.AddBookmark("Saved", original, a, error), "seed guarded update", error);
	Step(model.Load(0, a, error), "bind guarded pane zero", error);
	Step(model.Load(1, a, error), "bind guarded pane one", error);
	const auto active_before = model.GetPane(3);
	Step(model.UpdateFrom(3, a, error), "record guarded update", error);
	const auto newer = Different(model.GetPane(0).camera, 0.39);
	Step(model.ApplyCamera(0, newer, error), "make later independent pose edit", error);
	Step(model.Free(1, error), "detach another updated pane", error);
	Step(model.Load(1, a, error), "explicitly reload updated bookmark after transaction", error);
	const auto later_load = model.GetPane(1);
	Step(model.AddBookmark("Later addition", model.GetPane(2).camera, extra, error), "add after pending update", error);
	Step(model.Rename(a, "Later name", error), "rename after pending update", error);
	Step(model.Undo(error), "guarded undo with intervening changes", error);
	Check(Model::SameCamera(model.GetPane(0).camera, newer) && model.GetPane(0).camera_id.IsEmpty(),
	      "undo preserves later free-camera pose edit");
	Check(Model::SameCamera(model.GetPane(1).camera, later_load.camera) && model.GetPane(1).camera_id.IsEmpty(),
	      "undo preserves later reloaded pose and detaches superseded saved binding");
	Check(Model::SameCamera(model.GetPane(3).camera, active_before.camera) &&
	      model.GetPane(3).camera_id == active_before.camera_id,
	      "unchanged active source still restores its complete pre-update state");
	Check(model.GetCamera(extra) && BookmarkIs(model, a, original) && model.GetCamera(a)->name == "Later name",
	      "undo preserves later catalogue additions and rename");
}

void RemoveUndo()
{
	Model model;
	if(!Initialize(model)) return;
	String error, a, b, c, d;
	const auto camera = model.GetPane(3).camera;
	Step(model.AddBookmark("A", camera, a, error), "seed removed entry", error);
	Step(model.AddBookmark("B same pose", camera, b, error), "seed same-pose alternate ID", error);
	Step(model.AddBookmark("C", model.GetPane(2).camera, c, error), "seed trailing entry", error);
	Step(model.Load(0, a, error), "bind removal pane zero", error);
	Step(model.Load(1, a, error), "bind removal pane one", error);
	const auto before = Snapshot(model);
	Step(model.Remove(a, error), "remove bound camera", error);
	Check(!model.GetCamera(a) && model.GetCameraCount() == 2 && model.GetCamera(0)->id == b,
	      "remove changes catalogue without reusing IDs");
	Check(model.GetPane(0).camera_id.IsEmpty() && model.GetPane(1).camera_id.IsEmpty() &&
	      Model::SameCamera(model.GetPane(0).camera, before[0].camera) &&
	      Model::SameCamera(model.GetPane(1).camera, before[1].camera),
	      "remove detaches all bindings without moving their cameras");
	Step(model.Load(0, b, error), "bind another ID with identical pose after removal", error);
	Step(model.AddBookmark("D", model.GetPane(2).camera, d, error), "append camera while removal undo pending", error);
	Step(model.Undo(error), "undo camera removal", error);
	Check(model.GetCameraCount() == 4 && model.GetCamera(0)->id == a && model.GetCamera(1)->id == b &&
	      model.GetCamera(2)->id == c && model.GetCamera(3)->id == d,
	      "remove undo restores original order while preserving later additions");
	Check(model.GetPane(0).camera_id == b && model.GetPane(1).camera_id == a,
	      "remove undo restores untouched binding but cannot steal newer same-pose binding");
	Step(model.Load(0, a, error), "restore both original bindings for generation test", error);
	Step(model.Remove(a, error), "remove again for semantic ABA test", error);
	Step(model.Load(0, b, error), "intervening same-pose bind", error);
	Step(model.Free(0, error), "return to same free pose after intervening bind", error);
	Step(model.Undo(error), "undo removal with same-value newer generation", error);
	Check(model.GetPane(0).camera_id.IsEmpty() && model.GetPane(1).camera_id == a,
	      "generation guard prevents ABA restoration even when final pose/free state match");
	Step(model.Remove(a, error), "create first one-step removal", error);
	Step(model.Remove(b, error), "new removal replaces older undo", error);
	Step(model.Undo(error), "undo only latest removal", error);
	Check(!model.GetCamera(a) && model.GetCamera(b) && !model.CanUndo(), "one-step undo does not resurrect older removal");
}

void PreviewTransactions()
{
	Model model;
	if(!Initialize(model)) return;
	String error, id, unused;
	Step(model.Save(3, "Gesture start", id, error), "save preview start", error);
	const auto original = model.GetPane(3);
	Step(model.BeginPreview(3, error), "begin preview without mutation", error);
	Check(model.IsPreviewing(3) && Exactly(model.GetPane(3), original), "begin preserves pose/binding/generation");
	auto native_copy = original.camera; native_copy.revision += 300;
	Step(model.PreviewCamera(3, native_copy, error), "preview revision-only native copy", error);
	Check(Exactly(model.GetPane(3), original), "unchanged preview does not detach saved camera");
	Check(!model.BeginPreview(3, error), "nested preview rejected");
	Check(!model.Save(3, "During preview", unused, error) && !model.Load(3, id, error) && !model.Free(3, error),
	      "external pane commands cannot silently destroy preview transaction");
	Check(!model.ApplyCamera(3, Different(original.camera), error), "programmatic assignment rejects live preview");
	Check(!model.Remove(id, error) && !model.UpdateFrom(0, id, error),
	      "catalogue mutation affecting bound live preview is atomic rejection");
	Check(Exactly(model.GetPane(3), original) && BookmarkIs(model, id, original.camera),
	      "rejected preview-conflicting operations preserve pane and bookmark");
	Step(model.CommitPreview(3, error), "commit stationary preview", error);
	Check(!model.IsPreviewing(3) && Exactly(model.GetPane(3), original), "stationary commit remains bound");
	Step(model.CommitPreview(3, error), "duplicate preview commit is benign", error);
	Step(model.CancelPreview(3, error), "idle cancellation is benign", error);
	Step(model.BeginPreview(3, error), "begin moving preview", error);
	const auto moved = Different(original.camera);
	Step(model.PreviewCamera(3, moved, error), "preview actual pose change", error);
	Check(model.GetPane(3).camera_id.IsEmpty() && BookmarkIs(model, id, original.camera),
	      "actual preview detaches without modifying saved camera");
	const auto changed = model.GetPane(3);
	Step(model.PreviewCamera(3, moved, error), "repeat identical preview", error);
	Check(Exactly(model.GetPane(3), changed), "duplicate preview is a semantic no-op");
	auto invalid = moved; invalid.fov_y = std::numeric_limits<double>::quiet_NaN();
	Check(!model.PreviewCamera(3, invalid, error) && Exactly(model.GetPane(3), changed),
	      "invalid preview leaves last usable state and transaction intact");
	Step(model.Rename(id, "Renamed during drag", error), "rename during detached preview", error);
	Step(model.CancelPreview(3, error), "cancel moving preview", error);
	Check(model.GetPane(3).camera_id == id && Model::SameCamera(model.GetPane(3).camera, original.camera) &&
	      model.GetPane(3).generation > changed.generation && model.GetPane(3).camera.revision > changed.camera.revision,
	      "cancel restores pose/binding with monotonic revisions and generation");
	Check(model.GetCamera(id)->name == "Renamed during drag", "cancellation does not revert catalogue rename");
	Step(model.BeginPreview(3, error), "begin return-to-origin preview", error);
	Step(model.PreviewCamera(3, moved, error), "move away from bookmark", error);
	Step(model.PreviewCamera(3, original.camera, error), "move exactly back to original camera", error);
	Step(model.CommitPreview(3, error), "commit return-to-origin preview", error);
	Check(model.GetPane(3).camera_id.IsEmpty(), "commit stays detached after real movement even if pose returns");
	Check(!model.PreviewCamera(3, moved, error), "preview update without begin is rejected");
	Check(!model.CanUndo(), "preview navigation does not manufacture catalogue undo");
}

void PreviewCatalogueInterleaving()
{
	Model model;
	if(!Initialize(model)) return;
	String error, id;
	Step(model.Save(3, "May disappear", id, error), "save removable preview bookmark", error);
	const auto original = model.GetPane(3).camera;
	Step(model.BeginPreview(3, error), "begin preview before removal", error);
	Step(model.PreviewCamera(3, Different(original), error), "detach before removal", error);
	Step(model.Remove(id, error), "remove now-unbound bookmark during preview", error);
	Step(model.CancelPreview(3, error), "cancel after original bookmark disappears", error);
	Check(model.GetPane(3).camera_id.IsEmpty() && Model::SameCamera(model.GetPane(3).camera, original),
	      "cancel restores old pose without reviving deleted binding");
	Step(model.Undo(error), "restore catalogue after canceled detached preview", error);
	Check(model.GetCamera(id) && model.GetPane(3).camera_id.IsEmpty(),
	      "removal undo only restores bindings captured by that removal");
	Step(model.Load(3, id, error), "reload for updated-bookmark cancellation", error);
	Step(model.BeginPreview(3, error), "begin preview before saved snapshot update", error);
	Step(model.PreviewCamera(3, Different(original), error), "detach before snapshot update", error);
	const auto updated = model.GetPane(0).camera;
	Step(model.UpdateFrom(0, id, error), "update saved snapshot from another pane", error);
	Step(model.CancelPreview(3, error), "cancel after same ID now has a different saved pose", error);
	Check(model.GetPane(3).camera_id.IsEmpty() && Model::SameCamera(model.GetPane(3).camera, original) &&
	      BookmarkIs(model, id, updated), "cancel cannot falsely bind old pose to updated catalogue snapshot");
	Step(model.BeginPreview(0, error), "start stationary preview on pending-undo target", error);
	const auto before = Snapshot(model);
	Check(!model.Undo(error) && model.CanUndo() && AllUnchanged(model, before) && BookmarkIs(model, id, updated),
	      "undo conflicting with a live guarded preview rejects atomically and remains available");
	Step(model.CancelPreview(0, error), "finish preview before retrying undo", error);
	Step(model.Undo(error), "undo succeeds after preview safely ends", error);
}

void ResetAndUndoLifetime()
{
	Model model;
	if(!Initialize(model)) return;
	String error, old_id, new_id;
	Step(model.Save(3, "Old identity", old_id, error), "save before reset", error);
	Step(model.UpdateFrom(0, old_id, error), "create pending update before reset", error);
	Step(model.BeginPreview(2, error), "create unrelated live preview before reset", error);
	const auto before = Snapshot(model);
	auto invalid = InitialCameras(); invalid[1].near_plane = -1;
	Check(!model.Reset(invalid, error) && AllUnchanged(model, before) && model.GetCamera(old_id) &&
	      model.CanUndo() && model.IsPreviewing(2), "invalid reset preserves all model authorities atomically");
	Step(model.Reset(InitialCameras(), error), "explicit valid reset replaces entire workspace camera state", error);
	Check(!model.GetCamera(old_id) && model.GetCameraCount() == 0 && !model.CanUndo(),
	      "reset clears catalogue and undo");
	for(int i = 0; i < Model::PaneCount; ++i)
		Check(!model.IsPreviewing(i) && model.GetPane(i).camera_id.IsEmpty() &&
		      model.GetPane(i).generation > before[(size_t)i].generation,
		      "reset clears previews without reusing pane generations");
	Step(model.Save(3, "New identity", new_id, error), "save after reset", error);
	Check(new_id != old_id, "reset never reuses IDs from the old catalogue");
	Step(model.Remove(new_id, error), "create undo for host expiry", error);
	model.ClearUndo();
	Check(!model.CanUndo() && !model.Undo(error) && !model.GetCamera(new_id),
	      "host can expire one-step undo without side effects");
}

}

CONSOLE_APP_MAIN
{
	BasicCommands();
	NamesAndRejectedCommands();
	UpdateUndo();
	UndoPreservesInterveningChanges();
	RemoveUndo();
	PreviewTransactions();
	PreviewCatalogueInterleaving();
	ResetAndUndoLifetime();
	Cout() << "Ui3DViewCameraTest: " << checks << " checks, " << failures << " failures" << EOL;
	if(!checks || failures) SetExitCode(1);
}
