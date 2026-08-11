from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    s = p.read_text()
    assert s.count(old) == 1, f"expected one anchor in {path}, found {s.count(old)}"
    p.write_text(s.replace(old, new, 1))


replace_once(
    'render/GpuCtrl/GpuCtrl.cpp',
    '#include "GpuCtrl.h"\n#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>\n',
    '#include "GpuCtrl.h"\n#include "GpuCtrlTestHooks.h"\n#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>\n'
)

old = r'''static bool ReplayFillRects(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)
{
	if(!list.IsValid()) {
		error = list.GetError();
		return false;
	}
	if(list.GetCount() <= 0) {
		error = "GpuCtrl S16D frame requires at least one FillRect display operation";
		return false;
	}

	frame.fill_rects.Clear();
	frame.fill_rects.Reserve(list.GetCount());
	for(int i = 0; i < list.GetCount(); ++i) {
		const UiDisplayOp& op = list[i];
		if(op.type != UiDisplayOpType::FillRect) {
			error = "GpuCtrl S16D frame supports FillRect display operations only";
			frame.fill_rects.Clear();
			return false;
		}

		int left = (int)op.rect.left;
		int top = (int)op.rect.top;
		int right = (int)op.rect.right;
		int bottom = (int)op.rect.bottom;
		if(right <= left || bottom <= top) {
			error = "GpuCtrl S16D FillRect display operation is empty";
			frame.fill_rects.Clear();
			return false;
		}

		GpuCtrlFillRectIntent& fill = frame.fill_rects.Add();
		fill.rect = Rect(left, top, right, bottom);
		fill.color = ToFrameColor(op.color);
	}
	error.Clear();
	return true;
}
'''
new = r'''static Rect ToFrameRect(const Rectf& rect)
{
	return Rect((int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom);
}

static bool ReplayFillRectList(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)
{
	if(!list.IsValid()) {
		error = list.GetError();
		return false;
	}
	if(list.GetCount() <= 0) {
		error = "GpuCtrl S16E frame requires at least one display operation";
		return false;
	}

	frame.fill_rects.Clear();
	frame.fill_rects.Reserve(list.GetCount());
	bool has_clip = false;
	Rect clip_rect = Rect(0, 0, 0, 0);
	for(int i = 0; i < list.GetCount(); ++i) {
		const UiDisplayOp& op = list[i];
		switch(op.type) {
		case UiDisplayOpType::ClipRect: {
			Rect next_clip = ToFrameRect(op.rect);
			if(!has_clip) {
				clip_rect = next_clip;
				has_clip = true;
			}
			else
				clip_rect = clip_rect & next_clip;
			break;
		}
		case UiDisplayOpType::FillRect: {
			Rect draw_rect = ToFrameRect(op.rect);
			if(draw_rect.IsEmpty()) {
				error = "GpuCtrl S16E FillRect display operation is empty";
				frame.fill_rects.Clear();
				return false;
			}
			if(has_clip)
				draw_rect = draw_rect & clip_rect;
			if(draw_rect.IsEmpty())
				break;

			GpuCtrlFillRectIntent& fill = frame.fill_rects.Add();
			fill.rect = draw_rect;
			fill.color = ToFrameColor(op.color);
			break;
		}
		default:
			error = "GpuCtrl S16E replay supports FillRect and ClipRect operations only";
			frame.fill_rects.Clear();
			return false;
		}
	}
	error.Clear();
	return true;
}
'''
replace_once('render/GpuCtrl/GpuCtrl.cpp', old, new)

old = r'''	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),
	                 Rgba8(230, 82, 20, 255));
	builder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),
	                 Rgba8(36, 190, 110, 255));
	UiDisplayList list;
	if(!builder.Finish(list)) {
		error = builder.GetError();
		return false;
	}
	return ReplayFillRects(list, frame, error);
}
'''
new = r'''	UiDisplayListBuilder builder;
	builder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),
	                 Rgba8(230, 82, 20, 255));
	int clip_left = inner_left + inner_width / 2;
	builder.ClipRect(Rectf(clip_left, inner_top, inner_left + inner_width, inner_top + inner_height));
	builder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),
	                 Rgba8(36, 190, 110, 255));
	UiDisplayList list;
	if(!builder.Finish(list)) {
		error = builder.GetError();
		return false;
	}
	return ReplayFillRectList(list, frame, error);
}
'''
replace_once('render/GpuCtrl/GpuCtrl.cpp', old, new)

old = r'''static One<GpuCtrlBackendSession> CreateBackendSession(GpuBackendKind kind)
{
	if(kind == GpuBackendKind::Vulkan)
		return new VulkanGpuCtrlBackendSession;
	return One<GpuCtrlBackendSession>();
}

}

// The public control stays tiny; platform/session ownership lives behind this
'''
new = r'''static One<GpuCtrlBackendSession> CreateBackendSession(GpuBackendKind kind)
{
	if(kind == GpuBackendKind::Vulkan)
		return new VulkanGpuCtrlBackendSession;
	return One<GpuCtrlBackendSession>();
}

}

namespace GpuCtrlTestHooks {

static void CopyReplayResult(const GpuCtrlFrameIntent& frame, ReplayResult& out)
{
	out.background.red = frame.background.red;
	out.background.green = frame.background.green;
	out.background.blue = frame.background.blue;
	out.background.alpha = frame.background.alpha;
	out.fill_rects.Clear();
	out.fill_rects.Reserve(frame.fill_rects.GetCount());
	for(const GpuCtrlFillRectIntent& fill : frame.fill_rects) {
		ReplayFillRect& dst = out.fill_rects.Add();
		dst.rect = fill.rect;
		dst.color.red = fill.color.red;
		dst.color.green = fill.color.green;
		dst.color.blue = fill.color.blue;
		dst.color.alpha = fill.color.alpha;
	}
}

bool ReplayDisplayList(const UiDisplayList& list, ReplayResult& out)
{
	GpuCtrlFrameIntent frame;
	String error;
	if(!ReplayFillRectList(list, frame, error)) {
		out.fill_rects.Clear();
		out.error = error;
		return false;
	}
	CopyReplayResult(frame, out);
	out.error.Clear();
	return true;
}

bool BuildDefaultFrame(Size size, ReplayResult& out)
{
	GpuCtrlFrameIntent frame;
	String error;
	if(!BuildDefaultFrameIntent(size, frame, error)) {
		out.fill_rects.Clear();
		out.error = error;
		return false;
	}
	CopyReplayResult(frame, out);
	out.error.Clear();
	return true;
}

} // namespace GpuCtrlTestHooks

// The public control stays tiny; platform/session ownership lives behind this
'''
replace_once('render/GpuCtrl/GpuCtrl.cpp', old, new)

replace_once(
    'render/GpuCtrl/GpuCtrl.upp',
    'file\n\tGpuCtrl.h,\n\tGpuCtrl.cpp;\n',
    'file\n\tGpuCtrl.h,\n\tGpuCtrlTestHooks.h,\n\tGpuCtrl.cpp;\n'
)

Path('render/GpuCtrl/GpuCtrlTestHooks.h').write_text(r'''#pragma once

#include <RenderCanvas/RenderCanvas.h>

namespace Upp {
namespace GpuCtrlTestHooks {

struct ReplayColor {
	float red = 0.0f;
	float green = 0.0f;
	float blue = 0.0f;
	float alpha = 1.0f;
};

struct ReplayFillRect : Moveable<ReplayFillRect> {
	Rect rect = Rect(0, 0, 0, 0);
	ReplayColor color;
};

struct ReplayResult {
	ReplayColor background;
	Vector<ReplayFillRect> fill_rects;
	String error;
};

bool ReplayDisplayList(const UiDisplayList& list, ReplayResult& out);
bool BuildDefaultFrame(Size size, ReplayResult& out);

} // namespace GpuCtrlTestHooks
} // namespace Upp
''')

Path('tests/GpuCtrlReplayTest').mkdir(parents=True, exist_ok=True)
Path('tests/GpuCtrlReplayTest/GpuCtrlReplayTest.upp').write_text(r'''uses
	Core,
	GpuCtrl,
	RenderCanvas;

file
	main.cpp;

mainconfig
	"" = "CONSOLE";
''')

Path('tests/GpuCtrlReplayTest/main.cpp').write_text(r'''#include <Core/Core.h>
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
''')

replace_once(
    'docs/PROJECT_PLAN.md',
    '- TASK-008A1 S15 GpuCtrl Vulkan presentation integration is implemented and\n  awaiting Windows/runtime/visual acceptance\n',
    '- TASK-008A1 S15 GpuCtrl Vulkan presentation integration is accepted\n'
)
replace_once(
    'docs/PROJECT_PLAN.md',
    '- S16D extends the FillRect-only replay proof to ordered operations and carries\n  two fills through one Vulkan dynamic-rendering frame; transforms, clipping and\n  general renderer state remain deferred\n',
    '- S16D extends the FillRect-only replay proof to ordered operations and carries\n  two fills through one Vulkan dynamic-rendering frame\n- S16E adds persistent ClipRect replay above the Vulkan boundary: clips intersect\n  cumulatively and affect only later FillRects; Save/Restore, transforms and\n  general renderer state remain deferred\n'
)
