from pathlib import Path

p = Path('render/GpuCtrl/GpuCtrl.cpp')
s = p.read_text()

old = '''namespace {\n\nclass GpuCtrlBackendSession {\n'''
new = '''namespace {\n\nstruct GpuCtrlFrameColor {\n\tfloat red = 0.0f;\n\tfloat green = 0.0f;\n\tfloat blue = 0.0f;\n\tfloat alpha = 1.0f;\n};\n\nstruct GpuCtrlFrameIntent {\n\tGpuCtrlFrameColor background;\n\tbool has_fill_rect = false;\n\tRect fill_rect = Rect(0, 0, 0, 0);\n\tGpuCtrlFrameColor fill_color;\n};\n\nstatic GpuCtrlFrameIntent BuildDefaultFrameIntent(Size size)\n{\n\tGpuCtrlFrameIntent frame;\n\tframe.background = { 0.08f, 0.24f, 0.58f, 1.0f };\n\tif(size.cx <= 0 || size.cy <= 0)\n\t\treturn frame;\n\n\tint rect_width = max(1, size.cx / 2);\n\tint rect_height = max(1, size.cy / 2);\n\tint rect_left = (size.cx - rect_width) / 2;\n\tint rect_top = (size.cy - rect_height) / 2;\n\tframe.has_fill_rect = true;\n\tframe.fill_rect = RectC(rect_left, rect_top, rect_width, rect_height);\n\tframe.fill_color = { 0.90f, 0.32f, 0.08f, 1.0f };\n\treturn frame;\n}\n\nclass GpuCtrlBackendSession {\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tvirtual const String& GetError() const = 0;\n\tvirtual bool Present(Size requested_size, String& error) = 0;\n'''
new = '''\tvirtual const String& GetError() const = 0;\n\tvirtual bool Present(Size requested_size, const GpuCtrlFrameIntent& frame, String& error) = 0;\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tbool Present(Size requested_size, String& error) override\n\t{\n\t\terror.Clear();\n\t\tif(requested_size.cx <= 0 || requested_size.cy <= 0)\n\t\t\treturn true;\n\t\tif(!session.IsReady()) {\n\t\t\terror = session.GetError();\n\t\t\treturn false;\n\t\t}\n\t\tif(!EnsureSwapchain(requested_size, error))\n\t\t\treturn false;\n\t\tint rect_width = requested_size.cx / 2;\n\t\tint rect_height = requested_size.cy / 2;\n\t\tif(rect_width < 1) rect_width = 1;\n\t\tif(rect_height < 1) rect_height = 1;\n\t\tint rect_left = (requested_size.cx - rect_width) / 2;\n\t\tint rect_top = (requested_size.cy - rect_height) / 2;\n\t\tRect rect = RectC(rect_left, rect_top, rect_width, rect_height);\n\t\tif(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, rect, 0.90f, 0.32f, 0.08f, 1.0f))\n\t\t\treturn true;\n'''
new = '''\tbool Present(Size requested_size, const GpuCtrlFrameIntent& frame, String& error) override\n\t{\n\t\terror.Clear();\n\t\tif(requested_size.cx <= 0 || requested_size.cy <= 0)\n\t\t\treturn true;\n\t\tif(!session.IsReady()) {\n\t\t\terror = session.GetError();\n\t\t\treturn false;\n\t\t}\n\t\tif(!EnsureSwapchain(requested_size, error))\n\t\t\treturn false;\n\t\tif(PresentIntent(frame))\n\t\t\treturn true;\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\t\tif(!EnsureSwapchain(requested_size, error))\n\t\t\treturn false;\n\t\tif(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, rect, 0.90f, 0.32f, 0.08f, 1.0f)) {\n\t\t\terror.Clear();\n\t\t\treturn true;\n\t\t}\n'''
new = '''\t\tif(!EnsureSwapchain(requested_size, error))\n\t\t\treturn false;\n\t\tif(PresentIntent(frame)) {\n\t\t\terror.Clear();\n\t\t\treturn true;\n\t\t}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''private:\n\tbool EnsureSwapchain(Size requested_size, String& error)\n'''
new = '''private:\n\tbool PresentIntent(const GpuCtrlFrameIntent& frame)\n\t{\n\t\tconst GpuCtrlFrameColor& bg = frame.background;\n\t\tif(!frame.has_fill_rect)\n\t\t\treturn session.PresentClearFrame(bg.red, bg.green, bg.blue, bg.alpha);\n\t\tconst GpuCtrlFrameColor& fill = frame.fill_color;\n\t\treturn session.PresentRectFrame(bg.red, bg.green, bg.blue, bg.alpha, frame.fill_rect,\n\t\t                                fill.red, fill.green, fill.blue, fill.alpha);\n\t}\n\n\tbool EnsureSwapchain(Size requested_size, String& error)\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\t\tSize requested_size = GetNativeHostSize();\n\t\tString error;\n\t\tif(backend->Present(requested_size, error)) {\n'''
new = '''\t\tSize requested_size = GetNativeHostSize();\n\t\tGpuCtrlFrameIntent frame = BuildDefaultFrameIntent(requested_size);\n\t\tString error;\n\t\tif(backend->Present(requested_size, frame, error)) {\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

p.write_text(s)

p = Path('docs/PROJECT_PLAN.md')
s = p.read_text()
old = '''- S15 drives private swapchain recreation and S14 clear presentation from native\n  paint invalidation without adding a timer/render loop or Vulkan public API\n- general 2D rendering, shaders, painter callbacks, and shared control device\n  ownership remain deferred\n'''
new = '''- S15 drives private swapchain recreation and S14 clear presentation from native\n  paint invalidation without adding a timer/render loop or Vulkan public API\n- S16A adds the first backend-private filled rectangle through the real GpuCtrl\n  presentation lifecycle; S16B moves its background/rectangle description into\n  backend-neutral private frame intent so the Vulkan backend no longer invents\n  control content or geometry\n- general 2D rendering, display-list replay, shaders, painter callbacks, and\n  shared control device ownership remain deferred\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)
