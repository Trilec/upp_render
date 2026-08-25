#include <RenderVulkan/RenderVulkan.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

using namespace Upp;
using namespace Upp::VulkanTestHooks;

namespace {

const char *missing_proc = nullptr;

FARPROC WINAPI TestResolver(HMODULE module, LPCSTR name)
{
	(void)module;
	if(missing_proc && String(name) == missing_proc)
		return nullptr;
	return reinterpret_cast<FARPROC>(1);
}

bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

bool ZeroLive(const VulkanRuntimeDeviceDiagnostics& diag)
{
	return diag.runtime_live_count == 0 &&
	       diag.instance_live_count == 0 &&
	       diag.debug_messenger_live_count == 0 &&
	       diag.surface_live_count == 0 &&
	       diag.device_live_count == 0 &&
	       diag.swapchain_live_count == 0;
}

bool TestSuccessfulFailureCleanup()
{
	ClearVulkanValidationTestInjection();
	missing_proc = "vkGetDeviceQueue";
	VulkanSurfaceSessionAccountingResult result;
	bool ran = TestVulkanSurfaceSessionPostCreateFailure(true, &TestResolver, result);
	missing_proc = nullptr;
	if(!Check(ran, "post-create failure hook should complete")) return false;
	if(!Check(result.report.status == VulkanProbeStatus::DeviceCreationFailed,
	          "post-create failure should report DeviceCreationFailed")) return false;
	if(!Check(result.report.device_error == "vkGetDeviceQueue",
	          "post-create failure should preserve the missing procedure")) return false;
	if(!Check(result.report.device_cleanup_ok,
	          "successful post-create cleanup should report device cleanup ok")) return false;
	if(!Check(result.report.clean_shutdown,
	          "successful post-create cleanup should report a clean shutdown")) return false;
	if(!Check(result.report.cleanup_state_cleared,
	          "successful post-create cleanup should clear session state")) return false;
	if(!Check(ZeroLive(result.close_diag),
	          "successful post-create cleanup should leave zero live Vulkan ownership")) return false;
	return Check(ZeroLive(result.repeat_close_diag),
	             "repeat close after successful post-create cleanup should remain empty");
}

bool TestFailedFailureCleanup()
{
	VulkanValidationTestInjection injection;
	injection.enabled = true;
	injection.point = VulkanValidationTestPoint::DuringDeviceCleanup;
	injection.force_device_cleanup_failure = true;
	injection.device_cleanup_result = VK_ERROR_DEVICE_LOST;
	SetVulkanValidationTestInjection(injection);
	missing_proc = "vkGetDeviceQueue";
	VulkanSurfaceSessionAccountingResult result;
	bool ran = TestVulkanSurfaceSessionPostCreateFailure(true, &TestResolver, result);
	missing_proc = nullptr;
	ClearVulkanValidationTestInjection();
	if(!Check(ran, "post-create cleanup-failure hook should complete")) return false;
	if(!Check(result.report.status == VulkanProbeStatus::DeviceCreationFailed,
	          "cleanup failure must not replace the primary device-creation failure")) return false;
	if(!Check(result.report.device_error == "vkGetDeviceQueue",
	          "cleanup failure should preserve the primary missing procedure")) return false;
	if(!Check(!result.report.device_cleanup_ok,
	          "injected failed cleanup must remain visible in the report")) return false;
	if(!Check(!result.report.clean_shutdown,
	          "injected failed cleanup must mark shutdown unclean")) return false;
	if(!Check(result.report.cleanup_state_cleared,
	          "failed cleanup reporting should still clear owned Vulkan state")) return false;
	if(!Check(ZeroLive(result.close_diag),
	          "failed cleanup reporting should still leave zero live Vulkan ownership")) return false;
	return Check(ZeroLive(result.repeat_close_diag),
	             "repeat close after failed cleanup reporting should remain empty");
}

}

CONSOLE_APP_MAIN
{
	bool ok = TestSuccessfulFailureCleanup();
	ok = TestFailedFailureCleanup() && ok;
	ClearVulkanValidationTestInjection();
	missing_proc = nullptr;
	if(ok) {
		Cout() << "RenderVulkanPostCreateCleanupTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
