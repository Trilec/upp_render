from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    assert count == 1, f"{path}: expected one anchor, found {count}"
    p.write_text(text.replace(old, new, 1))


# RenderVulkanSurfaceSession.h: narrow rectangle entry point and shared implementation.
replace_once(
    "render/RenderVulkan/RenderVulkanSurfaceSession.h",
    "\tbool PresentClearFrame(float red, float green, float blue, float alpha = 1.0f);\n",
    "\tbool PresentClearFrame(float red, float green, float blue, float alpha = 1.0f);\n"
    "\tbool PresentRectFrame(float background_red, float background_green, float background_blue, float background_alpha,\n"
    "\t                      const Rect& rect, float red, float green, float blue, float alpha = 1.0f);\n",
)
replace_once(
    "render/RenderVulkan/RenderVulkanSurfaceSession.h",
    "\tbool GetFrameInterop(FrameInterop& out) const;\n",
    "\tbool PresentClearFrameImpl(float red, float green, float blue, float alpha,\n"
    "\t                           const Rect *rect, float rect_red, float rect_green, float rect_blue, float rect_alpha);\n"
    "\tbool GetFrameInterop(FrameInterop& out) const;\n",
)

# Reuse the accepted clear-frame path and optionally add one clear-attachment rectangle.
replace_once(
    "render/RenderVulkan/RenderVulkanClearFrame.cpp",
    "bool VulkanSurfaceSession::PresentClearFrame(float red, float green, float blue, float alpha)\n{\n\tFrameInterop interop;\n",
    "bool VulkanSurfaceSession::PresentClearFrame(float red, float green, float blue, float alpha)\n"
    "{\n"
    "\treturn PresentClearFrameImpl(red, green, blue, alpha, nullptr, 0.0f, 0.0f, 0.0f, 1.0f);\n"
    "}\n\n"
    "bool VulkanSurfaceSession::PresentRectFrame(float background_red, float background_green, float background_blue, float background_alpha,\n"
    "                                            const Rect& rect, float red, float green, float blue, float alpha)\n"
    "{\n"
    "\treturn PresentClearFrameImpl(background_red, background_green, background_blue, background_alpha, &rect, red, green, blue, alpha);\n"
    "}\n\n"
    "bool VulkanSurfaceSession::PresentClearFrameImpl(float red, float green, float blue, float alpha,\n"
    "                                                 const Rect *rect, float rect_red, float rect_green, float rect_blue, float rect_alpha)\n"
    "{\n"
    "\tFrameInterop interop;\n",
)
replace_once(
    "render/RenderVulkan/RenderVulkanClearFrame.cpp",
    "\tPFN_vkCmdBeginRendering cmd_begin_rendering = nullptr;\n\tPFN_vkCmdEndRendering cmd_end_rendering = nullptr;\n\tPFN_vkQueueSubmit2 queue_submit_2 = nullptr;\n",
    "\tPFN_vkCmdBeginRendering cmd_begin_rendering = nullptr;\n\tPFN_vkCmdEndRendering cmd_end_rendering = nullptr;\n\tPFN_vkCmdClearAttachments cmd_clear_attachments = nullptr;\n\tPFN_vkQueueSubmit2 queue_submit_2 = nullptr;\n",
)
replace_once(
    "render/RenderVulkan/RenderVulkanClearFrame.cpp",
    "\t   !ResolveClearProc(queue_submit_2, interop, \"vkQueueSubmit2\", error) ||\n"
    "\t   !ResolveClearProc(queue_present, interop, \"vkQueuePresentKHR\", error)) {\n"
    "\t\tframe_report.error = error;\n"
    "\t\treturn false;\n"
    "\t}\n\n"
    "\tVulkanClearResources resources;\n",
    "\t   !ResolveClearProc(queue_submit_2, interop, \"vkQueueSubmit2\", error) ||\n"
    "\t   !ResolveClearProc(queue_present, interop, \"vkQueuePresentKHR\", error)) {\n"
    "\t\tframe_report.error = error;\n"
    "\t\treturn false;\n"
    "\t}\n"
    "\tif(rect && !ResolveClearProc(cmd_clear_attachments, interop, \"vkCmdClearAttachments\", error)) {\n"
    "\t\tframe_report.error = error;\n"
    "\t\treturn false;\n"
    "\t}\n\n"
    "\tRect draw_rect;\n"
    "\tif(rect) {\n"
    "\t\tdraw_rect = *rect & Rect(0, 0, GetReport().swapchain_extent.cx, GetReport().swapchain_extent.cy);\n"
    "\t\tif(draw_rect.IsEmpty()) {\n"
    "\t\t\tframe_report.error = \"Vulkan rectangle is outside the swapchain extent\";\n"
    "\t\t\treturn false;\n"
    "\t\t}\n"
    "\t}\n\n"
    "\tVulkanClearResources resources;\n",
)
replace_once(
    "render/RenderVulkan/RenderVulkanClearFrame.cpp",
    "\t\tcmd_begin_rendering(resources.command_buffer, &rendering);\n\t\tcmd_end_rendering(resources.command_buffer);\n",
    "\t\tcmd_begin_rendering(resources.command_buffer, &rendering);\n"
    "\t\tif(rect) {\n"
    "\t\t\tVkClearAttachment rect_attachment{};\n"
    "\t\t\trect_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;\n"
    "\t\t\trect_attachment.colorAttachment = 0;\n"
    "\t\t\trect_attachment.clearValue.color.float32[0] = rect_red;\n"
    "\t\t\trect_attachment.clearValue.color.float32[1] = rect_green;\n"
    "\t\t\trect_attachment.clearValue.color.float32[2] = rect_blue;\n"
    "\t\t\trect_attachment.clearValue.color.float32[3] = rect_alpha;\n"
    "\t\t\tVkClearRect clear_rect{};\n"
    "\t\t\tclear_rect.rect.offset.x = draw_rect.left;\n"
    "\t\t\tclear_rect.rect.offset.y = draw_rect.top;\n"
    "\t\t\tclear_rect.rect.extent.width = (uint32_t)draw_rect.Width();\n"
    "\t\t\tclear_rect.rect.extent.height = (uint32_t)draw_rect.Height();\n"
    "\t\t\tclear_rect.baseArrayLayer = 0;\n"
    "\t\t\tclear_rect.layerCount = 1;\n"
    "\t\t\tcmd_clear_attachments(resources.command_buffer, 1, &rect_attachment, 1, &clear_rect);\n"
    "\t\t}\n"
    "\t\tcmd_end_rendering(resources.command_buffer);\n",
)

# GpuCtrl: central orange rectangle over the accepted S14 blue background.
replace_once(
    "render/GpuCtrl/GpuCtrl.cpp",
    "\t\tif(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f))\n\t\t\treturn true;\n",
    "\t\tint rect_width = requested_size.cx / 2;\n"
    "\t\tint rect_height = requested_size.cy / 2;\n"
    "\t\tif(rect_width < 1) rect_width = 1;\n"
    "\t\tif(rect_height < 1) rect_height = 1;\n"
    "\t\tint rect_left = (requested_size.cx - rect_width) / 2;\n"
    "\t\tint rect_top = (requested_size.cy - rect_height) / 2;\n"
    "\t\tRect rect = RectC(rect_left, rect_top, rect_width, rect_height);\n"
    "\t\tif(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, rect, 0.90f, 0.32f, 0.08f, 1.0f))\n"
    "\t\t\treturn true;\n",
)
replace_once(
    "render/GpuCtrl/GpuCtrl.cpp",
    "\t\tif(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f)) {\n\t\t\terror.Clear();\n\t\t\treturn true;\n\t\t}\n",
    "\t\tif(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, rect, 0.90f, 0.32f, 0.08f, 1.0f)) {\n"
    "\t\t\terror.Clear();\n"
    "\t\t\treturn true;\n"
    "\t\t}\n",
)

# Focused backend test: rectangle success, exact counters, and procedure isolation.
replace_once(
    "tests/RenderVulkanClearFrameTest/main.cpp",
    "\tconst float colors[][4] = {\n",
    "\tif(ok) {\n"
    "\t\tok &= Check(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, Rect(24, 16, 72, 48), 0.90f, 0.32f, 0.08f, 1.0f), \"rectangle frame should present\");\n"
    "\t\tok &= Check(session.GetFrameReport().clear_count == 2 && session.GetFrameReport().present_count == 2, \"rectangle frame should share the clear/present lifecycle\");\n"
    "\t\tok &= Check(session.GetFrameReport().state_cleared && !session.HasAcquiredFrame(), \"rectangle frame should leave no acquired private frame state\");\n"
    "\t}\n\n"
    "\tconst float colors[][4] = {\n",
)
replace_once(
    "tests/RenderVulkanClearFrameTest/main.cpp",
    "\t\tok &= Check(session.GetFrameReport().clear_count == 5 && session.GetFrameReport().present_count == 5, \"repeated clear/present counters should remain exact\");\n",
    "\t\tok &= Check(session.GetFrameReport().clear_count == 6 && session.GetFrameReport().present_count == 6, \"repeated clear/present counters should remain exact\");\n",
)
replace_once(
    "tests/RenderVulkanClearFrameTest/main.cpp",
    "\tg_missing_proc = nullptr;\n\n\tif(ok) {\n\t\tok &= Check(session.GetReport().validation_warning_count == 0, \"clear-frame validation warnings should be zero\");\n",
    "\tg_missing_proc = nullptr;\n"
    "\tif(ok) {\n"
    "\t\tg_missing_proc = \"vkCmdClearAttachments\";\n"
    "\t\tok &= Check(!session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, Rect(24, 16, 72, 48), 0.90f, 0.32f, 0.08f, 1.0f), \"missing rectangle procedure should refuse only the rectangle path\");\n"
    "\t\tok &= Check(session.GetFrameReport().error == \"vkCmdClearAttachments\", \"missing rectangle procedure should report its exact name\");\n"
    "\t\tok &= Check(!session.HasAcquiredFrame(), \"missing rectangle procedure should fail before image acquisition\");\n"
    "\t\tg_missing_proc = nullptr;\n"
    "\t\tok &= Check(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f), \"clear-only path should remain independent of rectangle procedure\");\n"
    "\t}\n"
    "\tg_missing_proc = nullptr;\n\n"
    "\tif(ok) {\n\t\tok &= Check(session.GetReport().validation_warning_count == 0, \"clear-frame validation warnings should be zero\");\n",
)
