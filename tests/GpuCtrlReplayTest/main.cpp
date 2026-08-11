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
		ok &= Check(result.fill_rects[0].color.red == 230.0f / 255.0f &&
		            result.fill_rects[0].color.green == 82.0f / 255.0f &&
		            result.fill_rects[1].color.red == 36.0f / 255.0f &&
		            result.fill_rects[1].color.green == 190.0f / 255.0f,
		            "clipping should preserve FillRect colour payloads");
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

	UiDisplayListBuilder unsupported_builder;
	unsupported_builder.StrokeRect(Rectf(0, 0, 20, 20), 1.0, Rgba8(255, 255, 255, 255));
	UiDisplayList unsupported;
	ok &= Check(unsupported_builder.Finish(unsupported), "unsupported-op display list should still record normally");
	ReplayResult unsupported_result;
	ok &= Check(!ReplayDisplayList(unsupported, unsupported_result), "GpuCtrl replay should reject unsupported display operations");
	ok &= Check(unsupported_result.error == "GpuCtrl S16E replay supports FillRect and ClipRect operations only",
	            "unsupported operation should report the deterministic S16E error");

	UiDisplayList empty;
	ReplayResult empty_result;
	ok &= Check(!ReplayDisplayList(empty, empty_result), "empty display list should be rejected by the focused replay path");
	ok &= Check(empty_result.error == "GpuCtrl S16E frame requires at least one display operation",
	            "empty list should report the deterministic S16E error");

	if(ok) {
		Cout() << "GpuCtrlReplayTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
