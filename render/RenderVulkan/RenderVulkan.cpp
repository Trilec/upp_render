#include "RenderVulkan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Upp {

namespace {

struct VulkanLoader {
	HMODULE module = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
	PFN_vkEnumerateInstanceVersion enumerate_instance_version = nullptr;
	PFN_vkEnumerateInstanceLayerProperties enumerate_instance_layer_properties = nullptr;
	PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extension_properties = nullptr;
	PFN_vkCreateInstance create_instance = nullptr;
	PFN_vkDestroyInstance destroy_instance = nullptr;
	PFN_vkEnumeratePhysicalDevices enumerate_physical_devices = nullptr;
	PFN_vkGetPhysicalDeviceProperties get_physical_device_properties = nullptr;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties get_physical_device_queue_family_properties = nullptr;
	PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extension_properties = nullptr;
	PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2 = nullptr;

	void Reset()
	{
		get_instance_proc_addr = nullptr;
		enumerate_instance_version = nullptr;
		enumerate_instance_layer_properties = nullptr;
		enumerate_instance_extension_properties = nullptr;
		create_instance = nullptr;
		destroy_instance = nullptr;
		enumerate_physical_devices = nullptr;
		get_physical_device_properties = nullptr;
		get_physical_device_queue_family_properties = nullptr;
		enumerate_device_extension_properties = nullptr;
		get_physical_device_features2 = nullptr;
		if(module) {
			FreeLibrary(module);
			module = nullptr;
		}
	}
};

template <class T>
static bool LoadProc(T& fn, PFN_vkGetInstanceProcAddr gpa, VkInstance instance, const char *name)
{
	fn = reinterpret_cast<T>(gpa(instance, name));
	return fn != nullptr;
}

template <class T, class Enumerator>
static bool EnumerateResult(Vector<T>& out, Enumerator enumerator, const char *what, String& error)
{
	for(int attempt = 0; attempt < 8; ++attempt) {
		uint32_t count = 0;
		VkResult vr = enumerator(&count, nullptr);
		if(vr != VK_SUCCESS && vr != VK_INCOMPLETE) {
			error = String(what) + " count query failed: " + AsString((int)vr);
			return false;
		}

		out.SetCount((int)count);
		if(count == 0)
			return true;

		vr = enumerator(&count, out.Begin());
		if(vr == VK_SUCCESS) {
			out.SetCount((int)count);
			return true;
		}
		if(vr != VK_INCOMPLETE) {
			error = String(what) + " enumeration failed: " + AsString((int)vr);
			return false;
		}
	}

	error = String(what) + " enumeration kept returning VK_INCOMPLETE";
	return false;
}

template <class T, class Enumerator>
static bool EnumerateVoid(Vector<T>& out, Enumerator enumerator, const char *what, String& error)
{
	for(int attempt = 0; attempt < 8; ++attempt) {
		uint32_t count = 0;
		enumerator(&count, nullptr);
		out.SetCount((int)count);
		if(count == 0)
			return true;

		uint32_t requested = count;
		enumerator(&count, out.Begin());
		if(count == requested)
			return true;
	}

	error = String(what) + " enumeration kept changing";
	return false;
}

static String ApiResultText(VkResult vr)
{
	return AsString((int)vr);
}

} // namespace

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
	case VulkanProbeStatus::RuntimeUnavailable: return "runtime unavailable";
	case VulkanProbeStatus::RequiredLoaderFunctionUnavailable: return "required loader function unavailable";
	case VulkanProbeStatus::LoaderTooOld: return "loader api version older than Vulkan 1.3";
	case VulkanProbeStatus::LayerEnumerationFailed: return "layer enumeration failed";
	case VulkanProbeStatus::ExtensionEnumerationFailed: return "extension enumeration failed";
	case VulkanProbeStatus::ValidationUnavailable: return "validation layer unavailable";
	case VulkanProbeStatus::InstanceCreationFailed: return "instance creation failed";
	case VulkanProbeStatus::PhysicalDeviceEnumerationFailed: return "physical device enumeration failed";
	case VulkanProbeStatus::NoPhysicalDevices: return "no physical devices";
	case VulkanProbeStatus::NoSuitableDevices: return "no suitable devices";
	}
	return "unknown";
}

String VulkanPreflight::FormatVersion(uint32_t version)
{
	return AsString((int)VK_VERSION_MAJOR(version)) + "." + AsString((int)VK_VERSION_MINOR(version)) + "." + AsString((int)VK_VERSION_PATCH(version));
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

	VulkanLoader loader;
	VkInstance instance = VK_NULL_HANDLE;
	auto set_error = [&](VulkanProbeStatus status, const String& error) {
		switch(status) {
		case VulkanProbeStatus::RuntimeUnavailable: report.runtime_error = error; break;
		case VulkanProbeStatus::RequiredLoaderFunctionUnavailable:
		case VulkanProbeStatus::LoaderTooOld: report.loader_error = error; break;
		case VulkanProbeStatus::LayerEnumerationFailed: report.layer_error = error; break;
		case VulkanProbeStatus::ExtensionEnumerationFailed: report.extension_error = error; break;
		case VulkanProbeStatus::PhysicalDeviceEnumerationFailed: report.physical_device_error = error; break;
		case VulkanProbeStatus::InstanceCreationFailed: report.instance_error = error; break;
		case VulkanProbeStatus::ValidationUnavailable:
		case VulkanProbeStatus::NoPhysicalDevices:
		case VulkanProbeStatus::NoSuitableDevices:
		case VulkanProbeStatus::Ok:
			report.instance_error = error;
			break;
		}
	};
	auto fail = [&](VulkanProbeStatus status, const String& error, bool has_instance = false) {
		report.status = status;
		report.status_text = StatusText(status);
		if(!error.IsEmpty())
			set_error(status, error);
		bool clean = !has_instance || loader.destroy_instance != nullptr;
		if(has_instance && loader.destroy_instance && instance)
			loader.destroy_instance(instance, nullptr);
		loader.Reset();
		report.clean_shutdown = clean;
		return pick(report);
	};

	loader.module = LoadLibraryW(L"vulkan-1.dll");
	if(!loader.module)
		return fail(VulkanProbeStatus::RuntimeUnavailable, "LoadLibraryW(vulkan-1.dll) failed");

	loader.get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader.module, "vkGetInstanceProcAddr"));
	if(!loader.get_instance_proc_addr)
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkGetInstanceProcAddr unavailable");

	if(!LoadProc(loader.enumerate_instance_version, loader.get_instance_proc_addr, VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkEnumerateInstanceVersion unavailable");
	if(!LoadProc(loader.enumerate_instance_layer_properties, loader.get_instance_proc_addr, VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkEnumerateInstanceLayerProperties unavailable");
	if(!LoadProc(loader.enumerate_instance_extension_properties, loader.get_instance_proc_addr, VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkEnumerateInstanceExtensionProperties unavailable");
	if(!LoadProc(loader.create_instance, loader.get_instance_proc_addr, VK_NULL_HANDLE, "vkCreateInstance"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkCreateInstance unavailable");
	LoadProc(loader.destroy_instance, loader.get_instance_proc_addr, VK_NULL_HANDLE, "vkDestroyInstance");

	uint32_t loader_version = VK_API_VERSION_1_0;
	VkResult vr = loader.enumerate_instance_version(&loader_version);
	if(vr != VK_SUCCESS)
		return fail(VulkanProbeStatus::LoaderTooOld, String("vkEnumerateInstanceVersion failed: ") + ApiResultText(vr));
	report.loader_available = true;
	report.loader_version = loader_version;

	Vector<VkLayerProperties> layers;
	if(!EnumerateResult<VkLayerProperties>(layers,
		[&](uint32_t *count, VkLayerProperties *data) { return loader.enumerate_instance_layer_properties(count, data); },
		"instance layer", report.layer_error))
		return fail(VulkanProbeStatus::LayerEnumerationFailed, report.layer_error);
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

	Vector<VkExtensionProperties> exts;
	if(!EnumerateResult<VkExtensionProperties>(exts,
		[&](uint32_t *count, VkExtensionProperties *data) { return loader.enumerate_instance_extension_properties(nullptr, count, data); },
		"instance extension", report.extension_error))
		return fail(VulkanProbeStatus::ExtensionEnumerationFailed, report.extension_error);
	for(const auto& ext : exts) {
		VulkanExtensionInfo info;
		info.name = ExtensionName(ext);
		info.spec_version = ExtensionVersionToUInt(ext);
		report.instance_extensions.Add() = pick(info);
	}

	if(request_validation && !report.validation_available)
		return fail(VulkanProbeStatus::ValidationUnavailable, "VK_LAYER_KHRONOS_validation not present");

	if(loader_version < VK_API_VERSION_1_3)
		return fail(VulkanProbeStatus::LoaderTooOld, "loader reports Vulkan " + FormatVersion(loader_version) + ", need 1.3+");

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

	vr = loader.create_instance(&create_info, nullptr, &instance);
	if(vr != VK_SUCCESS)
		return fail(VulkanProbeStatus::InstanceCreationFailed, String("vkCreateInstance failed: ") + ApiResultText(vr));
	report.instance_created = true;

	if(!LoadProc(loader.destroy_instance, loader.get_instance_proc_addr, instance, "vkDestroyInstance"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkDestroyInstance unavailable", true);
	if(!LoadProc(loader.enumerate_physical_devices, loader.get_instance_proc_addr, instance, "vkEnumeratePhysicalDevices"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkEnumeratePhysicalDevices unavailable", true);
	if(!LoadProc(loader.get_physical_device_properties, loader.get_instance_proc_addr, instance, "vkGetPhysicalDeviceProperties"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkGetPhysicalDeviceProperties unavailable", true);
	if(!LoadProc(loader.get_physical_device_queue_family_properties, loader.get_instance_proc_addr, instance, "vkGetPhysicalDeviceQueueFamilyProperties"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkGetPhysicalDeviceQueueFamilyProperties unavailable", true);
	if(!LoadProc(loader.enumerate_device_extension_properties, loader.get_instance_proc_addr, instance, "vkEnumerateDeviceExtensionProperties"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkEnumerateDeviceExtensionProperties unavailable", true);
	if(!LoadProc(loader.get_physical_device_features2, loader.get_instance_proc_addr, instance, "vkGetPhysicalDeviceFeatures2"))
		return fail(VulkanProbeStatus::RequiredLoaderFunctionUnavailable, "vkGetPhysicalDeviceFeatures2 unavailable", true);

	Vector<VkPhysicalDevice> devices;
	if(!EnumerateResult<VkPhysicalDevice>(devices,
		[&](uint32_t *count, VkPhysicalDevice *data) { return loader.enumerate_physical_devices(instance, count, data); },
		"physical device", report.physical_device_error)) {
		return fail(VulkanProbeStatus::PhysicalDeviceEnumerationFailed, report.physical_device_error, true);
	}
	if(devices.IsEmpty())
		return fail(VulkanProbeStatus::NoPhysicalDevices, "vkEnumeratePhysicalDevices returned zero devices", true);

	for(VkPhysicalDevice device : devices) {
		VulkanDeviceInfo info;
		VkPhysicalDeviceProperties props{};
		loader.get_physical_device_properties(device, &props);
		info.name = props.deviceName;
		info.type = DeviceTypeText(props.deviceType);
		info.vendor_id = props.vendorID;
		info.device_id = props.deviceID;
		info.driver_version = props.driverVersion;
		info.api_version = props.apiVersion;

		Vector<VkQueueFamilyProperties> qprops;
		if(!EnumerateVoid<VkQueueFamilyProperties>(qprops,
			[&](uint32_t *count, VkQueueFamilyProperties *data) { loader.get_physical_device_queue_family_properties(device, count, data); },
			"queue family", report.physical_device_error)) {
			return fail(VulkanProbeStatus::PhysicalDeviceEnumerationFailed, report.physical_device_error, true);
		}

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

		Vector<VkExtensionProperties> dexts;
		if(!EnumerateResult<VkExtensionProperties>(dexts,
			[&](uint32_t *count, VkExtensionProperties *data) { return loader.enumerate_device_extension_properties(device, nullptr, count, data); },
			"device extension", report.extension_error)) {
			return fail(VulkanProbeStatus::ExtensionEnumerationFailed, report.extension_error, true);
		}

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
			f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			VkPhysicalDeviceFeatures2 f2{};
			f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			f2.pNext = &f13;
			loader.get_physical_device_features2(device, &f2);
			has_dynamic_rendering = f13.dynamicRendering == VK_TRUE;
			has_sync2 = f13.synchronization2 == VK_TRUE;
		}
		else {
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
				loader.get_physical_device_features2(device, &f2);
				has_dynamic_rendering = has_dyn_ext && dyn.dynamicRendering == VK_TRUE;
				has_sync2 = has_sync_ext && sync2.synchronization2 == VK_TRUE;
			}
		}

		info.dynamic_rendering = has_dynamic_rendering;
		info.synchronization2 = has_sync2;
		info.suitable = info.api_version >= VK_API_VERSION_1_3 && info.graphics_queue && info.dynamic_rendering && info.synchronization2;
		if(!info.suitable) {
			if(info.api_version < VK_API_VERSION_1_3)
				AppendMissing(info, "Vulkan 1.3");
			if(!info.graphics_queue)
				AppendMissing(info, "graphics queue");
			if(!info.dynamic_rendering)
				AppendMissing(info, "dynamic rendering");
			if(!info.synchronization2)
				AppendMissing(info, "synchronization2");
		}
		if(info.suitable)
			report.suitable_device_count += 1;

		report.devices.Add() = pick(info);
	}

	loader.destroy_instance(instance, nullptr);
	loader.Reset();
	report.instance_created = true;
	report.clean_shutdown = true;
	report.status = report.suitable_device_count > 0 ? VulkanProbeStatus::Ok : VulkanProbeStatus::NoSuitableDevices;
	report.status_text = StatusText(report.status);
	return pick(report);
}

String VulkanPreflight::Dump(const VulkanPreflightReport& report) const
{
	String out;
	out << "Vulkan preflight summary\n";
	out << "Status: " << report.status_text << '\n';
	out << "Loader/runtime available: " << BoolText(report.loader_available) << '\n';
	if(report.loader_available)
		out << "Loader API version: " << FormatVersion(report.loader_version) << '\n';
	out << "Validation layer available: " << BoolText(report.validation_available) << '\n';
	out << "Validation requested: " << BoolText(report.validation_requested) << '\n';
	out << "Instance created: " << BoolText(report.instance_created) << '\n';
	out << "Clean shutdown: " << BoolText(report.clean_shutdown) << '\n';
	if(!report.runtime_error.IsEmpty())
		out << "Runtime error: " << report.runtime_error << '\n';
	if(!report.loader_error.IsEmpty())
		out << "Loader error: " << report.loader_error << '\n';
	if(!report.layer_error.IsEmpty())
		out << "Layer error: " << report.layer_error << '\n';
	if(!report.extension_error.IsEmpty())
		out << "Extension error: " << report.extension_error << '\n';
	if(!report.physical_device_error.IsEmpty())
		out << "Physical device error: " << report.physical_device_error << '\n';
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
		for(const auto& q : d.queue_families)
			out << "  Queue family " << q.index << ": flags=" << QueueFlagsText(q.flags) << " count=" << AsString(q.count) << " graphics=" << BoolText(q.graphics) << " compute=" << BoolText(q.compute) << " transfer=" << BoolText(q.transfer) << '\n';
	}
	out << "VulkanProbe " << (report.status == VulkanProbeStatus::Ok ? "passed" : "failed") << '\n';
	return out;
}

}
