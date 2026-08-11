#include <Core/Core.h>
#include <GpuCtrl/GpuCtrlTestHooks.h>

using namespace Upp;
using namespace Upp::GpuCtrlTestHooks;

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static bool SameRect(const Rect& a, const Rect& b)
{
	return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

static bool NearFloat(float actual, float expected)
{
	return fabs(actual - expected) <= 0.000001f;
}

static bool SameColor(const ReplayColor& actual, Rgba8 expected)
{
	return NearFloat(actual.red, expected.r / 255.0f) &&
	       NearFloat(actual.green, expected.g / 255.0f) &&
	       NearFloat(actual.blue, expected.b / 255.0f) &&
	       NearFloat(actual.alpha, expected.a / 255.0f);
}

CONSOLE_APP_MAIN
{
	bool ok = true;

	ReplayResult result;
	ok &= Check(BuildDefaultFrame(Size(200, 120), result), "default clipped frame should build");
	ok &= Check(result.error.IsEmpty(), "default clipped frame should have no replay error");
	ok &= Check(result.fill_rects.GetCount() == 2, "default clipped frame should retain two visible fills");
	if(result.fill_rects.GetCount() == 2) {
		ok &= Check(SameRect(result.fill_rects[0].rect, Rect(50, 30, 150, 90)),
		            "outer FillRect should remain unaffected before ClipRect");
		ok &= Check(SameRect(result.fill_rects[1].rect, Rect(100, 45, 125, 75)),
		            "inner FillRect should be clipped to its right half");
		ok &= Check(SameColor(result.fill_rects[0].color, Rgba8(230, 82, 20, 255)) &&
		            SameColor(result.fill_rects[1].color, Rgba8(36, 190, 110, 255)),
		            "clipping should preserve all FillRect colour channels");
	}

	UiDisplayListBuilder cumulative_builder;
	cumulative_builder.FillRect(Rectf(0, 0, 100, 100), Rgba8(200, 40, 30, 255));
	cumulative_builder.ClipRect(Rectf(10, 10, 90, 90));
	cumulative_builder.ClipRect(Rectf(30, 20, 80, 70));
	cumulative_builder.FillRect(Rectf(0, 0, 100, 100), Rgba8(20, 180, 70, 255));
	UiDisplayList cumulative;
	ok &= Check(cumulative_builder.Finish(cumulative), "cumulative clip display list should finish");
	ReplayResult cumulative_result;
	ok &= Check(ReplayDisplayList(cumulative, cumulative_result), "cumulative ClipRect replay should succeed");
	ok &= Check(cumulative_result.fill_rects.GetCount() == 2, "cumulative clip replay should keep both visible fills");
	if(cumulative_result.fill_rects.GetCount() == 2) {
		ok &= Check(SameRect(cumulative_result.fill_rects[0].rect, Rect(0, 0, 100, 100)),
		            "FillRect before clips should stay unmodified");
		ok &= Check(SameRect(cumulative_result.fill_rects[1].rect, Rect(30, 20, 80, 70)),
		            "multiple ClipRects should intersect for later fills");
	}

	UiDisplayListBuilder clipped_out_builder;
	clipped_out_builder.ClipRect(Rectf(0, 0, 10, 10));
	clipped_out_builder.FillRect(Rectf(20, 20, 30, 30), Rgba8(1, 2, 3, 255));
	UiDisplayList clipped_out;
	ok &= Check(clipped_out_builder.Finish(clipped_out), "fully clipped display list should finish");
	ReplayResult clipped_out_result;
	ok &= Check(ReplayDisplayList(clipped_out, clipped_out_result), "fully clipped FillRect should be a valid no-op");
	ok &= Check(clipped_out_result.fill_rects.IsEmpty(), "fully clipped FillRect should emit no backend fill");

	UiDisplayListBuilder restore_no_clip_builder;
	restore_no_clip_builder.Save();
	restore_no_clip_builder.ClipRect(Rectf(10, 10, 40, 40));
	restore_no_clip_builder.FillRect(Rectf(0, 0, 50, 50), Rgba8(70, 80, 90, 255));
	restore_no_clip_builder.Restore();
	restore_no_clip_builder.FillRect(Rectf(0, 0, 50, 50), Rgba8(100, 110, 120, 255));
	UiDisplayList restore_no_clip;
	ok &= Check(restore_no_clip_builder.Finish(restore_no_clip), "Save/Restore no-clip display list should finish");
	ReplayResult restore_no_clip_result;
	ok &= Check(ReplayDisplayList(restore_no_clip, restore_no_clip_result), "Save/Restore should restore the previous no-clip state");
	ok &= Check(restore_no_clip_result.fill_rects.GetCount() == 2, "Save/Restore no-clip replay should retain two visible fills");
	if(restore_no_clip_result.fill_rects.GetCount() == 2) {
		ok &= Check(SameRect(restore_no_clip_result.fill_rects[0].rect, Rect(10, 10, 40, 40)),
		            "FillRect inside saved clip should be clipped");
		ok &= Check(SameRect(restore_no_clip_result.fill_rects[1].rect, Rect(0, 0, 50, 50)),
		            "Restore should remove a clip introduced after Save");
	}

	UiDisplayListBuilder restore_outer_clip_builder;
	restore_outer_clip_builder.ClipRect(Rectf(5, 5, 45, 45));
	restore_outer_clip_builder.Save();
	restore_outer_clip_builder.ClipRect(Rectf(15, 10, 35, 30));
	restore_outer_clip_builder.FillRect(Rectf(0, 0, 50, 50), Rgba8(130, 140, 150, 255));
	restore_outer_clip_builder.Restore();
	restore_outer_clip_builder.FillRect(Rectf(0, 0, 50, 50), Rgba8(160, 170, 180, 255));
	UiDisplayList restore_outer_clip;
	ok &= Check(restore_outer_clip_builder.Finish(restore_outer_clip), "nested clip Save/Restore display list should finish");
	ReplayResult restore_outer_clip_result;
	ok &= Check(ReplayDisplayList(restore_outer_clip, restore_outer_clip_result), "Restore should reinstate the previous broader clip");
	ok &= Check(restore_outer_clip_result.fill_rects.GetCount() == 2, "nested clip Save/Restore should retain two visible fills");
	if(restore_outer_clip_result.fill_rects.GetCount() == 2) {
		ok &= Check(SameRect(restore_outer_clip_result.fill_rects[0].rect, Rect(15, 10, 35, 30)),
		            "nested ClipRect should constrain FillRect inside saved state");
		ok &= Check(SameRect(restore_outer_clip_result.fill_rects[1].rect, Rect(5, 5, 45, 45)),
		            "Restore should reinstate the earlier broader ClipRect");
	}

	UiDisplayListBuilder unsupported_builder;
	unsupported_builder.StrokeRect(Rectf(0, 0, 20, 20), 1.0, Rgba8(255, 255, 255, 255));
	UiDisplayList unsupported;
	ok &= Check(unsupported_builder.Finish(unsupported), "unsupported-op display list should still record normally");
	ReplayResult unsupported_result;
	ok &= Check(!ReplayDisplayList(unsupported, unsupported_result), "GpuCtrl replay should reject unsupported display operations");
	ok &= Check(unsupported_result.error == "GpuCtrl S16F replay supports Save, Restore, ClipRect and FillRect operations only",
	            "unsupported operation should report the deterministic S16F error");

	UiDisplayList empty;
	ReplayResult empty_result;
	ok &= Check(!ReplayDisplayList(empty, empty_result), "empty display list should be rejected by the focused replay path");
	ok &= Check(empty_result.error == "GpuCtrl S16E frame requires at least one display operation",
	            "empty list should report the deterministic S16E error");

	UiDisplayListBuilder invalid_builder;
	invalid_builder.Save();
	UiDisplayList invalid;
	ok &= Check(!invalid_builder.Finish(invalid), "unbalanced display list should finish invalid");
	ReplayResult invalid_result;
	ok &= Check(!ReplayDisplayList(invalid, invalid_result), "non-empty invalid display list should be rejected");
	ok &= Check(invalid_result.error == "unbalanced save depth at finish",
	            "non-empty invalid list should preserve its builder error");

	if(ok) {
		Cout() << "GpuCtrlReplayTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
