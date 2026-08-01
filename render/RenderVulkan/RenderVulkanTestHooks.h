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
	String error;
	Vector<String> enabled_layers;
	Vector<String> enabled_extensions;
};

struct VulkanSurfaceSessionAccountingResult {
	VulkanSurfaceReport report;
	VulkanSurfaceReport repeat_report;
	VulkanRuntimeDeviceDiagnostics open_diag;
	VulkanRuntimeDeviceDiagnostics close_diag;
	VulkanRuntimeDeviceDiagnostics repeat_close_diag;
	String error;
};

struct VulkanSharedInstanceRegistryAcquireResult {
	void *entry = nullptr;
	bool newly_created = false;
	VulkanPreflightReport preflight;
	bool debug_messenger_created = false;
	int failure_stage = 0;
	String error;
	VulkanRuntimeDeviceDiagnostics diag;
};

struct VulkanSharedInstanceRegistryReleaseResult {
	bool released = false;
	int registry_entry_count = 0;
	VulkanRuntimeDeviceDiagnostics diag;
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
	bool has_validation_layer = true, bool has_debug_utils_extension = true);
bool TestVulkanInstanceCompatibility(bool validation_a, bool surface_a, bool validation_b, bool surface_b,
	const char *application_name_a = nullptr, const char *application_name_b = nullptr);
bool TestVulkanInstanceOwner(bool validation, VulkanProcResolver resolver, int& out_failure_stage,
	bool& out_debug_messenger_created, VulkanRuntimeDeviceDiagnostics& out_diag);
bool TestVulkanInstanceOwnerCompatibility(bool validation, bool win32_surface);
bool TestVulkanSurfaceOwner(bool validation, VulkanProcResolver resolver, int& out_failure_stage,
	VulkanRuntimeDeviceDiagnostics& out_diag);
bool TestVulkanSurfaceOwnerCompatibility(bool validation);
bool TestVulkanSurfaceSessionLifecycle(bool validation, VulkanProcResolver resolver, VulkanSurfaceSessionAccountingResult& out_result);
bool TestVulkanSurfaceSessionPostCreateFailure(bool validation, VulkanProcResolver resolver, VulkanSurfaceSessionAccountingResult& out_result);
bool TestVulkanSurfaceSessionCleanupFailure(bool validation, VulkanProcResolver resolver, VulkanSurfaceSessionAccountingResult& out_result);
bool TestVulkanSharedInstanceEntryLifecycle(VulkanProcResolver resolver, VulkanRuntimeDeviceDiagnostics& out_diag);
bool TestVulkanSharedInstanceEntrySafety(VulkanProcResolver resolver, VulkanRuntimeDeviceDiagnostics& out_diag);
bool TestVulkanSharedInstanceEntryIncompatible(bool base_validation, bool base_surface, bool test_validation, bool test_surface, VulkanRuntimeDeviceDiagnostics& out_diag);
bool TestVulkanSharedInstanceRegistryReuse(VulkanProcResolver resolver, VulkanSharedInstanceRegistryAcquireResult& first, VulkanSharedInstanceRegistryAcquireResult& second);
bool TestVulkanSharedInstanceRegistryStability(VulkanProcResolver resolver, VulkanSharedInstanceRegistryAcquireResult& first, VulkanSharedInstanceRegistryAcquireResult& incompatible, VulkanSharedInstanceRegistryReleaseResult& release);
bool TestVulkanSharedInstanceRegistryFailures(VulkanProcResolver resolver, VulkanSharedInstanceRegistryAcquireResult& dispatch_failure, VulkanSharedInstanceRegistryAcquireResult& instance_failure, VulkanSharedInstanceRegistryAcquireResult& cleanup_failure);
bool TestVulkanSharedInstanceRegistryInvalidRelease(VulkanProcResolver resolver, VulkanSharedInstanceRegistryReleaseResult& result);

}

} // namespace Upp
