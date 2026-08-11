from pathlib import Path

BASE = "5927776633dcefdeb6213d05e1e0fd62d407b74d"

p = Path("render/GpuCtrl/GpuCtrl.cpp")
s = p.read_text()

old = r'''static Rect ToFrameRect(const Rectf& rect)
{
	return Rect((int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom);
}

struct GpuCtrlReplayState : Moveable<GpuCtrlReplayState> {
	bool has_clip = false;
	Rect clip_rect = Rect(0, 0, 0, 0);
};
'''
new = r'''static Rect ToFrameRect(const Rectf& rect)
{
	return Rect((int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom);
}

static Rect ToTranslatedFrameRect(const Rectf& rect, const Pointf& translation)
{
	return Rect((int)(rect.left + translation.x), (int)(rect.top + translation.y),
	            (int)(rect.right + translation.x), (int)(rect.bottom + translation.y));
}

static bool IsTranslationOnly(const Transform2D& transform)
{
	return transform.x.x == 1.0 && transform.x.y == 0.0 &&
	       transform.y.x == 0.0 && transform.y.y == 1.0;
}

struct GpuCtrlReplayState : Moveable<GpuCtrlReplayState> {
	bool has_clip = false;
	Rect clip_rect = Rect(0, 0, 0, 0);
	Pointf translation = Pointf(0, 0);
};
'''
assert s.count(old) == 1, "replay helper/state block mismatch"
s = s.replace(old, new)

old = r'''		case UiDisplayOpType::Save: {
			GpuCtrlReplayState& saved = state_stack.Add();
			saved.has_clip = state.has_clip;
			saved.clip_rect = state.clip_rect;
			break;
		}
'''
new = r'''		case UiDisplayOpType::Save: {
			GpuCtrlReplayState& saved = state_stack.Add();
			saved.has_clip = state.has_clip;
			saved.clip_rect = state.clip_rect;
			saved.translation = state.translation;
			break;
		}
'''
assert s.count(old) == 1, "Save block mismatch"
s = s.replace(old, new)

old = r'''			{
				GpuCtrlReplayState saved = state_stack.Pop();
				state.has_clip = saved.has_clip;
				state.clip_rect = saved.clip_rect;
			}
			break;
		case UiDisplayOpType::ClipRect: {
'''
new = r'''			{
				GpuCtrlReplayState saved = state_stack.Pop();
				state.has_clip = saved.has_clip;
				state.clip_rect = saved.clip_rect;
				state.translation = saved.translation;
			}
			break;
		case UiDisplayOpType::ClipRect: {
'''
assert s.count(old) == 1, "Restore block mismatch"
s = s.replace(old, new)

old = r'''		case UiDisplayOpType::ClipRect: {
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
'''
new = r'''		case UiDisplayOpType::ClipRect: {
			// Match RenderSoftware: ClipRect is target/device-space state, while
			// ConcatTransform affects subsequent drawable geometry.
			Rect next_clip = ToFrameRect(op.rect);
			if(!state.has_clip) {
				state.clip_rect = next_clip;
				state.has_clip = true;
			}
			else
				state.clip_rect = state.clip_rect & next_clip;
			break;
		}
		case UiDisplayOpType::ConcatTransform:
			if(!IsTranslationOnly(op.transform)) {
				error = "GpuCtrl S16G supports translation-only ConcatTransform operations";
				frame.fill_rects.Clear();
				return false;
			}
			state.translation.x += op.transform.t.x;
			state.translation.y += op.transform.t.y;
			break;
		case UiDisplayOpType::FillRect: {
			Rect draw_rect = ToTranslatedFrameRect(op.rect, state.translation);
'''
assert s.count(old) == 1, "ClipRect/FillRect block mismatch"
s = s.replace(old, new)

old = 'GpuCtrl S16F replay supports Save, Restore, ClipRect and FillRect operations only'
new = 'GpuCtrl S16G replay supports Save, Restore, ClipRect, ConcatTransform and FillRect operations only'
assert s.count(old) == 1, "unsupported replay error mismatch"
s = s.replace(old, new)

old = r'''	builder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),
	                 Rgba8(230, 82, 20, 255));
	builder.Save();
	int clip_left = inner_left + inner_width / 2;
	builder.ClipRect(Rectf(clip_left, inner_top, inner_left + inner_width, inner_top + inner_height));
	builder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),
	                 Rgba8(36, 190, 110, 255));
	builder.Restore();
'''
new = r'''	builder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),
	                 Rgba8(230, 82, 20, 255));
	builder.Save();
	int translation_x = inner_width / 4;
	builder.ConcatTransform(Transform2D::Translation(translation_x, 0));
	int translated_inner_left = inner_left + translation_x;
	int clip_left = translated_inner_left + inner_width / 2;
	builder.ClipRect(Rectf(clip_left, inner_top,
	                       translated_inner_left + inner_width, inner_top + inner_height));
	builder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),
	                 Rgba8(36, 190, 110, 255));
	builder.Restore();
'''
assert s.count(old) == 1, "default frame draw block mismatch"
s = s.replace(old, new)
p.write_text(s)

p = Path("tests/GpuCtrlReplayTest/main.cpp")
s = p.read_text()
old = 'SameRect(result.fill_rects[1].rect, Rect(100, 45, 125, 75))'
new = 'SameRect(result.fill_rects[1].rect, Rect(112, 45, 137, 75))'
assert s.count(old) == 1, "default green expectation mismatch"
s = s.replace(old, new)
s = s.replace('"inner FillRect should be clipped to its right half"',
              '"translated inner FillRect should keep its shifted right half"')

anchor = r'''	UiDisplayListBuilder unsupported_builder;
'''
insert = r'''	UiDisplayListBuilder translation_builder;
	translation_builder.ConcatTransform(Transform2D::Translation(7, -3));
	translation_builder.FillRect(Rectf(10, 10, 20, 20), Rgba8(10, 20, 30, 255));
	UiDisplayList translation;
	ok &= Check(translation_builder.Finish(translation), "translation display list should finish");
	ReplayResult translation_result;
	ok &= Check(ReplayDisplayList(translation, translation_result), "translation replay should succeed");
	ok &= Check(translation_result.fill_rects.GetCount() == 1, "translation replay should retain one fill");
	if(translation_result.fill_rects.GetCount() == 1)
		ok &= Check(SameRect(translation_result.fill_rects[0].rect, Rect(17, 7, 27, 17)),
		            "translation should offset subsequent FillRect geometry");

	UiDisplayListBuilder composed_translation_builder;
	composed_translation_builder.ConcatTransform(Transform2D::Translation(5, 7));
	composed_translation_builder.ConcatTransform(Transform2D::Translation(-2, 3));
	composed_translation_builder.FillRect(Rectf(1, 2, 11, 12), Rgba8(40, 50, 60, 255));
	UiDisplayList composed_translation;
	ok &= Check(composed_translation_builder.Finish(composed_translation), "composed translation display list should finish");
	ReplayResult composed_translation_result;
	ok &= Check(ReplayDisplayList(composed_translation, composed_translation_result), "composed translations should replay");
	ok &= Check(composed_translation_result.fill_rects.GetCount() == 1, "composed translations should retain one fill");
	if(composed_translation_result.fill_rects.GetCount() == 1)
		ok &= Check(SameRect(composed_translation_result.fill_rects[0].rect, Rect(4, 12, 14, 22)),
		            "translation-only ConcatTransform operations should compose additively");

	UiDisplayListBuilder restore_translation_builder;
	restore_translation_builder.ConcatTransform(Transform2D::Translation(4, 5));
	restore_translation_builder.Save();
	restore_translation_builder.ConcatTransform(Transform2D::Translation(10, -2));
	restore_translation_builder.FillRect(Rectf(0, 0, 10, 10), Rgba8(70, 80, 90, 255));
	restore_translation_builder.Restore();
	restore_translation_builder.FillRect(Rectf(0, 0, 10, 10), Rgba8(100, 110, 120, 255));
	UiDisplayList restore_translation;
	ok &= Check(restore_translation_builder.Finish(restore_translation), "translation Save/Restore display list should finish");
	ReplayResult restore_translation_result;
	ok &= Check(ReplayDisplayList(restore_translation, restore_translation_result), "Save/Restore should scope translation state");
	ok &= Check(restore_translation_result.fill_rects.GetCount() == 2, "translation Save/Restore should retain two fills");
	if(restore_translation_result.fill_rects.GetCount() == 2) {
		ok &= Check(SameRect(restore_translation_result.fill_rects[0].rect, Rect(14, 3, 24, 13)),
		            "FillRect inside saved translation should use the nested translation");
		ok &= Check(SameRect(restore_translation_result.fill_rects[1].rect, Rect(4, 5, 14, 15)),
		            "Restore should reinstate the earlier translation");
	}

	UiDisplayListBuilder translated_clip_builder;
	translated_clip_builder.ConcatTransform(Transform2D::Translation(10, 0));
	translated_clip_builder.ClipRect(Rectf(0, 0, 20, 20));
	translated_clip_builder.FillRect(Rectf(0, 0, 20, 20), Rgba8(130, 140, 150, 255));
	UiDisplayList translated_clip;
	ok &= Check(translated_clip_builder.Finish(translated_clip), "translated clip display list should finish");
	ReplayResult translated_clip_result;
	ok &= Check(ReplayDisplayList(translated_clip, translated_clip_result), "translation with ClipRect should replay");
	ok &= Check(translated_clip_result.fill_rects.GetCount() == 1, "translated clip replay should retain one visible fill");
	if(translated_clip_result.fill_rects.GetCount() == 1)
		ok &= Check(SameRect(translated_clip_result.fill_rects[0].rect, Rect(10, 0, 20, 20)),
		            "ClipRect should remain in target coordinates while FillRect is translated");

	UiDisplayListBuilder scale_builder;
	scale_builder.ConcatTransform(Transform2D::Scale(2.0));
	scale_builder.FillRect(Rectf(0, 0, 10, 10), Rgba8(160, 170, 180, 255));
	UiDisplayList scale;
	ok &= Check(scale_builder.Finish(scale), "scale display list should record normally");
	ReplayResult scale_result;
	ok &= Check(!ReplayDisplayList(scale, scale_result), "S16G replay should reject non-translation transforms");
	ok &= Check(scale_result.error == "GpuCtrl S16G supports translation-only ConcatTransform operations",
	            "non-translation transform should report the deterministic S16G error");

'''
assert s.count(anchor) == 1, "translation test insert anchor mismatch"
s = s.replace(anchor, insert + anchor)
old_error = 'GpuCtrl S16F replay supports Save, Restore, ClipRect and FillRect operations only'
new_error = 'GpuCtrl S16G replay supports Save, Restore, ClipRect, ConcatTransform and FillRect operations only'
assert s.count(old_error) == 1, "unsupported expectation mismatch"
s = s.replace(old_error, new_error)
s = s.replace('"unsupported operation should report the deterministic S16F error"',
              '"unsupported operation should report the deterministic S16G error"')
p.write_text(s)

p = Path("docs/PROJECT_PLAN.md")
s = p.read_text()
old = r'''- S16F adds Save/Restore scoping for that private replay state so ClipRect state
  can be restored deterministically; transforms and general renderer state remain deferred
'''
new = r'''- S16F adds Save/Restore scoping for that private replay state so ClipRect state
  can be restored deterministically
- S16G adds translation-only ConcatTransform replay for FillRect geometry, scoped by
  Save/Restore; scale, rotation, shear and general renderer state remain deferred
'''
assert s.count(old) == 1, "project plan S16F block mismatch"
s = s.replace(old, new)
p.write_text(s)
