#include <RenderVulkan/RenderVulkan.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

using namespace Upp;
using Upp::VulkanTestHooks::TestVulkanClearFrame;
using Upp::VulkanTestHooks::VulkanClearFrameTestResult;

static FARPROC WINAPI TestResolver(HMODULE module, LPCSTR name)
{
	(void)module;
	(void)name;
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
	VulkanClearFrameTestResult result;
	bool ok = TestVulkanClearFrame(&TestResolver, result);
	ok &= Check(result.clear_present, "clear-colour frame should submit and present");
	ok &= Check(result.repeat_clear, "repeated clear-colour frames should remain valid");
	ok &= Check(result.missing_clear_procedure_recovered, "missing clear-frame procedures should refuse cleanly and recover");
	ok &= Check(result.validation_clean, "clear-colour presentation should remain validation-clean");
	ok &= Check(result.final_diag.runtime_live_count == 0 &&
	            result.final_diag.instance_live_count == 0 &&
	            result.final_diag.debug_messenger_live_count == 0 &&
	            result.final_diag.surface_live_count == 0 &&
	            result.final_diag.device_live_count == 0 &&
	            result.final_diag.swapchain_live_count == 0,
	            "clear-colour test should finish with zero live Vulkan ownership diagnostics");
	if(ok) {
		Cout() << "RenderVulkanClearFrameTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
