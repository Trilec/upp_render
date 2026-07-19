#include <RenderVulkan/RenderVulkan.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

using namespace Upp;
using Upp::VulkanTestHooks::ClearVulkanValidationTestInjection;
using Upp::VulkanTestHooks::SetVulkanValidationTestInjection;
using Upp::VulkanTestHooks::VulkanValidationTestInjection;
using Upp::VulkanTestHooks::VulkanValidationTestPoint;

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

static VulkanPreflightReport RunPreflight(bool validation, const char *missing_proc = nullptr, const VulkanValidationTestInjection *injection = nullptr)
{
	g_missing_proc = missing_proc;
	if(injection)
		SetVulkanValidationTestInjection(*injection);
	else
		ClearVulkanValidationTestInjection();
	VulkanPreflight probe;
	VulkanPreflightReport report = probe.Run(validation, &TestResolver);
	ClearVulkanValidationTestInjection();
	g_missing_proc = nullptr;
	return report;
}

static VulkanBootstrapReport RunBootstrap(bool validation, bool create_device, const char *missing_proc = nullptr, const VulkanValidationTestInjection *injection = nullptr)
{
	g_missing_proc = missing_proc;
	if(injection)
		SetVulkanValidationTestInjection(*injection);
	else
		ClearVulkanValidationTestInjection();
	VulkanBootstrap probe;
	VulkanBootstrapReport report = probe.Run(validation, create_device, &TestResolver);
	ClearVulkanValidationTestInjection();
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

static VulkanValidationTestInjection MakeInjection(bool error, VulkanValidationTestPoint point, const char *message, bool cleanup_failure = false, VkResult cleanup_result = VK_ERROR_DEVICE_LOST)
{
	VulkanValidationTestInjection inj;
	inj.enabled = true;
	inj.error = error;
	inj.point = point;
	inj.message = message;
	inj.force_device_cleanup_failure = cleanup_failure;
	inj.device_cleanup_result = cleanup_result;
	return inj;
}

static bool TestValidationWarningPropagation()
{
	VulkanValidationTestInjection inj = MakeInjection(false, VulkanValidationTestPoint::BeforeDeviceCreation, "synthetic warning");
	VulkanBootstrapReport report = RunBootstrap(true, true, nullptr, &inj);
	if(!Check(report.status == VulkanProbeStatus::Ok, "warning run should still pass")) return false;
	if(!Check(report.validation_warning_count == 1, "warning count should be one")) return false;
	if(!Check(report.validation_error_count == 0, "warning run should have zero errors")) return false;
	if(!Check(report.validation_messages.GetCount() == 1 && report.validation_messages[0].Find("synthetic warning") >= 0, "warning message should survive cleanup")) return false;
	if(!Check(report.clean_shutdown, "warning run should cleanly shut down")) return false;
	return Check(report.cleanup_state_cleared, "warning run should clear internal state");
}

static bool TestValidationErrorBeforeDeviceCreation()
{
	VulkanValidationTestInjection inj = MakeInjection(true, VulkanValidationTestPoint::BeforeDeviceCreation, "synthetic pre-device error");
	VulkanBootstrapReport report = RunBootstrap(true, true, nullptr, &inj);
	if(!Check(report.status == VulkanProbeStatus::ValidationErrorsReported, "pre-device error should report validation failure")) return false;
	if(!Check(report.validation_error_count == 1, "pre-device error count should be one")) return false;
	if(!Check(report.validation_messages.GetCount() == 1 && report.validation_messages[0].Find("synthetic pre-device error") >= 0, "pre-device error message should survive cleanup")) return false;
	if(!Check(report.clean_shutdown, "pre-device error run should cleanly shut down")) return false;
	return Check(report.cleanup_state_cleared, "pre-device error run should clear internal state");
}

static bool TestValidationErrorDuringDeviceStage()
{
	VulkanValidationTestInjection inj = MakeInjection(true, VulkanValidationTestPoint::DuringDeviceCleanup, "synthetic device-stage error");
	VulkanBootstrapReport report = RunBootstrap(true, true, nullptr, &inj);
	if(!Check(report.status == VulkanProbeStatus::ValidationErrorsReported, "device-stage error should report validation failure")) return false;
	if(!Check(report.validation_error_count == 1, "device-stage error count should be one")) return false;
	if(!Check(report.validation_messages.GetCount() == 1 && report.validation_messages[0].Find("synthetic device-stage error") >= 0, "device-stage message should survive cleanup")) return false;
	if(!Check(report.clean_shutdown, "device-stage error run should cleanly shut down")) return false;
	return Check(report.cleanup_state_cleared, "device-stage error run should clear internal state");
}

static bool TestPrimaryFailurePlusValidationError()
{
	VulkanValidationTestInjection inj = MakeInjection(true, VulkanValidationTestPoint::BeforeDeviceCreation, "primary failure helper error");
	VulkanBootstrapReport report = RunBootstrap(true, true, "vkGetDeviceQueue", &inj);
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "primary failure should remain primary")) return false;
	if(!Check(report.device_error == "vkGetDeviceQueue", "primary failure name should be preserved")) return false;
	if(!Check(report.validation_error_count == 1, "validation error should still be counted")) return false;
	if(!Check(report.validation_messages.GetCount() == 1 && report.validation_messages[0].Find("primary failure helper error") >= 0, "validation message should survive primary failure")) return false;
	if(!Check(report.cleanup_state_cleared, "primary failure path should clear internal state")) return false;
	return Check(report.clean_shutdown, "primary failure path should still clean up");
}

static bool TestDeviceCleanupFailureDetails()
{
	VulkanValidationTestInjection inj = MakeInjection(false, VulkanValidationTestPoint::DuringDeviceCleanup, "cleanup warning", true, VK_ERROR_DEVICE_LOST);
	VulkanBootstrapReport report = RunBootstrap(true, true, nullptr, &inj);
	if(!Check(report.status == VulkanProbeStatus::CleanupFailed, "cleanup failure should become primary when no operational failure exists")) return false;
	if(!Check(report.device_cleanup_ok == false, "device cleanup should fail")) return false;
	if(!Check(report.device_cleanup_result == VK_ERROR_DEVICE_LOST, "device cleanup VkResult should be recorded")) return false;
	if(!Check(!report.device_cleanup_error.IsEmpty(), "device cleanup error text should be recorded")) return false;
	if(!Check(!report.clean_shutdown, "cleanup failure should mark shutdown unclean")) return false;
	return Check(report.cleanup_state_cleared, "cleanup failure should still clear internal state");
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

static bool TestMissingDeviceFunction()
{
	VulkanBootstrapReport report = RunBootstrap(false, true, "vkCreateDevice");
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "missing device function should be reported")) return false;
	if(!Check(report.instance_error == "vkCreateDevice", "missing device function name should be preserved")) return false;
	if(!Check(report.cleanup_state_cleared, "missing device function should still clear state")) return false;
	return Check(report.clean_shutdown, "missing device function should clean up what it can");
}

static bool TestInjectedFailureCleanup()
{
	VulkanValidationTestInjection inj = MakeInjection(true, VulkanValidationTestPoint::BeforeDeviceCreation, "primary failure cleanup error");
	VulkanBootstrapReport report = RunBootstrap(true, true, "vkGetDeviceQueue", &inj);
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "missing device function should be reported")) return false;
	if(!Check(report.device_error == "vkGetDeviceQueue", "missing device function name should be preserved")) return false;
	if(!Check(report.validation_error_count == 1, "primary failure should keep validation error count")) return false;
	if(!Check(report.validation_messages.GetCount() == 1 && report.validation_messages[0].Find("primary failure cleanup error") >= 0, "primary failure should keep validation message")) return false;
	if(!Check(report.cleanup_state_cleared, "injected failure should clear internal state")) return false;
	return Check(report.clean_shutdown, "injected failure should still cleanly release device");
}

static bool TestCleanupHonesty()
{
	VulkanValidationTestInjection inj = MakeInjection(true, VulkanValidationTestPoint::DuringDeviceCleanup, "cleanup failure warning", true, VK_ERROR_DEVICE_LOST);
	VulkanBootstrapReport report = RunBootstrap(true, true, nullptr, &inj);
	if(!Check(report.status == VulkanProbeStatus::CleanupFailed, "cleanup failure with validation should still be cleanup failed")) return false;
	if(!Check(report.validation_error_count == 1, "validation error should be retained on cleanup failure")) return false;
	if(!Check(report.validation_messages.GetCount() == 1 && report.validation_messages[0].Find("cleanup failure warning") >= 0, "validation message should be retained on cleanup failure")) return false;
	if(!Check(report.device_cleanup_ok == false, "cleanup failure should be recorded")) return false;
	if(!Check(report.device_cleanup_result == VK_ERROR_DEVICE_LOST, "cleanup VkResult should be preserved")) return false;
	if(!Check(report.device_cleanup_error.Find("vkDeviceWaitIdle failed") >= 0, "cleanup error text should explain the wait failure")) return false;
	if(!Check(report.cleanup_state_cleared, "cleanup state should still be cleared")) return false;
	return Check(!report.clean_shutdown, "cannot claim clean shutdown when device wait idle fails");
}

static bool TestMissingGlobalFunction(const char *name)
{
	VulkanBootstrapReport report = RunBootstrap(false, true, name);
	if(!Check(report.status == VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "global missing function should be reported")) return false;
	if(!Check(report.loader_error == name, "global missing function name should be preserved")) return false;
	if(!Check(report.cleanup_state_cleared, "global missing function should clear dispatch state")) return false;
	return Check(report.clean_shutdown, "global missing function should clean up what it can");
}

CONSOLE_APP_MAIN
{
	bool ok = true;
	ok &= TestPreflight(false);
	ok &= TestPreflight(true);
	ok &= TestBootstrap(false);
	ok &= TestBootstrap(true);
	ok &= TestValidationPropagation();
	ok &= TestValidationWarningPropagation();
	ok &= TestValidationErrorBeforeDeviceCreation();
	ok &= TestValidationErrorDuringDeviceStage();
	ok &= TestPrimaryFailurePlusValidationError();
	ok &= TestDeviceCleanupFailureDetails();
	ok &= TestRepeat();
	ok &= TestMissingGlobalFunction("vkEnumerateInstanceLayerProperties");
	ok &= TestMissingGlobalFunction("vkEnumerateInstanceExtensionProperties");
	ok &= TestMissingGlobalFunction("vkCreateInstance");
	ok &= TestMissingDeviceFunction();
	ok &= TestInjectedFailureCleanup();
	ok &= TestCleanupHonesty();
	if(ok) {
		Cout() << "RenderVulkanTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
