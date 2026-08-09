from pathlib import Path

p = Path('render/GpuCtrl/GpuCtrl.cpp')
s = p.read_text()

old = '#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>\n#include <RenderVulkan/RenderVulkanSurfaceSession.h>\n'
new = '#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>\n#include <RenderCanvas/RenderCanvas.h>\n#include <RenderVulkan/RenderVulkanSurfaceSession.h>\n'
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''static GpuCtrlFrameIntent BuildDefaultFrameIntent(Size size)\n{\n\tGpuCtrlFrameIntent frame;\n\tframe.background = { 0.08f, 0.24f, 0.58f, 1.0f };\n\tif(size.cx <= 0 || size.cy <= 0)\n\t\treturn frame;\n\n\tint rect_width = size.cx / 2;\n\tint rect_height = size.cy / 2;\n\tif(rect_width < 1)\n\t\trect_width = 1;\n\tif(rect_height < 1)\n\t\trect_height = 1;\n\tint rect_left = (size.cx - rect_width) / 2;\n\tint rect_top = (size.cy - rect_height) / 2;\n\tframe.has_fill_rect = true;\n\tframe.fill_rect = RectC(rect_left, rect_top, rect_width, rect_height);\n\tframe.fill_color = { 0.90f, 0.32f, 0.08f, 1.0f };\n\treturn frame;\n}\n'''
new = '''static GpuCtrlFrameColor ToFrameColor(Rgba8 color)\n{\n\tconst float scale = 1.0f / 255.0f;\n\treturn { color.r * scale, color.g * scale, color.b * scale, color.a * scale };\n}\n\nstatic bool ReplaySingleFillRect(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)\n{\n\tif(!list.IsValid()) {\n\t\terror = list.GetError();\n\t\treturn false;\n\t}\n\tif(list.GetCount() != 1 || list[0].type != UiDisplayOpType::FillRect) {\n\t\terror = \"GpuCtrl S16C frame requires exactly one FillRect display operation\";\n\t\treturn false;\n\t}\n\n\tconst UiDisplayOp& op = list[0];\n\tint left = (int)op.rect.left;\n\tint top = (int)op.rect.top;\n\tint right = (int)op.rect.right;\n\tint bottom = (int)op.rect.bottom;\n\tif(right <= left || bottom <= top) {\n\t\terror = \"GpuCtrl S16C FillRect display operation is empty\";\n\t\treturn false;\n\t}\n\n\tframe.has_fill_rect = true;\n\tframe.fill_rect = Rect(left, top, right, bottom);\n\tframe.fill_color = ToFrameColor(op.color);\n\terror.Clear();\n\treturn true;\n}\n\nstatic bool BuildDefaultFrameIntent(Size size, GpuCtrlFrameIntent& frame, String& error)\n{\n\tframe = GpuCtrlFrameIntent();\n\tframe.background = { 0.08f, 0.24f, 0.58f, 1.0f };\n\terror.Clear();\n\tif(size.cx <= 0 || size.cy <= 0)\n\t\treturn true;\n\n\tint rect_width = size.cx / 2;\n\tint rect_height = size.cy / 2;\n\tif(rect_width < 1)\n\t\trect_width = 1;\n\tif(rect_height < 1)\n\t\trect_height = 1;\n\tint rect_left = (size.cx - rect_width) / 2;\n\tint rect_top = (size.cy - rect_height) / 2;\n\n\tUiDisplayListBuilder builder;\n\tbuilder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),\n\t                 Rgba8(230, 82, 20, 255));\n\tUiDisplayList list;\n\tif(!builder.Finish(list)) {\n\t\terror = builder.GetError();\n\t\treturn false;\n\t}\n\treturn ReplaySingleFillRect(list, frame, error);\n}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\t\tSize requested_size = GetNativeHostSize();\n\t\tGpuCtrlFrameIntent frame = BuildDefaultFrameIntent(requested_size);\n\t\tString error;\n\t\tif(backend->Present(requested_size, frame, error)) {\n'''
new = '''\t\tSize requested_size = GetNativeHostSize();\n\t\tGpuCtrlFrameIntent frame;\n\t\tString error;\n\t\tif(!BuildDefaultFrameIntent(requested_size, frame, error)) {\n\t\t\tpresentation_error = error;\n\t\t\treturn false;\n\t\t}\n\t\tif(backend->Present(requested_size, frame, error)) {\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)

p = Path('render/GpuCtrl/GpuCtrl.upp')
s = p.read_text()
old = '''\tCtrlLib,\n\tRenderPlatformWin32,\n\tRenderRhi,\n'''
new = '''\tCtrlLib,\n\tRenderCanvas,\n\tRenderPlatformWin32,\n\tRenderRhi,\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)

p = Path('docs/PROJECT_PLAN.md')
s = p.read_text()
old = '''- S16A adds the first backend-private filled rectangle through the real GpuCtrl\n  presentation lifecycle; S16B moves its background/rectangle description into\n  backend-neutral private frame intent so the Vulkan backend no longer invents\n  control content or geometry\n- general 2D rendering, display-list replay, shaders, painter callbacks, and\n  shared control device ownership remain deferred\n'''
new = '''- S16A adds the first backend-private filled rectangle through the real GpuCtrl\n  presentation lifecycle; S16B moves its background/rectangle description into\n  backend-neutral private frame intent so the Vulkan backend no longer invents\n  control content or geometry\n- S16C records the orange rectangle as one existing UiDisplayList FillRect and\n  replays that neutral operation into the private frame intent; broader display-\n  list replay and renderer state remain deferred\n- general 2D rendering, shaders, painter callbacks, and shared control device\n  ownership remain deferred\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)
