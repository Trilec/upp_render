#pragma once

#include <Core/Core.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace Upp {

enum class VulkanProbeStatus {
	Ok,
	RuntimeUnavailable,
	RequiredLoaderFunctionUnavailable,
	LoaderTooOld,
	LayerEnumerationFailed,
	ExtensionEnumerationFailed,
	ValidationUnavailable,
	InstanceCreationFailed,
	PhysicalDeviceEnumerationFailed,
	NoPhysicalDevices,
	NoSuitableDevices,
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
	bool instance_created = false;
	bool clean_shutdown = false;
	String runtime_error;
	String loader_error;
	String layer_error;
	String extension_error;
	String physical_device_error;
	String instance_error;
	Vector<VulkanLayerInfo> instance_layers;
	Vector<VulkanExtensionInfo> instance_extensions;
	Vector<VulkanDeviceInfo> devices;
	int suitable_device_count = 0;
};

class VulkanPreflight {
public:
	VulkanPreflight();

	VulkanPreflightReport Run(bool request_validation);
	String Dump(const VulkanPreflightReport& report) const;

	static String FormatVersion(uint32_t version);

private:
	static String BoolText(bool value);
	static String StatusText(VulkanProbeStatus status);
	static String ExtensionName(const VkExtensionProperties& prop);
	static String LayerName(const VkLayerProperties& prop);
	static String DeviceTypeText(VkPhysicalDeviceType type);
	static String QueueFlagsText(VkQueueFlags flags);
	static bool HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name);
	static void AppendMissing(VulkanDeviceInfo& device, const char *text);
	static uint32_t LayerVersionToUInt(const VkLayerProperties& prop);
	static uint32_t ExtensionVersionToUInt(const VkExtensionProperties& prop);
};

}
