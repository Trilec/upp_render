#include <RenderVulkan/RenderVulkan.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Upp;
using namespace Upp::VulkanTestHooks;

static const char *g_missing_proc = nullptr;

static FARPROC WINAPI TestResolver(HMODULE module, LPCSTR name)
{
	(void)module;
	if(g_missing_proc && String(name) == g_missing_proc)
		return nullptr;
	return reinterpret_cast<FARPROC>(1);
}

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static HWND CreateHiddenWindow()
{
	return CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 96, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

CONSOLE_APP_MAIN
{
	ClearVulkanRuntimeDeviceDiagnostics();
	HWND hwnd = CreateHiddenWindow();
	bool ok = Check(hwnd != nullptr, "hidden test window should be created");
	if(!ok) {
		SetExitCode(1);
		return;
	}
	struct WindowGuard { HWND hwnd; ~WindowGuard() { if(hwnd) DestroyWindow(hwnd); } } window{hwnd};

	GpuNativeWindowDesc native_window;
	native_window.kind = GpuNativeWindowKind::Win32;
	native_window.handle = (uint64_t)(uintptr_t)hwnd;

	VulkanSurfaceSession session;
	ok &= Check(session.Open(true, native_window, &TestResolver), "clear-frame session should open");
	ok &= Check(session.IsReady(), "clear-frame session should be ready");
	ok &= Check(session.CreateSwapchain(Size(96, 64)), "clear-frame swapchain should be created");

	if(ok) {
		ok &= Check(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f), "first clear-colour frame should present");
		const VulkanFrameReport& first = session.GetFrameReport();
		ok &= Check(first.clear_requested && first.cleared && first.presented, "first clear report should be authoritative");
		ok &= Check(first.clear_count == 1 && first.present_count == 1, "first clear/present counters should increment");
		ok &= Check(first.clear_red == 0.08f && first.clear_green == 0.24f && first.clear_blue == 0.58f && first.clear_alpha == 1.0f, "clear colour evidence should match the requested blue");
		ok &= Check(first.state_cleared && !session.HasAcquiredFrame(), "clear frame should leave no acquired private frame state");
	}

	if(ok) {
		ok &= Check(session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, Rect(24, 16, 72, 48), 0.90f, 0.32f, 0.08f, 1.0f), "rectangle frame should present");
		ok &= Check(session.GetFrameReport().clear_count == 2 && session.GetFrameReport().present_count == 2, "rectangle frame should share the clear/present lifecycle");
		ok &= Check(session.GetFrameReport().state_cleared && !session.HasAcquiredFrame(), "rectangle frame should leave no acquired private frame state");
	}

	if(ok) {
		Vector<VulkanFrameRect> rects;
		VulkanFrameRect& outer = rects.Add();
		outer.rect = Rect(24, 16, 72, 48);
		outer.red = 0.90f; outer.green = 0.32f; outer.blue = 0.08f; outer.alpha = 1.0f;
		VulkanFrameRect& inner = rects.Add();
		inner.rect = Rect(36, 24, 60, 40);
		inner.red = 0.14f; inner.green = 0.75f; inner.blue = 0.43f; inner.alpha = 1.0f;
		ok &= Check(session.PresentRectsFrame(0.08f, 0.24f, 0.58f, 1.0f, rects), "ordered rectangle-list frame should present");
		ok &= Check(session.GetFrameReport().clear_count == 3 && session.GetFrameReport().present_count == 3, "rectangle-list frame should remain one clear/present lifecycle");
		ok &= Check(session.GetFrameReport().state_cleared && !session.HasAcquiredFrame(), "rectangle-list frame should leave no acquired private frame state");
	}

	const float colors[][4] = {
		{ 0.65f, 0.10f, 0.12f, 1.0f },
		{ 0.08f, 0.55f, 0.22f, 1.0f },
		{ 0.10f, 0.22f, 0.70f, 1.0f },
		{ 0.55f, 0.18f, 0.62f, 1.0f },
	};
	for(const auto& c : colors)
		if(ok)
			ok &= Check(session.PresentClearFrame(c[0], c[1], c[2], c[3]), "repeated clear-colour frame should present");
	if(ok)
		ok &= Check(session.GetFrameReport().clear_count == 7 && session.GetFrameReport().present_count == 7, "repeated clear/present counters should remain exact");

	const char *missing[] = { "vkCreateImageView", "vkDestroyImageView", "vkCmdBeginRendering", "vkCmdEndRendering" };
	for(const char *name : missing) {
		if(!ok)
			break;
		g_missing_proc = name;
		ok &= Check(!session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f), "missing clear procedure should refuse");
		ok &= Check(session.GetFrameReport().error == name, "missing clear procedure should report its exact name");
		ok &= Check(!session.HasAcquiredFrame(), "missing clear procedure should not acquire an image");
		g_missing_proc = nullptr;
		ok &= Check(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f), "same session should recover after missing clear procedure");
	}
	g_missing_proc = nullptr;
	if(ok) {
		g_missing_proc = "vkCmdClearAttachments";
		ok &= Check(!session.PresentRectFrame(0.08f, 0.24f, 0.58f, 1.0f, Rect(24, 16, 72, 48), 0.90f, 0.32f, 0.08f, 1.0f), "missing rectangle procedure should refuse only the rectangle path");
		ok &= Check(session.GetFrameReport().error == "vkCmdClearAttachments", "missing rectangle procedure should report its exact name");
		ok &= Check(!session.HasAcquiredFrame(), "missing rectangle procedure should fail before image acquisition");
		g_missing_proc = nullptr;
		ok &= Check(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f), "clear-only path should remain independent of rectangle procedure");
	}
	g_missing_proc = nullptr;

	if(ok) {
		ok &= Check(session.GetReport().validation_warning_count == 0, "clear-frame validation warnings should be zero");
		ok &= Check(session.GetReport().validation_error_count == 0, "clear-frame validation errors should be zero");
	}

	session.Close();
	VulkanRuntimeDeviceDiagnostics diag = GetVulkanRuntimeDeviceDiagnostics();
	ok &= Check(diag.runtime_live_count == 0 && diag.instance_live_count == 0 && diag.debug_messenger_live_count == 0 &&
	            diag.surface_live_count == 0 && diag.device_live_count == 0 && diag.swapchain_live_count == 0,
	            "clear-frame test should finish with zero live Vulkan ownership diagnostics");

	if(ok) {
		Cout() << "RenderVulkanClearFrameTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
