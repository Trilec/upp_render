from pathlib import Path

p = Path('render/GpuCtrl/GpuCtrl.cpp')
s = p.read_text()

old = '''struct GpuCtrlFrameIntent {\n\tGpuCtrlFrameColor background;\n\tbool has_fill_rect = false;\n\tRect fill_rect = Rect(0, 0, 0, 0);\n\tGpuCtrlFrameColor fill_color;\n};\n'''
new = '''struct GpuCtrlFillRectIntent : Moveable<GpuCtrlFillRectIntent> {\n\tRect rect = Rect(0, 0, 0, 0);\n\tGpuCtrlFrameColor color;\n};\n\nstruct GpuCtrlFrameIntent {\n\tGpuCtrlFrameColor background;\n\tVector<GpuCtrlFillRectIntent> fill_rects;\n};\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''static bool ReplaySingleFillRect(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)\n{\n\tif(!list.IsValid()) {\n\t\terror = list.GetError();\n\t\treturn false;\n\t}\n\tif(list.GetCount() != 1 || list[0].type != UiDisplayOpType::FillRect) {\n\t\terror = "GpuCtrl S16C frame requires exactly one FillRect display operation";\n\t\treturn false;\n\t}\n\n\tconst UiDisplayOp& op = list[0];\n\tint left = (int)op.rect.left;\n\tint top = (int)op.rect.top;\n\tint right = (int)op.rect.right;\n\tint bottom = (int)op.rect.bottom;\n\tif(right <= left || bottom <= top) {\n\t\terror = "GpuCtrl S16C FillRect display operation is empty";\n\t\treturn false;\n\t}\n\n\tframe.has_fill_rect = true;\n\tframe.fill_rect = Rect(left, top, right, bottom);\n\tframe.fill_color = ToFrameColor(op.color);\n\terror.Clear();\n\treturn true;\n}\n'''
new = '''static bool ReplayFillRects(const UiDisplayList& list, GpuCtrlFrameIntent& frame, String& error)\n{\n\tif(!list.IsValid()) {\n\t\terror = list.GetError();\n\t\treturn false;\n\t}\n\tif(list.GetCount() <= 0) {\n\t\terror = "GpuCtrl S16D frame requires at least one FillRect display operation";\n\t\treturn false;\n\t}\n\n\tframe.fill_rects.Clear();\n\tframe.fill_rects.Reserve(list.GetCount());\n\tfor(int i = 0; i < list.GetCount(); ++i) {\n\t\tconst UiDisplayOp& op = list[i];\n\t\tif(op.type != UiDisplayOpType::FillRect) {\n\t\t\terror = "GpuCtrl S16D frame supports FillRect display operations only";\n\t\t\tframe.fill_rects.Clear();\n\t\t\treturn false;\n\t\t}\n\n\t\tint left = (int)op.rect.left;\n\t\tint top = (int)op.rect.top;\n\t\tint right = (int)op.rect.right;\n\t\tint bottom = (int)op.rect.bottom;\n\t\tif(right <= left || bottom <= top) {\n\t\t\terror = "GpuCtrl S16D FillRect display operation is empty";\n\t\t\tframe.fill_rects.Clear();\n\t\t\treturn false;\n\t\t}\n\n\t\tGpuCtrlFillRectIntent& fill = frame.fill_rects.Add();\n\t\tfill.rect = Rect(left, top, right, bottom);\n\t\tfill.color = ToFrameColor(op.color);\n\t}\n\terror.Clear();\n\treturn true;\n}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tUiDisplayListBuilder builder;\n\tbuilder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),\n\t                 Rgba8(230, 82, 20, 255));\n\tUiDisplayList list;\n\tif(!builder.Finish(list)) {\n\t\terror = builder.GetError();\n\t\treturn false;\n\t}\n\treturn ReplaySingleFillRect(list, frame, error);\n}\n'''
new = '''\tint inner_width = rect_width / 2;\n\tint inner_height = rect_height / 2;\n\tif(inner_width < 1)\n\t\tinner_width = 1;\n\tif(inner_height < 1)\n\t\tinner_height = 1;\n\tint inner_left = (size.cx - inner_width) / 2;\n\tint inner_top = (size.cy - inner_height) / 2;\n\n\tUiDisplayListBuilder builder;\n\tbuilder.FillRect(Rectf(rect_left, rect_top, rect_left + rect_width, rect_top + rect_height),\n\t                 Rgba8(230, 82, 20, 255));\n\tbuilder.FillRect(Rectf(inner_left, inner_top, inner_left + inner_width, inner_top + inner_height),\n\t                 Rgba8(36, 190, 110, 255));\n\tUiDisplayList list;\n\tif(!builder.Finish(list)) {\n\t\terror = builder.GetError();\n\t\treturn false;\n\t}\n\treturn ReplayFillRects(list, frame, error);\n}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tbool PresentIntent(const GpuCtrlFrameIntent& frame)\n\t{\n\t\tconst GpuCtrlFrameColor& bg = frame.background;\n\t\tif(!frame.has_fill_rect)\n\t\t\treturn session.PresentClearFrame(bg.red, bg.green, bg.blue, bg.alpha);\n\t\tconst GpuCtrlFrameColor& fill = frame.fill_color;\n\t\treturn session.PresentRectFrame(bg.red, bg.green, bg.blue, bg.alpha, frame.fill_rect,\n\t\t                                fill.red, fill.green, fill.blue, fill.alpha);\n\t}\n'''
new = '''\tbool PresentIntent(const GpuCtrlFrameIntent& frame)\n\t{\n\t\tconst GpuCtrlFrameColor& bg = frame.background;\n\t\tif(frame.fill_rects.IsEmpty())\n\t\t\treturn session.PresentClearFrame(bg.red, bg.green, bg.blue, bg.alpha);\n\n\t\tVector<VulkanFrameRect> rects;\n\t\trects.Reserve(frame.fill_rects.GetCount());\n\t\tfor(const GpuCtrlFillRectIntent& fill : frame.fill_rects) {\n\t\t\tVulkanFrameRect& out = rects.Add();\n\t\t\tout.rect = fill.rect;\n\t\t\tout.red = fill.color.red;\n\t\t\tout.green = fill.color.green;\n\t\t\tout.blue = fill.color.blue;\n\t\t\tout.alpha = fill.color.alpha;\n\t\t}\n\t\treturn session.PresentRectsFrame(bg.red, bg.green, bg.blue, bg.alpha, rects);\n\t}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)

p = Path('render/RenderVulkan/RenderVulkanSurfaceSession.h')
s = p.read_text()
old = '''class VulkanSurfaceSession {\n'''
new = '''struct VulkanFrameRect : Moveable<VulkanFrameRect> {\n\tRect rect = Rect(0, 0, 0, 0);\n\tfloat red = 0.0f;\n\tfloat green = 0.0f;\n\tfloat blue = 0.0f;\n\tfloat alpha = 1.0f;\n};\n\nclass VulkanSurfaceSession {\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tbool PresentRectFrame(float background_red, float background_green, float background_blue, float background_alpha,\n\t                      const Rect& rect, float red, float green, float blue, float alpha = 1.0f);\n'''
new = '''\tbool PresentRectFrame(float background_red, float background_green, float background_blue, float background_alpha,\n\t                      const Rect& rect, float red, float green, float blue, float alpha = 1.0f);\n\tbool PresentRectsFrame(float background_red, float background_green, float background_blue, float background_alpha,\n\t                       const Vector<VulkanFrameRect>& rects);\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tbool PresentClearFrameImpl(float red, float green, float blue, float alpha,\n\t                           const Rect *rect, float rect_red, float rect_green, float rect_blue, float rect_alpha);\n'''
new = '''\tbool PresentClearFrameImpl(float red, float green, float blue, float alpha,\n\t                           const Vector<VulkanFrameRect> *rects);\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)

p = Path('render/RenderVulkan/RenderVulkanClearFrame.cpp')
s = p.read_text()
old = '''bool VulkanSurfaceSession::PresentClearFrame(float red, float green, float blue, float alpha)\n{\n\treturn PresentClearFrameImpl(red, green, blue, alpha, nullptr, 0.0f, 0.0f, 0.0f, 1.0f);\n}\n\nbool VulkanSurfaceSession::PresentRectFrame(float background_red, float background_green, float background_blue, float background_alpha,\n                                            const Rect& rect, float red, float green, float blue, float alpha)\n{\n\treturn PresentClearFrameImpl(background_red, background_green, background_blue, background_alpha, &rect, red, green, blue, alpha);\n}\n\nbool VulkanSurfaceSession::PresentClearFrameImpl(float red, float green, float blue, float alpha,\n                                                 const Rect *rect, float rect_red, float rect_green, float rect_blue, float rect_alpha)\n{\n'''
new = '''bool VulkanSurfaceSession::PresentClearFrame(float red, float green, float blue, float alpha)\n{\n\treturn PresentClearFrameImpl(red, green, blue, alpha, nullptr);\n}\n\nbool VulkanSurfaceSession::PresentRectFrame(float background_red, float background_green, float background_blue, float background_alpha,\n                                            const Rect& rect, float red, float green, float blue, float alpha)\n{\n\tVector<VulkanFrameRect> rects;\n\tVulkanFrameRect& fill = rects.Add();\n\tfill.rect = rect;\n\tfill.red = red;\n\tfill.green = green;\n\tfill.blue = blue;\n\tfill.alpha = alpha;\n\treturn PresentClearFrameImpl(background_red, background_green, background_blue, background_alpha, &rects);\n}\n\nbool VulkanSurfaceSession::PresentRectsFrame(float background_red, float background_green, float background_blue, float background_alpha,\n                                             const Vector<VulkanFrameRect>& rects)\n{\n\treturn PresentClearFrameImpl(background_red, background_green, background_blue, background_alpha, &rects);\n}\n\nbool VulkanSurfaceSession::PresentClearFrameImpl(float red, float green, float blue, float alpha,\n                                                 const Vector<VulkanFrameRect> *rects)\n{\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\tif(rect && !ResolveClearProc(cmd_clear_attachments, interop, "vkCmdClearAttachments", error)) {\n\t\tframe_report.error = error;\n\t\treturn false;\n\t}\n\n\tRect draw_rect;\n\tif(rect) {\n\t\tdraw_rect = *rect & Rect(0, 0, GetReport().swapchain_extent.cx, GetReport().swapchain_extent.cy);\n\t\tif(draw_rect.IsEmpty()) {\n\t\t\tframe_report.error = "Vulkan rectangle is outside the swapchain extent";\n\t\t\treturn false;\n\t\t}\n\t}\n'''
new = '''\tif(rects && !rects->IsEmpty() && !ResolveClearProc(cmd_clear_attachments, interop, "vkCmdClearAttachments", error)) {\n\t\tframe_report.error = error;\n\t\treturn false;\n\t}\n\n\tVector<VulkanFrameRect> draw_rects;\n\tif(rects) {\n\t\tdraw_rects = clone(*rects);\n\t\tRect extent_rect(0, 0, GetReport().swapchain_extent.cx, GetReport().swapchain_extent.cy);\n\t\tfor(VulkanFrameRect& fill : draw_rects) {\n\t\t\tfill.rect = fill.rect & extent_rect;\n\t\t\tif(fill.rect.IsEmpty()) {\n\t\t\t\tframe_report.error = "Vulkan rectangle is outside the swapchain extent";\n\t\t\t\treturn false;\n\t\t\t}\n\t\t}\n\t}\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)

old = '''\t\tcmd_begin_rendering(resources.command_buffer, &rendering);\n\t\tif(rect) {\n\t\t\tVkClearAttachment rect_attachment{};\n\t\t\trect_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;\n\t\t\trect_attachment.colorAttachment = 0;\n\t\t\trect_attachment.clearValue.color.float32[0] = rect_red;\n\t\t\trect_attachment.clearValue.color.float32[1] = rect_green;\n\t\t\trect_attachment.clearValue.color.float32[2] = rect_blue;\n\t\t\trect_attachment.clearValue.color.float32[3] = rect_alpha;\n\t\t\tVkClearRect clear_rect{};\n\t\t\tclear_rect.rect.offset.x = draw_rect.left;\n\t\t\tclear_rect.rect.offset.y = draw_rect.top;\n\t\t\tclear_rect.rect.extent.width = (uint32_t)draw_rect.Width();\n\t\t\tclear_rect.rect.extent.height = (uint32_t)draw_rect.Height();\n\t\t\tclear_rect.baseArrayLayer = 0;\n\t\t\tclear_rect.layerCount = 1;\n\t\t\tcmd_clear_attachments(resources.command_buffer, 1, &rect_attachment, 1, &clear_rect);\n\t\t}\n\t\tcmd_end_rendering(resources.command_buffer);\n'''
new = '''\t\tcmd_begin_rendering(resources.command_buffer, &rendering);\n\t\tfor(const VulkanFrameRect& fill : draw_rects) {\n\t\t\tVkClearAttachment rect_attachment{};\n\t\t\trect_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;\n\t\t\trect_attachment.colorAttachment = 0;\n\t\t\trect_attachment.clearValue.color.float32[0] = fill.red;\n\t\t\trect_attachment.clearValue.color.float32[1] = fill.green;\n\t\t\trect_attachment.clearValue.color.float32[2] = fill.blue;\n\t\t\trect_attachment.clearValue.color.float32[3] = fill.alpha;\n\t\t\tVkClearRect clear_rect{};\n\t\t\tclear_rect.rect.offset.x = fill.rect.left;\n\t\t\tclear_rect.rect.offset.y = fill.rect.top;\n\t\t\tclear_rect.rect.extent.width = (uint32_t)fill.rect.Width();\n\t\t\tclear_rect.rect.extent.height = (uint32_t)fill.rect.Height();\n\t\t\tclear_rect.baseArrayLayer = 0;\n\t\t\tclear_rect.layerCount = 1;\n\t\t\tcmd_clear_attachments(resources.command_buffer, 1, &rect_attachment, 1, &clear_rect);\n\t\t}\n\t\tcmd_end_rendering(resources.command_buffer);\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)

p = Path('tests/RenderVulkanClearFrameTest/main.cpp')
s = p.read_text()
old = '''\tif(ok) {\n\t\tok &= Check(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, Rect(24, 16, 72, 48), 0.90f, 0.32f, 0.08f, 1.0f), "rectangle frame should present");\n\t\tok &= Check(session.GetFrameReport().clear_count == 2 && session.GetFrameReport().present_count == 2, "rectangle frame should share the clear/present lifecycle");\n\t\tok &= Check(session.GetFrameReport().state_cleared && !session.HasAcquiredFrame(), "rectangle frame should leave no acquired private frame state");\n\t}\n\n\tconst float colors[][4] = {\n'''
new = '''\tif(ok) {\n\t\tok &= Check(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, Rect(24, 16, 72, 48), 0.90f, 0.32f, 0.08f, 1.0f), "rectangle frame should present");\n\t\tok &= Check(session.GetFrameReport().clear_count == 2 && session.GetFrameReport().present_count == 2, "rectangle frame should share the clear/present lifecycle");\n\t\tok &= Check(session.GetFrameReport().state_cleared && !session.HasAcquiredFrame(), "rectangle frame should leave no acquired private frame state");\n\t}\n\n\tif(ok) {\n\t\tVector<VulkanFrameRect> rects;\n\t\tVulkanFrameRect& outer = rects.Add();\n\t\touter.rect = Rect(24, 16, 72, 48);\n\t\touter.red = 0.90f; outer.green = 0.32f; outer.blue = 0.08f; outer.alpha = 1.0f;\n\t\tVulkanFrameRect& inner = rects.Add();\n\t\tinner.rect = Rect(36, 24, 60, 40);\n\t\tinner.red = 0.14f; inner.green = 0.75f; inner.blue = 0.43f; inner.alpha = 1.0f;\n\t\tok &= Check(session.PresentRectsFrame(0.08f, 0.24f, 0.58f, 1.0f, rects), "ordered rectangle-list frame should present");\n\t\tok &= Check(session.GetFrameReport().clear_count == 3 && session.GetFrameReport().present_count == 3, "rectangle-list frame should remain one clear/present lifecycle");\n\t\tok &= Check(session.GetFrameReport().state_cleared && !session.HasAcquiredFrame(), "rectangle-list frame should leave no acquired private frame state");\n\t}\n\n\tconst float colors[][4] = {\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
old_count = 'session.GetFrameReport().clear_count == 6 && session.GetFrameReport().present_count == 6'
assert s.count(old_count) == 1
s = s.replace(old_count, 'session.GetFrameReport().clear_count == 7 && session.GetFrameReport().present_count == 7', 1)
p.write_text(s)

p = Path('docs/PROJECT_PLAN.md')
s = p.read_text()
old = '''- S16C records the orange rectangle as one existing UiDisplayList FillRect and\n  replays that neutral operation into the private frame intent; broader display-\n  list replay and renderer state remain deferred\n- general 2D rendering, shaders, painter callbacks, and shared control device\n  ownership remain deferred\n'''
new = '''- S16C records the orange rectangle as one existing UiDisplayList FillRect and\n  replays that neutral operation into the private frame intent\n- S16D extends the FillRect-only replay proof to ordered operations and carries\n  two fills through one Vulkan dynamic-rendering frame; transforms, clipping and\n  general renderer state remain deferred\n- general 2D rendering, shaders, painter callbacks, and shared control device\n  ownership remain deferred\n'''
assert s.count(old) == 1
s = s.replace(old, new, 1)
p.write_text(s)
