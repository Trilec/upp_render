#pragma once

#include <Core/Core.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace Upp {

using VulkanProcResolver = FARPROC (WINAPI *)(HMODULE, LPCSTR);

enum class VulkanProbeStatus {
	Ok,
	RuntimeUnavailable,
	RequiredLoaderFunctionUnavailable,
	LoaderTooOld,
	LayerEnumerationFailed,
	ExtensionEnumerationFailed,
	ValidationUnavailable,
	DebugUtilsUnavailable,
	InstanceCreationFailed,
	PhysicalDeviceEnumerationFailed,
	NoPhysicalDevices,
	NoSuitableDevices,
	DeviceCreationFailed,
	ValidationErrorsReported,
};

struct VulkanLayerInfo : Moveable<VulkanLayerInfo> {
	String name;
	String description;
	uint32_t spec_version = 0;
};

struct VulkanExtensionInfo : Moveable<VulkanExtensionInfo> {
	String name;
	uint32_t spec_version = 0;
};

struct VulkanQueueFamilyInfo : Moveable<VulkanQueueFamilyInfo> {
	int index = -1;
	uint32_t flags = 0;
	uint32_t count = 0;
	bool graphics = false;
	bool compute = false;
	bool transfer = false;
	bool sparse_binding = false;
};

struct VulkanDeviceInfo : Moveable<VulkanDeviceInfo> {
	String name;
	String type;
	uint32_t vendor_id = 0;
	uint32_t device_id = 0;
	uint32_t driver_version = 0;
	uint32_t api_version = 0;
	bool graphics_queue = false;
	bool dynamic_rendering = false;
	bool synchronization2 = false;
	bool suitable = false;
	String selection_reason;
	int selected_queue_family_index = -1;
	uint32_t selected_queue_count = 0;
	uint32_t selected_queue_flags = 0;
	bool selected_queue_compute = false;
	bool selected_queue_transfer = false;
	bool logical_device_created = false;
	bool graphics_queue_acquired = false;
	Vector<String> missing_requirements;
	Vector<VulkanQueueFamilyInfo> queue_families;
	Vector<VulkanExtensionInfo> device_extensions;
};

struct VulkanPreflightReport : Moveable<VulkanPreflightReport> {
	VulkanProbeStatus status = VulkanProbeStatus::RuntimeUnavailable;
	String status_text;
	bool loader_available = false;
	uint32_t loader_version = 0;
	bool validation_requested = false;
	bool validation_available = false;
	bool debug_utils_available = false;
	bool instance_created = false;
	bool clean_shutdown = false;
	bool cleanup_state_cleared = false;
	int validation_warning_count = 0;
	int validation_error_count = 0;
	Vector<String> validation_messages;
	String runtime_error;
	String loader_error;
	String layer_error;
	String extension_error;
	String instance_error;
	String physical_device_error;
	Vector<VulkanLayerInfo> instance_layers;
	Vector<VulkanExtensionInfo> instance_extensions;
	Vector<VulkanDeviceInfo> devices;
	int suitable_device_count = 0;
};

struct VulkanBootstrapReport : Moveable<VulkanBootstrapReport> {
	VulkanProbeStatus status = VulkanProbeStatus::RuntimeUnavailable;
	String status_text;
	VulkanPreflightReport preflight;
	bool validation_requested = false;
	bool create_device_requested = false;
	bool validation_available = false;
	bool debug_utils_available = false;
	bool debug_messenger_created = false;
	bool logical_device_created = false;
	bool graphics_queue_acquired = false;
	bool clean_shutdown = false;
	bool cleanup_state_cleared = false;
	bool instance_cleanup_ok = false;
	bool device_cleanup_ok = false;
	bool dispatch_cleanup_ok = false;
	int validation_warning_count = 0;
	int validation_error_count = 0;
	Vector<String> validation_messages;
	int device_cleanup_result = 0;
	String device_cleanup_error;
	String runtime_error;
	String loader_error;
	String validation_error;
	String instance_error;
	String physical_device_error;
	String device_error;
	VulkanDeviceInfo selected_device;
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

class VulkanPreflight {
public:
	VulkanPreflight();

	VulkanPreflightReport Run(bool request_validation);
	VulkanPreflightReport Run(bool request_validation, VulkanProcResolver resolver);
	String Dump(const VulkanPreflightReport& report) const;

private:
	static String BoolText(bool value);
	static String StatusText(VulkanProbeStatus status);
	static String FormatVersion(uint32_t version);
	static String DeviceTypeText(VkPhysicalDeviceType type);
	static String QueueFlagsText(VkQueueFlags flags);
	static String LayerName(const VkLayerProperties& prop);
	static String ExtensionName(const VkExtensionProperties& prop);
	static uint32_t LayerVersionToUInt(const VkLayerProperties& prop);
	static uint32_t ExtensionVersionToUInt(const VkExtensionProperties& prop);
	static bool HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name);
	static void AppendMissing(VulkanDeviceInfo& device, const char *text);

	friend class VulkanBootstrap;
};

class VulkanBootstrap {
public:
	VulkanBootstrap();

	VulkanBootstrapReport Run(bool request_validation, bool create_device);
	VulkanBootstrapReport Run(bool request_validation, bool create_device, VulkanProcResolver resolver);
	String Dump(const VulkanBootstrapReport& report) const;

private:
	static String BoolText(bool value);
	static String StatusText(VulkanProbeStatus status);
	static String FormatVersion(uint32_t version);
	static String DeviceTypeText(VkPhysicalDeviceType type);
	static String QueueFlagsText(VkQueueFlags flags);
	static String LayerName(const VkLayerProperties& prop);
	static String ExtensionName(const VkExtensionProperties& prop);
	static uint32_t LayerVersionToUInt(const VkLayerProperties& prop);
	static uint32_t ExtensionVersionToUInt(const VkExtensionProperties& prop);
	static bool HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name);
	static bool IsSuitableDevice(const VulkanDeviceInfo& device);
	static int DeviceRank(VkPhysicalDeviceType type);
	static int QueueRank(const VulkanQueueFamilyInfo& family);
	static String SanitizeValidationMessage(const String& text);

	static VulkanPreflightReport BuildPreflight(bool request_validation, bool request_debug_utils, bool allow_validation, VulkanProcResolver resolver);
	static bool BuildBootstrap(VulkanBootstrapReport& report, bool request_validation, bool create_device, VulkanProcResolver resolver);
};

}
