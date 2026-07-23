#pragma once

#include <RenderVulkan/RenderVulkan.h>

namespace Upp {

namespace VulkanTestHooks {

struct VulkanRuntimeDeviceDiagnostics {
	uint64_t runtime_create_count = 0;
	uint64_t runtime_live_count = 0;
	uint64_t runtime_id = 0;
	uint64_t instance_create_count = 0;
	uint64_t instance_live_count = 0;
	uint64_t debug_messenger_create_count = 0;
	uint64_t debug_messenger_live_count = 0;
	uint64_t physical_device_discovery_count = 0;
	uint64_t device_create_count = 0;
	uint64_t device_live_count = 0;
	uint64_t device_id = 0;
	uint64_t surface_create_count = 0;
	uint64_t surface_live_count = 0;
	uint64_t surface_id = 0;
};

struct VulkanInstanceOptionsTestResult {
	bool opened = false;
	bool close_ok = false;
	String error;
	Vector<String> enabled_layers;
	Vector<String> enabled_extensions;
	VulkanRuntimeDeviceDiagnostics diagnostics;
};

enum class VulkanValidationTestPoint {
	None,
	BeforeDeviceCreation,
	AfterDeviceCreation,
	DuringDeviceCleanup,
};

struct VulkanValidationTestInjection {
	bool enabled = false;
	bool error = false;
	VulkanValidationTestPoint point = VulkanValidationTestPoint::None;
	bool force_device_cleanup_failure = false;
	VkResult device_cleanup_result = VK_ERROR_DEVICE_LOST;
	String message;
};

void SetVulkanValidationTestInjection(const VulkanValidationTestInjection& injection);
void ClearVulkanValidationTestInjection();
VulkanRuntimeDeviceDiagnostics GetVulkanRuntimeDeviceDiagnostics();
void ClearVulkanRuntimeDeviceDiagnostics();
VulkanInstanceOptionsTestResult RunVulkanInstanceOptionsTest(bool validation, bool win32_surface,
	bool has_surface_extension = true, bool has_win32_surface_extension = true,
	bool has_validation_layer = true, bool has_debug_utils_extension = true,
	const char *application_name = nullptr);

}

} // namespace Upp
