#include "RenderVulkan.h"

#include <vulkan/vulkan.h>

namespace Upp {

VulkanPreflight::VulkanPreflight()
{
}

String VulkanPreflight::BoolText(bool value)
{
	return value ? "yes" : "no";
}

String VulkanPreflight::StatusText(VulkanProbeStatus status)
{
	switch(status) {
	case VulkanProbeStatus::Ok: return "ok";
	case VulkanProbeStatus::LoaderUnavailable: return "loader unavailable";
	case VulkanProbeStatus::LoaderTooOld: return "loader api version older than Vulkan 1.3";
	case VulkanProbeStatus::ValidationUnavailable: return "validation layer unavailable";
	case VulkanProbeStatus::InstanceCreationFailed: return "instance creation failed";
	case VulkanProbeStatus::NoPhysicalDevices: return "no physical devices";
	case VulkanProbeStatus::NoSuitableDevices: return "no suitable devices";
	}
	return "unknown";
}

String VulkanPreflight::FormatVersion(uint32_t version)
{
	return AsString((int)VK_VERSION_MAJOR(version)) + "." + AsString((int)VK_VERSION_MINOR(version)) + "." + AsString((int)VK_VERSION_PATCH(version));
}

String VulkanPreflight::FormatDeviceType(String type)
{
	return type;
}

String VulkanPreflight::DeviceTypeText(VkPhysicalDeviceType type)
{
	switch(type) {
	case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "other";
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
	case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
	default: return "unknown";
	}
}

String VulkanPreflight::QueueFlagsText(VkQueueFlags flags)
{
	String out;
	if(flags & VK_QUEUE_GRAPHICS_BIT) out << "graphics";
	if(flags & VK_QUEUE_COMPUTE_BIT) { if(!out.IsEmpty()) out << '|'; out << "compute"; }
	if(flags & VK_QUEUE_TRANSFER_BIT) { if(!out.IsEmpty()) out << '|'; out << "transfer"; }
	if(flags & VK_QUEUE_SPARSE_BINDING_BIT) { if(!out.IsEmpty()) out << '|'; out << "sparse"; }
	if(flags & VK_QUEUE_PROTECTED_BIT) { if(!out.IsEmpty()) out << '|'; out << "protected"; }
	if(out.IsEmpty()) out = "none";
	return out;
}

bool VulkanPreflight::HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name)
{
	for(const auto& ext : extensions)
		if(ext.name == name)
			return true;
	return false;
}

bool VulkanPreflight::HasLayer(const Vector<VulkanLayerInfo>& layers, const char *name)
{
	for(const auto& layer : layers)
		if(layer.name == name)
			return true;
	return false;
}

void VulkanPreflight::AppendMissing(VulkanDeviceInfo& device, const char *text)
{
	device.missing_requirements.Add(text);
}

uint32_t VulkanPreflight::LayerVersionToUInt(const VkLayerProperties& prop)
{
	return prop.specVersion;
}

uint32_t VulkanPreflight::ExtensionVersionToUInt(const VkExtensionProperties& prop)
{
	return prop.specVersion;
}

String VulkanPreflight::LayerName(const VkLayerProperties& prop)
{
	return String(prop.layerName);
}

String VulkanPreflight::ExtensionName(const VkExtensionProperties& prop)
{
	return String(prop.extensionName);
}

VulkanPreflightReport VulkanPreflight::Run(bool request_validation)
{
	VulkanPreflightReport report;
	report.validation_requested = request_validation;

	uint32_t loader_version = VK_API_VERSION_1_0;
	VkResult vr = vkEnumerateInstanceVersion(&loader_version);
	if(vr != VK_SUCCESS) {
		report.status = VulkanProbeStatus::LoaderUnavailable;
		report.status_text = StatusText(report.status);
		return report;
	}

	report.loader_available = true;
	report.loader_version = loader_version;

	uint32_t layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	Vector<VkLayerProperties> layers(layer_count);
	if(layer_count)
		vkEnumerateInstanceLayerProperties(&layer_count, layers.Begin());
	for(const auto& layer : layers) {
		VulkanLayerInfo info;
		info.name = LayerName(layer);
		info.description = layer.description;
		info.spec_version = LayerVersionToUInt(layer);
		report.instance_layers.Add() = pick(info);
	}
	for(const auto& layer : report.instance_layers)
		if(layer.name == "VK_LAYER_KHRONOS_validation")
			report.validation_available = true;

	uint32_t ext_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
	Vector<VkExtensionProperties> exts(ext_count);
	if(ext_count)
		vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.Begin());
	for(const auto& ext : exts) {
		VulkanExtensionInfo info;
		info.name = ExtensionName(ext);
		info.spec_version = ExtensionVersionToUInt(ext);
		report.instance_extensions.Add() = pick(info);
	}

	if(request_validation && !report.validation_available) {
		report.status = VulkanProbeStatus::ValidationUnavailable;
		report.status_text = StatusText(report.status);
		return report;
	}

	if(loader_version < VK_API_VERSION_1_3) {
		report.status = VulkanProbeStatus::LoaderTooOld;
		report.status_text = StatusText(report.status);
		return report;
	}

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "VulkanProbe";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "upp_render";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_3;

	Vector<const char*> enabled_layers;
	if(request_validation)
		enabled_layers.Add("VK_LAYER_KHRONOS_validation");

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledLayerCount = enabled_layers.GetCount();
	create_info.ppEnabledLayerNames = enabled_layers.IsEmpty() ? nullptr : enabled_layers.Begin();

	VkInstance instance = VK_NULL_HANDLE;
	vr = vkCreateInstance(&create_info, nullptr, &instance);
	if(vr != VK_SUCCESS) {
		report.status = VulkanProbeStatus::InstanceCreationFailed;
		report.instance_error = String("vkCreateInstance failed: ") + AsString((int)vr);
		report.status_text = StatusText(report.status);
		return report;
	}
	report.instance_created = true;

	uint32_t device_count = 0;
	vr = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	if(vr != VK_SUCCESS || device_count == 0) {
		vkDestroyInstance(instance, nullptr);
		report.status = VulkanProbeStatus::NoPhysicalDevices;
		report.status_text = StatusText(report.status);
		return report;
	}

	Vector<VkPhysicalDevice> devices(device_count);
	vr = vkEnumeratePhysicalDevices(instance, &device_count, devices.Begin());
	if(vr != VK_SUCCESS) {
		vkDestroyInstance(instance, nullptr);
		report.status = VulkanProbeStatus::NoPhysicalDevices;
		report.instance_error = String("vkEnumeratePhysicalDevices failed: ") + AsString((int)vr);
		report.status_text = StatusText(report.status);
		return report;
	}

	for(VkPhysicalDevice device : devices) {
		VulkanDeviceInfo info;
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(device, &props);
		info.name = props.deviceName;
		info.type = DeviceTypeText(props.deviceType);
		info.vendor_id = props.vendorID;
		info.device_id = props.deviceID;
		info.driver_version = props.driverVersion;
		info.api_version = props.apiVersion;

		uint32_t qcount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &qcount, nullptr);
		Vector<VkQueueFamilyProperties> qprops(qcount);
		if(qcount)
			vkGetPhysicalDeviceQueueFamilyProperties(device, &qcount, qprops.Begin());
		for(int i = 0; i < qprops.GetCount(); ++i) {
			const VkQueueFamilyProperties& q = qprops[i];
			VulkanQueueFamilyInfo qinfo;
			qinfo.index = i;
			qinfo.flags = q.queueFlags;
			qinfo.count = q.queueCount;
			qinfo.graphics = (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			qinfo.compute = (q.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
			qinfo.transfer = (q.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
			qinfo.sparse_binding = (q.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0;
			info.queue_families.Add() = pick(qinfo);
			if(qinfo.graphics)
				info.graphics_queue = true;
		}

		uint32_t dext_count = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &dext_count, nullptr);
		Vector<VkExtensionProperties> dexts(dext_count);
		if(dext_count)
			vkEnumerateDeviceExtensionProperties(device, nullptr, &dext_count, dexts.Begin());

		for(const auto& ext : dexts) {
			VulkanExtensionInfo einfo;
			einfo.name = ExtensionName(ext);
			einfo.spec_version = ExtensionVersionToUInt(ext);
			info.device_extensions.Add() = pick(einfo);
		}

		bool has_dynamic_rendering = false;
		bool has_sync2 = false;
		if(props.apiVersion >= VK_API_VERSION_1_3) {
			VkPhysicalDeviceVulkan13Features f13{};
			VkPhysicalDeviceFeatures2 f2{};
			f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			f2.pNext = &f13;
			vkGetPhysicalDeviceFeatures2(device, &f2);
			has_dynamic_rendering = f13.dynamicRendering == VK_TRUE;
			has_sync2 = f13.synchronization2 == VK_TRUE;
		} else {
			bool has_dyn_ext = HasExtension(info.device_extensions, "VK_KHR_dynamic_rendering");
			bool has_sync_ext = HasExtension(info.device_extensions, "VK_KHR_synchronization2");
			if(has_dyn_ext || has_sync_ext) {
				VkPhysicalDeviceFeatures2 f2{};
				VkPhysicalDeviceDynamicRenderingFeaturesKHR dyn{};
				VkPhysicalDeviceSynchronization2FeaturesKHR sync2{};
				f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				dyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
				sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
				f2.pNext = &dyn;
				dyn.pNext = &sync2;
				vkGetPhysicalDeviceFeatures2(device, &f2);
				has_dynamic_rendering = has_dyn_ext && dyn.dynamicRendering == VK_TRUE;
				has_sync2 = has_sync_ext && sync2.synchronization2 == VK_TRUE;
			}
		}
		info.dynamic_rendering = has_dynamic_rendering;
		info.synchronization2 = has_sync2;

		if(props.apiVersion < VK_API_VERSION_1_3)
			AppendMissing(info, "Vulkan 1.3");
		if(!info.graphics_queue)
			AppendMissing(info, "graphics queue");
		if(!info.dynamic_rendering)
			AppendMissing(info, "dynamic rendering");
		if(!info.synchronization2)
			AppendMissing(info, "synchronization2");
		info.suitable = info.missing_requirements.IsEmpty() && props.apiVersion >= VK_API_VERSION_1_3;
		if(!info.suitable && props.apiVersion >= VK_API_VERSION_1_3 && info.graphics_queue && info.dynamic_rendering && info.synchronization2)
			info.suitable = true;
		if(info.suitable)
			report.suitable_device_count += 1;

		report.devices.Add() = pick(info);
	}

	vkDestroyInstance(instance, nullptr);
	report.status = report.devices.IsEmpty() ? VulkanProbeStatus::NoPhysicalDevices : (report.suitable_device_count > 0 ? VulkanProbeStatus::Ok : VulkanProbeStatus::NoSuitableDevices);
	report.status_text = StatusText(report.status);
	return report;
}

String VulkanPreflight::Dump(const VulkanPreflightReport& report) const
{
	String out;
	out << "Vulkan preflight summary\n";
	out << "Status: " << report.status_text << '\n';
	out << "Loader available: " << BoolText(report.loader_available) << '\n';
	if(report.loader_available)
		out << "Loader version: " << FormatVersion(report.loader_version) << '\n';
	out << "Validation layer available: " << BoolText(report.validation_available) << '\n';
	out << "Validation requested: " << BoolText(report.validation_requested) << '\n';
	out << "Instance created: " << BoolText(report.instance_created) << '\n';
	if(!report.instance_error.IsEmpty())
		out << "Instance error: " << report.instance_error << '\n';
	out << "Instance layers: " << report.instance_layers.GetCount() << '\n';
	for(const auto& layer : report.instance_layers)
		out << "  " << layer.name << " spec=" << FormatVersion(layer.spec_version) << " desc=" << layer.description << '\n';
	out << "Instance extensions: " << report.instance_extensions.GetCount() << '\n';
	for(const auto& ext : report.instance_extensions)
		out << "  " << ext.name << " spec=" << FormatVersion(ext.spec_version) << '\n';
	out << "Physical devices: " << report.devices.GetCount() << '\n';
	out << "Suitable Vulkan 1.3 devices: " << report.suitable_device_count << '\n';
	for(int i = 0; i < report.devices.GetCount(); ++i) {
		const VulkanDeviceInfo& d = report.devices[i];
		out << "Device " << i << ": " << d.name << '\n';
		out << "  Type: " << d.type << '\n';
		out << "  Vendor ID: " << AsString(d.vendor_id) << '\n';
		out << "  Device ID: " << AsString(d.device_id) << '\n';
		out << "  Driver version: " << AsString(d.driver_version) << '\n';
		out << "  API version: " << FormatVersion(d.api_version) << '\n';
		out << "  Graphics queue: " << BoolText(d.graphics_queue) << '\n';
		out << "  Dynamic Rendering: " << BoolText(d.dynamic_rendering) << '\n';
		out << "  Synchronization2: " << BoolText(d.synchronization2) << '\n';
		out << "  Suitable: " << BoolText(d.suitable) << '\n';
		if(!d.missing_requirements.IsEmpty())
			out << "  Missing: " << Join(d.missing_requirements, ", ") << '\n';
		out << "  Device extensions: " << d.device_extensions.GetCount() << '\n';
		for(const auto& ext : d.device_extensions)
			out << "    " << ext.name << " spec=" << FormatVersion(ext.spec_version) << '\n';
		for(const auto& q : d.queue_families) {
			out << "  Queue family " << q.index << ": flags=" << QueueFlagsText(q.flags) << " count=" << AsString(q.count) << " graphics=" << BoolText(q.graphics) << " compute=" << BoolText(q.compute) << " transfer=" << BoolText(q.transfer) << '\n';
		}
	}
	out << "VulkanProbe " << (report.status == VulkanProbeStatus::Ok ? "passed" : "failed") << '\n';
	return out;
}

}
