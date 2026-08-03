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
	uint64_t swapchain_create_count = 0;
	uint64_t swapchain_live_count = 0;
	uint64_t swapchain_id = 0;
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
	VulkanRuntimeDeviceDiagnostics pre_refusal_diag;
	VulkanRuntimeDeviceDiagnostics post_refusal_diag;
	int registry_entry_count = 0;
	int acquire_count = 0;
	bool opened = false;
	bool cleanup_ok = true;
	bool owner_cleared = false;
	bool recovery_succeeded = false;
	bool replacement_refused = false;
	bool incompatible_succeeded = false;
	int retained_registry_entry_count = 0;
	uint64_t retained_runtime_create_count = 0;
	uint64_t retained_instance_create_count = 0;
	uint64_t retained_debug_messenger_create_count = 0;
	bool reusable_preflight_equal = false;
	bool stable_address_preserved = false;
	uint64_t first_discovery_count = 0;
	uint64_t second_discovery_count = 0;
	int original_acquire_count = 0;
	int validation_mismatch_entry_count = 0;
	int surface_mismatch_entry_count = 0;
	int after_first_removal_entry_count = 0;
	int after_second_removal_entry_count = 0;
	bool identity_after_validation = false;
	bool identity_after_surface = false;
	bool identity_after_first_removal = false;
	bool identity_after_second_removal = false;
	bool state_preserved_after_validation = false;
	bool state_preserved_after_surface = false;
	bool state_preserved_after_first_removal = false;
	bool state_preserved_after_second_removal = false;
	bool compatibility_preserved = false;
	bool refusal_diagnostics_unchanged = false;
	bool retained_identity_preserved = false;
	bool retained_state_preserved = false;
	bool retained_compatibility_preserved = false;
};

struct VulkanSharedInstanceRegistryReleaseResult {
	bool released = false;
	int registry_entry_count = 0;
	VulkanRuntimeDeviceDiagnostics diag;
	bool null_release_rejected = false;
	bool foreign_release_rejected = false;
	bool cross_release_rejected = false;
	bool removed_release_rejected = false;
	int registry_a_count = 0;
	int registry_b_count = 0;
	int foreign_acquire_count = 0;
};

struct VulkanSharedInstanceLeaseTestResult {
	bool automatic_release = false;
	bool two_lease_reuse = false;
	bool non_final_release = false;
	bool final_release = false;
	bool reset_idempotent = false;
	bool move_transfer = false;
	bool occupied_refused = false;
	bool dispatch_failure_empty = false;
	bool instance_failure_empty = false;
	bool recovery = false;
	bool cleanup_failure_empty = false;
	bool no_double_release = false;
	bool detailed_outcomes = false;
	VulkanRuntimeDeviceDiagnostics diag;
	VulkanRuntimeDeviceDiagnostics pre_refusal_diag;
	VulkanRuntimeDeviceDiagnostics post_refusal_diag;
	int registry_entry_count = 0;
	int retained_acquire_count = 0;
	bool retained_opened = false;
	bool retained_cleanup_ok = true;
	bool retained_owner_cleared = false;
	bool occupied_identity_preserved = false;
	bool occupied_acquire_count_preserved = false;
	bool occupied_registry_preserved = false;
	bool occupied_diagnostics_unchanged = false;
	bool occupied_outputs_reset = false;
	bool dispatch_lease_empty = false;
	bool instance_lease_empty = false;
	bool dispatch_outputs_complete = false;
	bool instance_outputs_complete = false;
	bool retained_identity_after_destructor = false;
	bool destructor_diagnostics_unchanged = false;
	bool same_key_refused = false;
	bool refusal_diagnostics_unchanged = false;
	bool incompatible_lease_succeeded = false;
	bool source_empty_after_move = false;
	bool destination_registry_preserved = false;
	bool destination_entry_preserved = false;
	bool move_count_preserved = false;
	bool move_registry_count_preserved = false;
	bool move_diagnostics_unchanged = false;
	bool null_outcome_invalid = false;
	bool foreign_outcome_invalid = false;
	bool cross_outcome_invalid = false;
	bool non_final_outcome = false;
	bool final_outcome = false;
	bool cleanup_failed_outcome = false;
	VulkanRuntimeDeviceDiagnostics destructor_pre_diag;
	VulkanRuntimeDeviceDiagnostics destructor_post_diag;
	VulkanRuntimeDeviceDiagnostics refusal_pre_diag;
	VulkanRuntimeDeviceDiagnostics refusal_post_diag;
	int refusal_registry_entry_count = 0;
};

struct VulkanGroupedSurfaceSessionTestResult {
	bool compatible_shared = false;
	bool non_final_close = false;
	bool final_close = false;
	bool reverse_close = false;
	bool incompatible_entries = false;
	bool post_lease_failure = false;
	bool recovery = false;
	bool device_cleanup_failure_non_short_circuit = false;
	bool grouped_swapchains_separate = false;
	bool first_report_authoritative = false;
	bool second_report_authoritative = false;
	bool first_survivor_state = false;
	bool second_survivor_state = false;
	bool never_acquired_report = false;
	bool retained_cleanup_failure = false;
	bool same_key_refused = false;
	bool incompatible_after_failure = false;
	bool retained_state_unchanged = false;
	VulkanRuntimeDeviceDiagnostics refusal_pre_diag;
	VulkanRuntimeDeviceDiagnostics refusal_post_diag;
	int compatible_registry_entries = 0;
	int non_final_registry_entries = 0;
	int final_registry_entries = 0;
	int incompatible_registry_entries = 0;
	int failure_registry_entries = 0;
	int compatible_acquire_count = 0;
	int non_final_acquire_count = 0;
	VulkanRuntimeDeviceDiagnostics compatible_diag;
	VulkanRuntimeDeviceDiagnostics non_final_diag;
	VulkanRuntimeDeviceDiagnostics final_diag;
	VulkanRuntimeDeviceDiagnostics diag;
};

struct VulkanSwapchainTestResult {
	bool created = false;
	bool destroyed = false;
	bool idempotent = false;
	bool invalid_size_refused = false;
	bool already_created_refused = false;
	bool missing_procedure_recovered = false;
	VulkanSurfaceReport active_report;
	VulkanSurfaceReport destroyed_report;
	VulkanRuntimeDeviceDiagnostics active_diag;
	VulkanRuntimeDeviceDiagnostics final_diag;
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
bool TestVulkanSharedInstanceLease(VulkanProcResolver resolver, VulkanSharedInstanceLeaseTestResult& result);
bool TestVulkanGroupedSurfaceSessions(VulkanProcResolver resolver, VulkanGroupedSurfaceSessionTestResult& result);
bool TestVulkanSwapchain(VulkanProcResolver resolver, VulkanSwapchainTestResult& result);

}

} // namespace Upp
