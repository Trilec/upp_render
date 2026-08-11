from pathlib import Path

p = Path('render/GpuCtrl/GpuCtrl.cpp')
s = p.read_text()
old = '''static bool ReplayFillRectList(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)\n{\n\tif(!list.IsValid()) {\n\t\terror = list.GetError();\n\t\treturn false;\n\t}\n\tif(list.GetCount() <= 0) {\n\t\terror = "GpuCtrl S16E frame requires at least one display operation";\n\t\treturn false;\n\t}\n'''
new = '''static bool ReplayFillRectList(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)\n{\n\tif(list.GetCount() <= 0) {\n\t\terror = "GpuCtrl S16E frame requires at least one display operation";\n\t\treturn false;\n\t}\n\tif(!list.IsValid()) {\n\t\terror = list.GetError();\n\t\treturn false;\n\t}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)

p = Path('tests/GpuCtrlReplayTest/main.cpp')
s = p.read_text()
old = '''static bool SameRect(const Rect& a, const Rect& b)\n{\n\treturn a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;\n}\n'''
new = '''static bool SameRect(const Rect& a, const Rect& b)\n{\n\treturn a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;\n}\n\nstatic bool NearFloat(float actual, float expected)\n{\n\treturn fabs(actual - expected) <= 0.000001f;\n}\n\nstatic bool SameColor(const ReplayColor& actual, Rgba8 expected)\n{\n\treturn NearFloat(actual.red, expected.r / 255.0f) &&\n\t       NearFloat(actual.green, expected.g / 255.0f) &&\n\t       NearFloat(actual.blue, expected.b / 255.0f) &&\n\t       NearFloat(actual.alpha, expected.a / 255.0f);\n}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\t\tok &= Check(result.fill_rects[0].color.red == 230.0f / 255.0f &&\n\t\t            result.fill_rects[0].color.green == 82.0f / 255.0f &&\n\t\t            result.fill_rects[1].color.red == 36.0f / 255.0f &&\n\t\t            result.fill_rects[1].color.green == 190.0f / 255.0f,\n\t\t            "clipping should preserve FillRect colour payloads");\n'''
new = '''\t\tok &= Check(SameColor(result.fill_rects[0].color, Rgba8(230, 82, 20, 255)) &&\n\t\t            SameColor(result.fill_rects[1].color, Rgba8(36, 190, 110, 255)),\n\t\t            "clipping should preserve all FillRect colour channels");\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tUiDisplayList empty;\n\tReplayResult empty_result;\n\tok &= Check(!ReplayDisplayList(empty, empty_result), "empty display list should be rejected by the focused replay path");\n\tok &= Check(empty_result.error == "GpuCtrl S16E frame requires at least one display operation",\n\t            "empty list should report the deterministic S16E error");\n\n\tif(ok) {\n'''
new = '''\tUiDisplayList empty;\n\tReplayResult empty_result;\n\tok &= Check(!ReplayDisplayList(empty, empty_result), "empty display list should be rejected by the focused replay path");\n\tok &= Check(empty_result.error == "GpuCtrl S16E frame requires at least one display operation",\n\t            "empty list should report the deterministic S16E error");\n\n\tUiDisplayListBuilder invalid_builder;\n\tinvalid_builder.Save();\n\tUiDisplayList invalid;\n\tok &= Check(!invalid_builder.Finish(invalid), "unbalanced display list should finish invalid");\n\tReplayResult invalid_result;\n\tok &= Check(!ReplayDisplayList(invalid, invalid_result), "non-empty invalid display list should be rejected");\n\tok &= Check(invalid_result.error == "unbalanced save depth at finish",\n\t            "non-empty invalid list should preserve its builder error");\n\n\tif(ok) {\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)
