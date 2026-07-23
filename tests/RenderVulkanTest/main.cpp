#include <RenderVulkan/RenderVulkan.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>
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

static bool CheckQueueFamilyInfo(const VulkanQueueFamilyInfo& a, const VulkanQueueFamilyInfo& b, const char *prefix)
{
	return Check(a.index == b.index, prefix) &&
	       Check(a.flags == b.flags, prefix) &&
	       Check(a.count == b.count, prefix) &&
	       Check(a.graphics == b.graphics, prefix) &&
	       Check(a.present == b.present, prefix) &&
	       Check(a.compute == b.compute, prefix) &&
	       Check(a.transfer == b.transfer, prefix) &&
	       Check(a.sparse_binding == b.sparse_binding, prefix);
}

static bool CheckExtensionInfo(const VulkanExtensionInfo& a, const VulkanExtensionInfo& b, const char *prefix)
{
	return Check(a.name == b.name, prefix) &&
	       Check(a.spec_version == b.spec_version, prefix);
}

static bool CheckLayerInfo(const VulkanLayerInfo& a, const VulkanLayerInfo& b, const char *prefix)
{
	return Check(a.name == b.name, prefix) &&
	       Check(a.description == b.description, prefix) &&
	       Check(a.spec_version == b.spec_version, prefix);
}

static bool CheckDeviceInfo(const VulkanDeviceInfo& a, const VulkanDeviceInfo& b, const char *prefix)
{
	if(!Check(a.name == b.name, prefix)) return false;
	if(!Check(a.type == b.type, prefix)) return false;
	if(!Check(a.vendor_id == b.vendor_id, prefix)) return false;
	if(!Check(a.device_id == b.device_id, prefix)) return false;
	if(!Check(a.driver_version == b.driver_version, prefix)) return false;
	if(!Check(a.api_version == b.api_version, prefix)) return false;
	if(!Check(a.graphics_queue == b.graphics_queue, prefix)) return false;
	if(!Check(a.dynamic_rendering == b.dynamic_rendering, prefix)) return false;
	if(!Check(a.synchronization2 == b.synchronization2, prefix)) return false;
	if(!Check(a.suitable == b.suitable, prefix)) return false;
	if(!Check(a.selection_reason == b.selection_reason, prefix)) return false;
	if(!Check(a.selected_queue_family_index == b.selected_queue_family_index, prefix)) return false;
	if(!Check(a.selected_queue_count == b.selected_queue_count, prefix)) return false;
	if(!Check(a.selected_queue_flags == b.selected_queue_flags, prefix)) return false;
	if(!Check(a.selected_queue_compute == b.selected_queue_compute, prefix)) return false;
	if(!Check(a.selected_queue_transfer == b.selected_queue_transfer, prefix)) return false;
	if(!Check(a.logical_device_created == b.logical_device_created, prefix)) return false;
	if(!Check(a.graphics_queue_acquired == b.graphics_queue_acquired, prefix)) return false;
	if(!Check(a.missing_requirements.GetCount() == b.missing_requirements.GetCount(), prefix)) return false;
	for(int i = 0; i < a.missing_requirements.GetCount(); ++i)
		if(!Check(a.missing_requirements[i] == b.missing_requirements[i], prefix)) return false;
	if(!Check(a.queue_families.GetCount() == b.queue_families.GetCount(), prefix)) return false;
	for(int i = 0; i < a.queue_families.GetCount(); ++i)
		if(!CheckQueueFamilyInfo(a.queue_families[i], b.queue_families[i], prefix)) return false;
	if(!Check(a.device_extensions.GetCount() == b.device_extensions.GetCount(), prefix)) return false;
	for(int i = 0; i < a.device_extensions.GetCount(); ++i)
		if(!CheckExtensionInfo(a.device_extensions[i], b.device_extensions[i], prefix)) return false;
	return true;
}

static bool CheckPreflightReport(const VulkanPreflightReport& a, const VulkanPreflightReport& b, const char *prefix)
{
	if(!Check(a.status == b.status, prefix)) return false;
	if(!Check(a.status_text == b.status_text, prefix)) return false;
	if(!Check(a.loader_available == b.loader_available, prefix)) return false;
	if(!Check(a.loader_version == b.loader_version, prefix)) return false;
	if(!Check(a.validation_requested == b.validation_requested, prefix)) return false;
	if(!Check(a.validation_available == b.validation_available, prefix)) return false;
	if(!Check(a.debug_utils_available == b.debug_utils_available, prefix)) return false;
	if(!Check(a.instance_created == b.instance_created, prefix)) return false;
	if(!Check(a.clean_shutdown == b.clean_shutdown, prefix)) return false;
	if(!Check(a.cleanup_state_cleared == b.cleanup_state_cleared, prefix)) return false;
	if(!Check(a.validation_warning_count == b.validation_warning_count, prefix)) return false;
	if(!Check(a.validation_error_count == b.validation_error_count, prefix)) return false;
	if(!Check(a.validation_messages.GetCount() == b.validation_messages.GetCount(), prefix)) return false;
	for(int i = 0; i < a.validation_messages.GetCount(); ++i)
		if(!Check(a.validation_messages[i] == b.validation_messages[i], prefix)) return false;
	if(!Check(a.runtime_error == b.runtime_error, prefix)) return false;
	if(!Check(a.loader_error == b.loader_error, prefix)) return false;
	if(!Check(a.layer_error == b.layer_error, prefix)) return false;
	if(!Check(a.extension_error == b.extension_error, prefix)) return false;
	if(!Check(a.instance_error == b.instance_error, prefix)) return false;
	if(!Check(a.physical_device_error == b.physical_device_error, prefix)) return false;
	if(!Check(a.instance_layers.GetCount() == b.instance_layers.GetCount(), prefix)) return false;
	for(int i = 0; i < a.instance_layers.GetCount(); ++i)
		if(!CheckLayerInfo(a.instance_layers[i], b.instance_layers[i], prefix)) return false;
	if(!Check(a.instance_extensions.GetCount() == b.instance_extensions.GetCount(), prefix)) return false;
	for(int i = 0; i < a.instance_extensions.GetCount(); ++i)
		if(!CheckExtensionInfo(a.instance_extensions[i], b.instance_extensions[i], prefix)) return false;
	if(!Check(a.devices.GetCount() == b.devices.GetCount(), prefix)) return false;
	for(int i = 0; i < a.devices.GetCount(); ++i)
		if(!CheckDeviceInfo(a.devices[i], b.devices[i], prefix)) return false;
	if(!Check(a.suitable_device_count == b.suitable_device_count, prefix)) return false;
	return true;
}

static bool CheckBootstrapReport(const VulkanBootstrapReport& a, const VulkanBootstrapReport& b, const char *prefix)
{
	if(!Check(a.status == b.status, prefix)) return false;
	if(!Check(a.status_text == b.status_text, prefix)) return false;
	if(!CheckPreflightReport(a.preflight, b.preflight, prefix)) return false;
	if(!Check(a.validation_requested == b.validation_requested, prefix)) return false;
	if(!Check(a.create_device_requested == b.create_device_requested, prefix)) return false;
	if(!Check(a.validation_available == b.validation_available, prefix)) return false;
	if(!Check(a.debug_utils_available == b.debug_utils_available, prefix)) return false;
	if(!Check(a.debug_messenger_created == b.debug_messenger_created, prefix)) return false;
	if(!Check(a.logical_device_created == b.logical_device_created, prefix)) return false;
	if(!Check(a.graphics_queue_acquired == b.graphics_queue_acquired, prefix)) return false;
	if(!Check(a.clean_shutdown == b.clean_shutdown, prefix)) return false;
	if(!Check(a.cleanup_state_cleared == b.cleanup_state_cleared, prefix)) return false;
	if(!Check(a.instance_cleanup_ok == b.instance_cleanup_ok, prefix)) return false;
	if(!Check(a.device_cleanup_ok == b.device_cleanup_ok, prefix)) return false;
	if(!Check(a.dispatch_cleanup_ok == b.dispatch_cleanup_ok, prefix)) return false;
	if(!Check(a.validation_warning_count == b.validation_warning_count, prefix)) return false;
	if(!Check(a.validation_error_count == b.validation_error_count, prefix)) return false;
	if(!Check(a.validation_messages.GetCount() == b.validation_messages.GetCount(), prefix)) return false;
	for(int i = 0; i < a.validation_messages.GetCount(); ++i)
		if(!Check(a.validation_messages[i] == b.validation_messages[i], prefix)) return false;
	if(!Check(a.device_cleanup_result == b.device_cleanup_result, prefix)) return false;
	if(!Check(a.device_cleanup_error == b.device_cleanup_error, prefix)) return false;
	if(!Check(a.runtime_error == b.runtime_error, prefix)) return false;
	if(!Check(a.loader_error == b.loader_error, prefix)) return false;
	if(!Check(a.validation_error == b.validation_error, prefix)) return false;
	if(!Check(a.instance_error == b.instance_error, prefix)) return false;
	if(!Check(a.physical_device_error == b.physical_device_error, prefix)) return false;
	if(!Check(a.device_error == b.device_error, prefix)) return false;
	if(!CheckDeviceInfo(a.selected_device, b.selected_device, prefix)) return false;
	return true;
}

static bool CheckSurfaceReport(const VulkanSurfaceReport& a, const VulkanSurfaceReport& b, const char *prefix)
{
	if(!Check(a.status == b.status, prefix)) return false;
	if(!Check(a.status_text == b.status_text, prefix)) return false;
	if(!CheckPreflightReport(a.preflight, b.preflight, prefix)) return false;
	if(!Check(a.validation_requested == b.validation_requested, prefix)) return false;
	if(!Check(a.surface_requested == b.surface_requested, prefix)) return false;
	if(!Check(a.validation_available == b.validation_available, prefix)) return false;
	if(!Check(a.debug_utils_available == b.debug_utils_available, prefix)) return false;
	if(!Check(a.surface_created == b.surface_created, prefix)) return false;
	if(!Check(a.logical_device_created == b.logical_device_created, prefix)) return false;
	if(!Check(a.graphics_queue_acquired == b.graphics_queue_acquired, prefix)) return false;
	if(!Check(a.present_queue_acquired == b.present_queue_acquired, prefix)) return false;
	if(!Check(a.same_queue_family == b.same_queue_family, prefix)) return false;
	if(!Check(a.swapchain_enabled == b.swapchain_enabled, prefix)) return false;
	if(!Check(a.clean_shutdown == b.clean_shutdown, prefix)) return false;
	if(!Check(a.cleanup_state_cleared == b.cleanup_state_cleared, prefix)) return false;
	if(!Check(a.instance_cleanup_ok == b.instance_cleanup_ok, prefix)) return false;
	if(!Check(a.surface_cleanup_ok == b.surface_cleanup_ok, prefix)) return false;
	if(!Check(a.device_cleanup_ok == b.device_cleanup_ok, prefix)) return false;
	if(!Check(a.dispatch_cleanup_ok == b.dispatch_cleanup_ok, prefix)) return false;
	if(!Check(a.validation_warning_count == b.validation_warning_count, prefix)) return false;
	if(!Check(a.validation_error_count == b.validation_error_count, prefix)) return false;
	if(!Check(a.validation_messages.GetCount() == b.validation_messages.GetCount(), prefix)) return false;
	for(int i = 0; i < a.validation_messages.GetCount(); ++i)
		if(!Check(a.validation_messages[i] == b.validation_messages[i], prefix)) return false;
	if(!Check(a.runtime_error == b.runtime_error, prefix)) return false;
	if(!Check(a.loader_error == b.loader_error, prefix)) return false;
	if(!Check(a.validation_error == b.validation_error, prefix)) return false;
	if(!Check(a.instance_error == b.instance_error, prefix)) return false;
	if(!Check(a.surface_error == b.surface_error, prefix)) return false;
	if(!Check(a.physical_device_error == b.physical_device_error, prefix)) return false;
	if(!Check(a.device_error == b.device_error, prefix)) return false;
	if(!Check(a.native_window.kind == b.native_window.kind, prefix)) return false;
	if(!Check(a.native_window.handle == b.native_window.handle, prefix)) return false;
	if(!CheckDeviceInfo(a.selected_device, b.selected_device, prefix)) return false;
	if(!Check(a.graphics_queue_family_index == b.graphics_queue_family_index, prefix)) return false;
	if(!Check(a.present_queue_family_index == b.present_queue_family_index, prefix)) return false;
	if(!Check(a.graphics_queue_count == b.graphics_queue_count, prefix)) return false;
	if(!Check(a.present_queue_count == b.present_queue_count, prefix)) return false;
	if(!Check(a.graphics_queue_flags == b.graphics_queue_flags, prefix)) return false;
	if(!Check(a.present_queue_flags == b.present_queue_flags, prefix)) return false;
	if(!Check(a.surface_formats.GetCount() == b.surface_formats.GetCount(), prefix)) return false;
	for(int i = 0; i < a.surface_formats.GetCount(); ++i)
		if(!Check(a.surface_formats[i] == b.surface_formats[i], prefix)) return false;
	if(!Check(a.present_modes.GetCount() == b.present_modes.GetCount(), prefix)) return false;
	for(int i = 0; i < a.present_modes.GetCount(); ++i)
		if(!Check(a.present_modes[i] == b.present_modes[i], prefix)) return false;
	if(!Check(a.min_image_count == b.min_image_count, prefix)) return false;
	if(!Check(a.max_image_count == b.max_image_count, prefix)) return false;
	if(!Check(a.current_extent == b.current_extent, prefix)) return false;
	if(!Check(a.min_extent == b.min_extent, prefix)) return false;
	if(!Check(a.max_extent == b.max_extent, prefix)) return false;
	if(!Check(a.supported_transforms == b.supported_transforms, prefix)) return false;
	if(!Check(a.current_transform == b.current_transform, prefix)) return false;
	if(!Check(a.supported_composite_alpha == b.supported_composite_alpha, prefix)) return false;
	if(!Check(a.supported_image_usage == b.supported_image_usage, prefix)) return false;
	if(!Check(a.preferred_bgra8 == b.preferred_bgra8, prefix)) return false;
	if(!Check(a.preferred_rgba8 == b.preferred_rgba8, prefix)) return false;
	if(!Check(a.preferred_srgb == b.preferred_srgb, prefix)) return false;
	if(!Check(a.preferred_mailbox == b.preferred_mailbox, prefix)) return false;
	if(!Check(a.preferred_fifo == b.preferred_fifo, prefix)) return false;
	return true;
}

static bool TestReportCopyReset()
{
	VulkanDeviceInfo device;
	device.name = "device";
	device.type = "discrete";
	device.vendor_id = 11;
	device.device_id = 22;
	device.driver_version = 33;
	device.api_version = 44;
	device.graphics_queue = true;
	device.dynamic_rendering = true;
	device.synchronization2 = true;
	device.suitable = true;
	device.selection_reason = "selected";
	device.selected_queue_family_index = 5;
	device.selected_queue_count = 6;
	device.selected_queue_flags = 7;
	device.selected_queue_compute = true;
	device.selected_queue_transfer = false;
	device.logical_device_created = true;
	device.graphics_queue_acquired = true;
	device.missing_requirements.Add("req-a");
	device.missing_requirements.Add("req-b");
	device.queue_families.Add();
	device.queue_families[0].index = 1;
	device.queue_families[0].flags = 2;
	device.queue_families[0].count = 3;
	device.queue_families[0].graphics = true;
	device.queue_families[0].present = false;
	device.queue_families[0].compute = true;
	device.queue_families[0].transfer = false;
	device.queue_families[0].sparse_binding = true;
	device.device_extensions.Add();
	device.device_extensions[0].name = "ext-a";
	device.device_extensions[0].spec_version = 9;
	VulkanDeviceInfo device_copy = device;
	if(!CheckDeviceInfo(device, device_copy, "device copy should match")) return false;
	device_copy = VulkanDeviceInfo();
	if(!Check(device_copy.name.IsEmpty(), "device reset should clear name")) return false;
	if(!Check(device_copy.missing_requirements.IsEmpty(), "device reset should clear requirements")) return false;

	VulkanPreflightReport preflight;
	preflight.status = VulkanProbeStatus::PresentationUnsupported;
	preflight.status_text = "presentation unsupported";
	preflight.loader_available = true;
	preflight.loader_version = 123;
	preflight.validation_requested = true;
	preflight.validation_available = true;
	preflight.debug_utils_available = true;
	preflight.instance_created = true;
	preflight.clean_shutdown = true;
	preflight.cleanup_state_cleared = true;
	preflight.validation_warning_count = 4;
	preflight.validation_error_count = 5;
	preflight.validation_messages.Add("warn");
	preflight.runtime_error = "runtime";
	preflight.loader_error = "loader";
	preflight.layer_error = "layer";
	preflight.extension_error = "ext";
	preflight.instance_error = "instance";
	preflight.physical_device_error = "pdev";
	preflight.instance_layers.Add();
	preflight.instance_layers[0].name = "layer-a";
	preflight.instance_layers[0].description = "desc";
	preflight.instance_layers[0].spec_version = 42;
	preflight.instance_extensions.Add();
	preflight.instance_extensions[0].name = "ext-a";
	preflight.instance_extensions[0].spec_version = 43;
	preflight.devices.Add(device);
	preflight.suitable_device_count = 1;
	VulkanPreflightReport preflight_copy = preflight;
	if(!CheckPreflightReport(preflight, preflight_copy, "preflight copy should match")) return false;
	preflight_copy = VulkanPreflightReport();
	if(!Check(preflight_copy.validation_messages.IsEmpty(), "preflight reset should clear messages")) return false;
	if(!Check(preflight_copy.devices.IsEmpty(), "preflight reset should clear devices")) return false;

	VulkanBootstrapReport bootstrap;
	bootstrap.status = VulkanProbeStatus::CleanupFailed;
	bootstrap.status_text = "cleanup failed";
	bootstrap.preflight = preflight;
	bootstrap.validation_requested = true;
	bootstrap.create_device_requested = true;
	bootstrap.validation_available = true;
	bootstrap.debug_utils_available = true;
	bootstrap.debug_messenger_created = true;
	bootstrap.logical_device_created = true;
	bootstrap.graphics_queue_acquired = true;
	bootstrap.clean_shutdown = false;
	bootstrap.cleanup_state_cleared = true;
	bootstrap.instance_cleanup_ok = true;
	bootstrap.device_cleanup_ok = false;
	bootstrap.dispatch_cleanup_ok = true;
	bootstrap.validation_warning_count = 2;
	bootstrap.validation_error_count = 3;
	bootstrap.validation_messages.Add("bootstrap");
	bootstrap.device_cleanup_result = VK_ERROR_DEVICE_LOST;
	bootstrap.device_cleanup_error = "device cleanup";
	bootstrap.runtime_error = "runtime";
	bootstrap.loader_error = "loader";
	bootstrap.validation_error = "validation";
	bootstrap.instance_error = "instance";
	bootstrap.physical_device_error = "pdev";
	bootstrap.device_error = "device";
	bootstrap.selected_device = device;
	VulkanBootstrapReport bootstrap_copy = bootstrap;
	if(!CheckBootstrapReport(bootstrap, bootstrap_copy, "bootstrap copy should match")) return false;
	bootstrap_copy = VulkanBootstrapReport();
	if(!Check(bootstrap_copy.validation_messages.IsEmpty(), "bootstrap reset should clear messages")) return false;
	if(!Check(bootstrap_copy.device_cleanup_error.IsEmpty(), "bootstrap reset should clear cleanup error")) return false;

	VulkanSurfaceReport surface;
	surface.status = VulkanProbeStatus::SurfaceCapabilitiesFailed;
	surface.status_text = "surface failed";
	surface.preflight = preflight;
	surface.validation_requested = true;
	surface.surface_requested = true;
	surface.validation_available = true;
	surface.debug_utils_available = true;
	surface.surface_created = true;
	surface.logical_device_created = true;
	surface.graphics_queue_acquired = true;
	surface.present_queue_acquired = true;
	surface.same_queue_family = false;
	surface.swapchain_enabled = false;
	surface.clean_shutdown = false;
	surface.cleanup_state_cleared = true;
	surface.instance_cleanup_ok = true;
	surface.surface_cleanup_ok = true;
	surface.device_cleanup_ok = true;
	surface.dispatch_cleanup_ok = true;
	surface.validation_warning_count = 6;
	surface.validation_error_count = 7;
	surface.validation_messages.Add("surface");
	surface.runtime_error = "runtime";
	surface.loader_error = "loader";
	surface.validation_error = "validation";
	surface.instance_error = "instance";
	surface.surface_error = "surface";
	surface.physical_device_error = "pdev";
	surface.device_error = "device";
	surface.native_window.kind = GpuNativeWindowKind::Win32;
	surface.native_window.handle = 123;
	surface.selected_device = device;
	surface.graphics_queue_family_index = 4;
	surface.present_queue_family_index = 5;
	surface.graphics_queue_count = 6;
	surface.present_queue_count = 7;
	surface.graphics_queue_flags = 8;
	surface.present_queue_flags = 9;
	surface.surface_formats.Add("format-a");
	surface.present_modes.Add("mode-a");
	surface.min_image_count = 2;
	surface.max_image_count = 8;
	surface.current_extent = Size(640, 480);
	surface.min_extent = Size(1, 2);
	surface.max_extent = Size(1920, 1080);
	surface.supported_transforms = 10;
	surface.current_transform = 11;
	surface.supported_composite_alpha = 12;
	surface.supported_image_usage = 13;
	surface.preferred_bgra8 = true;
	surface.preferred_rgba8 = false;
	surface.preferred_srgb = true;
	surface.preferred_mailbox = false;
	surface.preferred_fifo = true;
	VulkanSurfaceReport surface_copy = surface;
	if(!CheckSurfaceReport(surface, surface_copy, "surface copy should match")) return false;
	surface_copy = VulkanSurfaceReport();
	if(!Check(surface_copy.surface_formats.IsEmpty(), "surface reset should clear formats")) return false;
	if(!Check(surface_copy.native_window.handle == 0, "surface reset should clear native handle")) return false;

	return true;
}

static bool TestSessionLifecycle()
{
	VulkanSurfaceSession session;
	if(!Check(!session.IsOpen(), "session should start closed")) return false;
	if(!Check(!session.IsReady(), "session should start not ready")) return false;
	if(!Check(session.GetError().IsEmpty(), "session should start without error")) return false;

	GpuNativeWindowDesc desc;
	desc.kind = GpuNativeWindowKind::None;
	desc.handle = 0;
	bool opened = session.Open(true, desc);
	if(!Check(!opened, "invalid native window should fail open")) return false;
	if(!Check(!session.IsReady(), "failed session should not be ready")) return false;
	if(!Check(!session.GetError().IsEmpty(), "failed open should retain error")) return false;
	const VulkanSurfaceReport& report = session.GetReport();
	if(!Check(report.cleanup_state_cleared, "failed open should clear state")) return false;
	session.Close();
	session.Close();
	if(!Check(session.GetReport().native_window.handle == 0, "close should release native handle")) return false;
	if(!Check(!session.IsOpen(), "close should be idempotent")) return false;
	if(!Check(!session.IsReady(), "closed session should not be ready")) return false;
	return true;
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
	ok &= TestReportCopyReset();
	ok &= TestSessionLifecycle();
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
