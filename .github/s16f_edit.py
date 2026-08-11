from pathlib import Path

base = "47219f319aeb152de9b1fba81eb46a820a8f3c92"

p = Path("render/GpuCtrl/GpuCtrl.cpp")
s = p.read_text()
old = r'''static bool ReplayFillRectList(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)
{
	if(list.GetCount() <= 0) {
		error = "GpuCtrl S16E frame requires at least one display operation";
		return false;
	}
	if(!list.IsValid()) {
		error = list.GetError();
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
new = r'''struct GpuCtrlReplayState : Moveable<GpuCtrlReplayState> {
	bool has_clip = false;
	Rect clip_rect = Rect(0, 0, 0, 0);
};

static bool ReplayFrameList(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)
{
	if(list.GetCount() <= 0) {
		error = "GpuCtrl S16E frame requires at least one display operation";
		return false;
	}
	if(!list.IsValid()) {
		error = list.GetError();
		return false;
	}

	frame.fill_rects.Clear();
	frame.fill_rects.Reserve(list.GetCount());
	GpuCtrlReplayState state;
	Vector<GpuCtrlReplayState> state_stack;
	for(int i = 0; i < list.GetCount(); ++i) {
		const UiDisplayOp& op = list[i];
		switch(op.type) {
		case UiDisplayOpType::Save: {
			GpuCtrlReplayState& saved = state_stack.Add();
			saved.has_clip = state.has_clip;
			saved.clip_rect = state.clip_rect;
			break;
		}
		case UiDisplayOpType::Restore:
			if(state_stack.IsEmpty()) {
				error = "GpuCtrl S16F restore without matching save";
				frame.fill_rects.Clear();
				return false;
			}
			{
				GpuCtrlReplayState saved = state_stack.Pop();
				state.has_clip = saved.has_clip;
				state.clip_rect = saved.clip_rect;
			}
			break;
		case UiDisplayOpType::ClipRect: {
			Rect next_clip = ToFrameRect(op.rect);
			if(!state.has_clip) {
				state.clip_rect = next_clip;
				state.has_clip = true;
			}
			else
				state.clip_rect = state.clip_rect & next_clip;
			break;
		}
		case UiDisplayOpType::FillRect: {
			Rect draw_rect = ToFrameRect(op.rect);
			if(draw_rect.IsEmpty()) {
				error = "GpuCtrl S16E FillRect display operation is empty";
				frame.fill_rects.Clear();
				return false;
			}
			if(state.has_clip)
				draw_rect = draw_rect & state.clip_rect;
			if(draw_rect.IsEmpty())
				break;

			GpuCtrlFillRectIntent& fill = frame.fill_rects.Add();
			fill.rect = draw_rect;
			fill.color = ToFrameColor(op.color);
			break;
		}
		default:
			error = "GpuCtrl S16F replay supports Save, Restore, ClipRect and FillRect operations only";
			frame.fill_rects.Clear();
			return false;
		}
	}
	if(!state_stack.IsEmpty()) {
		error = "GpuCtrl S16F replay ended with unbalanced save state";
		frame.fill_rects.Clear();
		return false;
	}
	error.Clear();
	return true;
}
'''
assert s.count(old) == 1, "replay block mismatch"
s = s.replace(old, new)
old = r'''	builder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),
	                 Rgba8(230, 82, 20, 255));
	int clip_left = inner_left + inner_width / 2;
	builder.ClipRect(Rectf(clip_left, inner_top, inner_left + inner_width, inner_top + inner_height));
	builder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),
	                 Rgba8(36, 190, 110, 255));
'''
new = r'''	builder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),
	                 Rgba8(230, 82, 20, 255));
	builder.Save();
	int clip_left = inner_left + inner_width / 2;
	builder.ClipRect(Rectf(clip_left, inner_top, inner_left + inner_width, inner_top + inner_height));
	builder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),
	                 Rgba8(36, 190, 110, 255));
	builder.Restore();
'''
assert s.count(old) == 1, "default frame block mismatch"
s = s.replace(old, new)
assert s.count("ReplayFillRectList(list, frame, error)") == 2
s = s.replace("ReplayFillRectList(list, frame, error)", "ReplayFrameList(list, frame, error)")
assert s.count("ReplayFillRectList(list, frame, error)") == 0
assert s.count("ReplayFrameList(list, frame, error)") == 2
p.write_text(s)

p = Path("tests/GpuCtrlReplayTest/main.cpp")
s = p.read_text()
anchor = r'''	UiDisplayListBuilder unsupported_builder;
'''
insert = r'''	UiDisplayListBuilder restore_no_clip_builder;
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

'''
assert s.count(anchor) == 1, "test insert anchor mismatch"
s = s.replace(anchor, insert + anchor)
old_error = 'GpuCtrl S16E replay supports FillRect and ClipRect operations only'
new_error = 'GpuCtrl S16F replay supports Save, Restore, ClipRect and FillRect operations only'
assert s.count(old_error) == 1, "unsupported error expectation mismatch"
s = s.replace(old_error, new_error)
p.write_text(s)

p = Path("docs/PROJECT_PLAN.md")
s = p.read_text()
old = r'''- S16E adds persistent ClipRect replay above the Vulkan boundary: clips intersect
  cumulatively and affect only later FillRects; Save/Restore, transforms and
  general renderer state remain deferred
'''
new = r'''- S16E adds persistent ClipRect replay above the Vulkan boundary: clips intersect
  cumulatively and affect only later FillRects
- S16F adds Save/Restore scoping for that private replay state so ClipRect state
  can be restored deterministically; transforms and general renderer state remain deferred
'''
assert s.count(old) == 1, "project plan block mismatch"
s = s.replace(old, new)
p.write_text(s)
