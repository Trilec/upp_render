#include <RenderVulkan/RenderVulkan.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

using namespace Upp;
using Upp::VulkanTestHooks::TestVulkanFramePresentation;
using Upp::VulkanTestHooks::VulkanFrameTestResult;

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

CONSOLE_APP_MAIN
{
	VulkanFrameTestResult result;
	bool ok = TestVulkanFramePresentation(&TestResolver, result);
	ok &= Check(result.no_swapchain_refused, "frame acquisition without swapchain should be refused");
	ok &= Check(result.acquire_present, "frame acquire and present should succeed");
	ok &= Check(result.repeat_present, "frame synchronization should support repeated presentation");
	ok &= Check(result.duplicate_acquire_refused, "duplicate frame acquisition should be refused");
	ok &= Check(result.missing_procedure_recovered, "missing frame procedures should recover on the same session");
	ok &= Check(result.acquire_suboptimal, "suboptimal acquisition should remain successful and explicit");
	ok &= Check(result.acquire_out_of_date, "acquire out-of-date state should be authoritative");
	ok &= Check(result.present_suboptimal, "suboptimal presentation should remain successful and explicit");
	ok &= Check(result.present_out_of_date, "present out-of-date state should require recreation");
	ok &= Check(result.submit_failure_recovered, "submit failure should clear frame synchronization and recover");
	ok &= Check(result.destroy_with_acquired_cleanup, "swapchain destruction should clear an acquired frame");
	ok &= Check(result.close_with_acquired_cleanup, "session close should clear an acquired frame without validation errors");
	ok &= Check(result.destructor_cleanup, "frame resources should be released by session destruction");
	ok &= Check(result.grouped_isolation, "grouped sessions should keep frame state isolated");
	ok &= Check(result.validation_clean, "frame acquisition and presentation should remain validation-clean");
	ok &= Check(result.final_diag.runtime_live_count == 0 &&
	            result.final_diag.instance_live_count == 0 &&
	            result.final_diag.debug_messenger_live_count == 0 &&
	            result.final_diag.surface_live_count == 0 &&
	            result.final_diag.device_live_count == 0 &&
	            result.final_diag.swapchain_live_count == 0,
	            "frame presentation tests should finish with zero live Vulkan ownership diagnostics");
	if(ok) {
		Cout() << "RenderVulkanFrameTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
