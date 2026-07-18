#include <RenderVulkan/RenderVulkan.h>

using namespace Upp;

static const char *g_missing_proc = nullptr;

static FARPROC WINAPI TestResolver(HMODULE module, LPCSTR name)
{
	(void)module;
	if(g_missing_proc && String(name) == g_missing_proc)
		return nullptr;
	return reinterpret_cast<FARPROC>(1);
}

static bool Check(bool cond, const char *msg)
{
	if(!cond)
		Cout() << "FAIL: " << msg << EOL;
	return cond;
}

static VulkanPreflightReport RunPreflight(bool validation, const char *missing_proc = nullptr)
{
	g_missing_proc = missing_proc;
	VulkanPreflight probe;
	VulkanPreflightReport report = probe.Run(validation, &TestResolver);
	g_missing_proc = nullptr;
	return report;
}

static VulkanBootstrapReport RunBootstrap(bool validation, bool create_device, const char *missing_proc = nullptr)
{
	g_missing_proc = missing_proc;
	VulkanBootstrap probe;
	VulkanBootstrapReport report = probe.Run(validation, create_device, &TestResolver);
	g_missing_proc = nullptr;
	return report;
}

static bool TestPreflight(bool validation)
{
	VulkanPreflightReport report = RunPreflight(validation);
	if(!Check(report.loader_available, "loader should be available")) return false;
	if(!Check(report.instance_created, "instance should be created")) return false;
	if(!Check(!report.devices.IsEmpty(), "physical devices should be enumerated")) return false;
	if(!Check(report.suitable_device_count > 0, "at least one suitable device expected")) return false;
	if(!Check(report.cleanup_state_cleared, "preflight should clear internal state")) return false;
	if(!Check(report.clean_shutdown, "preflight should cleanly shut down")) return false;
	if(!Check(report.status == VulkanProbeStatus::Ok, "preflight should pass")) return false;
	if(validation) {
		if(!Check(report.validation_available, "validation should be available")) return false;
		if(!Check(report.debug_utils_available, "debug utils should be available")) return false;
		if(!Check(report.validation_warning_count == 0, "validation warnings should be zero")) return false;
		if(!Check(report.validation_error_count == 0, "validation errors should be zero")) return false;
		if(!Check(report.validation_messages.IsEmpty(), "validation messages should be empty")) return false;
	}
	return true;
}

static bool TestBootstrap(bool validation)
{
	VulkanBootstrapReport report = RunBootstrap(validation, true);
	if(!Check(report.status == VulkanProbeStatus::Ok, "bootstrap should pass")) return false;
	if(!Check(report.logical_device_created, "logical device should be created")) return false;
	if(!Check(report.graphics_queue_acquired, "graphics queue should be acquired")) return false;
	if(!Check(report.selected_device.suitable, "selected device should be suitable")) return false;
	if(!Check(report.selected_device.dynamic_rendering, "dynamic rendering should be enabled")) return false;
	if(!Check(report.selected_device.synchronization2, "synchronization2 should be enabled")) return false;
	if(!Check(report.selected_device.selected_queue_family_index >= 0, "queue family should be selected")) return false;
	if(!Check(report.cleanup_state_cleared, "bootstrap should clear internal state")) return false;
	if(!Check(report.clean_shutdown, "bootstrap should cleanly shut down")) return false;
	if(validation) {
		if(!Check(report.validation_warning_count == 0, "validation warnings should be zero")) return false;
		if(!Check(report.validation_error_count == 0, "validation errors should be zero")) return false;
		if(!Check(report.validation_messages.IsEmpty(), "validation messages should be empty")) return false;
	}
	return true;
}

static bool TestValidationPropagation()
{
	VulkanPreflightReport preflight = RunPreflight(true);
	if(!Check(preflight.status == VulkanProbeStatus::Ok, "validation preflight should pass")) return false;
	if(!Check(preflight.validation_warning_count == 0 && preflight.validation_error_count == 0, "validation counts should propagate")) return false;
	if(!Check(preflight.validation_messages.IsEmpty(), "validation messages should propagate")) return false;

	VulkanBootstrapReport bootstrap = RunBootstrap(true, true);
	if(!Check(bootstrap.status == VulkanProbeStatus::Ok, "validation bootstrap should pass")) return false;
	if(!Check(bootstrap.validation_warning_count == bootstrap.preflight.validation_warning_count, "bootstrap warning count should mirror preflight")) return false;
	if(!Check(bootstrap.validation_error_count == bootstrap.preflight.validation_error_count, "bootstrap error count should mirror preflight")) return false;
	if(!Check(bootstrap.validation_messages.GetCount() == bootstrap.preflight.validation_messages.GetCount(), "bootstrap validation messages should mirror preflight")) return false;
	return true;
}

static bool TestRepeat()
{
	String device_name;
	int queue_family = -1;
	for(int i = 0; i < 10; ++i) {
		VulkanBootstrapReport report = RunBootstrap(true, true);
		if(!Check(report.status == VulkanProbeStatus::Ok, "repeat cycle should pass")) return false;
		if(!Check(report.validation_error_count == 0, "repeat cycle should have no validation errors")) return false;
		if(!Check(report.cleanup_state_cleared, "repeat cycle should clear internal state")) return false;
		if(i == 0) {
			device_name = report.selected_device.name;
			queue_family = report.selected_device.selected_queue_family_index;
		}
		else {
			if(!Check(report.selected_device.name == device_name, "selected device should remain stable")) return false;
			if(!Check(report.selected_device.selected_queue_family_index == queue_family, "queue family should remain stable")) return false;
		}
		if(!Check(report.clean_shutdown, "repeat cycle should cleanly shut down")) return false;
	}
	return true;
}

static bool TestMissingLoaderFunction()
{
	VulkanBootstrapReport report = RunBootstrap(false, true, "vkCreateDevice");
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "missing loader function should be reported")) return false;
	if(!Check(report.instance_error == "vkCreateDevice", "missing loader function name should be preserved")) return false;
	if(!Check(report.cleanup_state_cleared, "missing loader function should still clear state")) return false;
	return Check(report.clean_shutdown, "missing loader function should clean up what it can");
}

static bool TestInjectedFailureCleanup()
{
	VulkanBootstrapReport report = RunBootstrap(false, true, "vkGetDeviceQueue");
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "missing device function should be reported")) return false;
	if(!Check(report.device_error == "vkGetDeviceQueue", "missing device function name should be preserved")) return false;
	if(!Check(report.cleanup_state_cleared, "injected failure should clear internal state")) return false;
	return Check(report.clean_shutdown, "injected failure should still cleanly release device");
}

static bool TestCleanupHonesty()
{
	VulkanBootstrapReport report = RunBootstrap(false, true, "vkDestroyDevice");
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "missing destroy function should be reported")) return false;
	if(!Check(report.device_error == "vkDestroyDevice", "missing destroy function name should be preserved")) return false;
	if(!Check(report.cleanup_state_cleared, "cleanup state should still be cleared")) return false;
	return Check(!report.clean_shutdown, "cannot claim clean shutdown when device could not be destroyed");
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ok &= TestPreflight(false);
	ok &= TestPreflight(true);
	ok &= TestBootstrap(false);
	ok &= TestBootstrap(true);
	ok &= TestValidationPropagation();
	ok &= TestRepeat();
	ok &= TestMissingLoaderFunction();
	ok &= TestInjectedFailureCleanup();
	ok &= TestCleanupHonesty();
	if(ok) {
		Cout() << "RenderVulkanTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
