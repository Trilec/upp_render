#include "RenderVulkan.h"
#include "RenderVulkanSurfaceSession.h"
#include "RenderVulkanTestHooks.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan_win32.h>

namespace Upp {

namespace {

using VulkanValidationTestInjection = VulkanTestHooks::VulkanValidationTestInjection;
using VulkanValidationTestPoint = VulkanTestHooks::VulkanValidationTestPoint;

struct VulkanRuntimeDeviceDiagnosticsState {
	std::atomic<uint64_t> runtime_create_count { 0 };
	std::atomic<uint64_t> runtime_live_count { 0 };
	std::atomic<uint64_t> runtime_next_id { 1 };
	std::atomic<uint64_t> runtime_last_id { 0 };
	std::atomic<uint64_t> instance_next_id { 1 };
	std::atomic<uint64_t> instance_last_id { 0 };
	std::atomic<uint64_t> instance_create_count { 0 };
	std::atomic<uint64_t> instance_live_count { 0 };
	std::atomic<uint64_t> debug_messenger_create_count { 0 };
	std::atomic<uint64_t> debug_messenger_live_count { 0 };
	std::atomic<uint64_t> physical_device_discovery_count { 0 };
	std::atomic<uint64_t> device_create_count { 0 };
	std::atomic<uint64_t> device_live_count { 0 };
	std::atomic<uint64_t> device_next_id { 1 };
	std::atomic<uint64_t> device_last_id { 0 };
	std::atomic<uint64_t> surface_create_count { 0 };
	std::atomic<uint64_t> surface_live_count { 0 };
	std::atomic<uint64_t> surface_next_id { 1 };
	std::atomic<uint64_t> surface_last_id { 0 };
};

static VulkanRuntimeDeviceDiagnosticsState g_runtime_device_stats;

static uint64_t NextDiagnosticId(std::atomic<uint64_t>& counter)
{
	return counter.fetch_add(1, std::memory_order_relaxed);
}

static String BoolText(bool value)
{
	return value ? "yes" : "no";
}

static String FormatVersion(uint32_t version)
{
	return AsString((int)VK_VERSION_MAJOR(version)) + "." + AsString((int)VK_VERSION_MINOR(version)) + "." + AsString((int)VK_VERSION_PATCH(version));
}

static String DeviceTypeText(VkPhysicalDeviceType type)
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

static String QueueFlagsText(VkQueueFlags flags)
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

static String DumpFlags(int flags, const char *const *names, const int *bits, int count)
{
	if(flags == 0)
		return "None";
	String out;
	for(int i = 0; i < count; ++i) {
		if(flags & bits[i]) {
			if(!out.IsEmpty())
				out << '|';
			out << names[i];
		}
	}
	if(out.IsEmpty())
		out = AsString(flags);
	return out;
}

static String SurfaceFormatText(VkFormat format);
static String ColorSpaceText(VkColorSpaceKHR color_space);
static String PresentModeText(VkPresentModeKHR mode);
static String SurfaceTransformText(VkSurfaceTransformFlagsKHR flags);
static String CompositeAlphaText(VkCompositeAlphaFlagsKHR flags);
static String ImageUsageText(VkImageUsageFlags flags);

static String LayerName(const VkLayerProperties& prop)
{
	return String(prop.layerName);
}

static String ExtensionName(const VkExtensionProperties& prop)
{
	return String(prop.extensionName);
}

static uint32_t LayerVersionToUInt(const VkLayerProperties& prop)
{
	return prop.specVersion;
}

static uint32_t ExtensionVersionToUInt(const VkExtensionProperties& prop)
{
	return prop.specVersion;
}

static bool HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name)
{
	for(const auto& ext : extensions)
		if(ext.name == name)
			return true;
	return false;
}

static void CloneExtensionInfo(VulkanExtensionInfo& dst, const VulkanExtensionInfo& src)
{
	dst.name = src.name;
	dst.spec_version = src.spec_version;
}

static void CloneQueueFamilyInfo(VulkanQueueFamilyInfo& dst, const VulkanQueueFamilyInfo& src)
{
	dst.index = src.index;
	dst.flags = src.flags;
	dst.count = src.count;
	dst.graphics = src.graphics;
	dst.present = src.present;
	dst.compute = src.compute;
	dst.transfer = src.transfer;
	dst.sparse_binding = src.sparse_binding;
}

static void CloneDeviceInfo(VulkanDeviceInfo& dst, const VulkanDeviceInfo& src)
{
	dst.name = src.name;
	dst.type = src.type;
	dst.vendor_id = src.vendor_id;
	dst.device_id = src.device_id;
	dst.driver_version = src.driver_version;
	dst.api_version = src.api_version;
	dst.graphics_queue = src.graphics_queue;
	dst.dynamic_rendering = src.dynamic_rendering;
	dst.synchronization2 = src.synchronization2;
	dst.suitable = src.suitable;
	dst.selection_reason = src.selection_reason;
	dst.selected_queue_family_index = src.selected_queue_family_index;
	dst.selected_queue_count = src.selected_queue_count;
	dst.selected_queue_flags = src.selected_queue_flags;
	dst.selected_queue_compute = src.selected_queue_compute;
	dst.selected_queue_transfer = src.selected_queue_transfer;
	dst.logical_device_created = src.logical_device_created;
	dst.graphics_queue_acquired = src.graphics_queue_acquired;
	dst.missing_requirements.Clear();
	for(const auto& s : src.missing_requirements)
		dst.missing_requirements.Add(s);
	dst.queue_families.Clear();
	for(const auto& q : src.queue_families) {
		dst.queue_families.Add();
		CloneQueueFamilyInfo(dst.queue_families.Top(), q);
	}
	dst.device_extensions.Clear();
	for(const auto& e : src.device_extensions) {
		dst.device_extensions.Add();
		CloneExtensionInfo(dst.device_extensions.Top(), e);
	}
}

static void AppendMissing(VulkanDeviceInfo& device, const char *text)
{
	device.missing_requirements.Add(text);
}

static String SanitizeValidationMessage(String text)
{
	for(int i = 0; i + 2 < text.GetCount(); ++i) {
		if(text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
			int j = i + 2;
			while(j < text.GetCount()) {
				char c = text[j];
				bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
				if(!hex)
					break;
				++j;
			}
			if(j > i + 2) {
				text.Remove(i + 2, j - (i + 2));
				text.Insert(i + 2, "...");
				i += 4;
			}
		}
	}
	if(text.GetCount() > 220)
		text = text.Left(220) + "...";
	return text;
}

template <class T, class Fn>
static bool EnumerateResult(Vector<T>& out, Fn fn, String& error, const char *what)
{
	for(int attempt = 0; attempt < 8; ++attempt) {
		uint32_t count = 0;
		VkResult vr = fn(&count, nullptr);
		if(vr != VK_SUCCESS && vr != VK_INCOMPLETE) {
			error = String(what) + " count query failed: " + AsString((int)vr);
			return false;
		}
		if(count == 0) {
			out.Clear();
			return true;
		}

		Vector<T> tmp;
		tmp.SetCount((int)count);
		uint32_t requested = count;
		vr = fn(&requested, tmp.Begin());
		if(vr == VK_SUCCESS && requested == count) {
			out = pick(tmp);
			return true;
		}
		if(vr != VK_INCOMPLETE && requested == count) {
			error = String(what) + " enumeration failed: " + AsString((int)vr);
			return false;
		}
	}
	error = String(what) + " enumeration kept returning VK_INCOMPLETE";
	return false;
}

template <class Fn>
static bool EnumerateQueueFamilies(Vector<VkQueueFamilyProperties>& out, Fn fn, String& error, const char *what)
{
	for(int attempt = 0; attempt < 8; ++attempt) {
		uint32_t count = 0;
		fn(&count, nullptr);
		if(count == 0) {
			out.Clear();
			return true;
		}

		Vector<VkQueueFamilyProperties> tmp;
		tmp.SetCount((int)count);
		uint32_t requested = count;
		fn(&requested, tmp.Begin());
		if(requested == count) {
			out = pick(tmp);
			return true;
		}
	}
	error = String(what) + " enumeration kept changing";
	return false;
}

struct VulkanValidationCapture {
	int warnings = 0;
	int errors = 0;
	Vector<String> messages;
};

static void CopyValidationCapture(VulkanPreflightReport& report, const VulkanValidationCapture& capture)
{
	report.validation_warning_count = capture.warnings;
	report.validation_error_count = capture.errors;
	report.validation_messages.Clear();
	for(const String& msg : capture.messages)
		report.validation_messages.Add(msg);
}

static void CopyValidationCapture(VulkanBootstrapReport& report, const VulkanValidationCapture& capture)
{
	report.validation_warning_count = capture.warnings;
	report.validation_error_count = capture.errors;
	report.validation_messages.Clear();
	for(const String& msg : capture.messages)
		report.validation_messages.Add(msg);
}

static void CopyMessages(Vector<String>& dst, const Vector<String>& src)
{
	dst.Clear();
	for(const String& msg : src)
		dst.Add(msg);
}

static VulkanValidationTestInjection g_validation_test_injection;

static void InjectValidationIfRequested(VulkanValidationCapture& capture, VulkanValidationTestPoint point)
{
	if(!g_validation_test_injection.enabled || g_validation_test_injection.point != point)
		return;
	if(g_validation_test_injection.error)
		capture.errors++;
	else
		capture.warnings++;
		
	if(!g_validation_test_injection.message.IsEmpty())
		capture.messages.Add(g_validation_test_injection.message);
	else
		capture.messages.Add(g_validation_test_injection.error ? String("synthetic validation error") : String("synthetic validation warning"));
	g_validation_test_injection.enabled = false;
}

template <class T>
static bool ResolveInstanceProc(T& out, VulkanProcResolver filter, PFN_vkGetInstanceProcAddr get_proc, VkInstance instance, const char *name, String& error)
{
	if(filter && !filter(nullptr, name)) {
		error = name;
		return false;
	}
	out = reinterpret_cast<T>(get_proc(instance, name));
	if(!out) {
		error = name;
		return false;
	}
	return true;
}

template <class T>
static bool ResolveDeviceProc(T& out, VulkanProcResolver filter, PFN_vkGetDeviceProcAddr get_proc, VkDevice device, const char *name, String& error)
{
	if(filter && !filter(nullptr, name)) {
		error = name;
		return false;
	}
	out = reinterpret_cast<T>(get_proc(device, name));
	if(!out) {
		error = name;
		return false;
	}
	return true;
}

template <class T>
static bool ResolveGlobalProc(T& out, VulkanProcResolver filter, HMODULE module, const char *name, String& error)
{
	if(filter && !filter(module, name)) {
		error = name;
		return false;
	}
	out = reinterpret_cast<T>(GetProcAddress(module, name));
	if(!out) {
		error = name;
		return false;
	}
	return true;
}

static bool QueryLoaderVersion(PFN_vkEnumerateInstanceVersion enumerate_instance_version, uint32_t& version, String& error)
{
	version = VK_API_VERSION_1_0;
	if(!enumerate_instance_version)
		return true;
	VkResult vr = enumerate_instance_version(&version);
	if(vr != VK_SUCCESS) {
		error = String("vkEnumerateInstanceVersion failed: ") + AsString((int)vr);
		return false;
	}
	return true;
}

struct VulkanDiscoveredDevice : Moveable<VulkanDiscoveredDevice> {
	VkPhysicalDevice handle = VK_NULL_HANDLE;
	VulkanDeviceInfo info;
};

static int DeviceRank(const String& type);
static int QueueRank(const VulkanQueueFamilyInfo& family);

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data)
{
	auto& capture = *reinterpret_cast<VulkanValidationCapture*>(user_data);
	if(!(severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)))
		return VK_FALSE;

	String msg;
	if(data && data->pMessageIdName && *data->pMessageIdName)
		msg << data->pMessageIdName << ": ";
	if(data && data->pMessage)
		msg << SanitizeValidationMessage(data->pMessage);
	if(msg.IsEmpty())
		msg = "validation message";

	if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		capture.errors++;
	else if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		capture.warnings++;

	capture.messages.Add(pick(msg));
	return VK_FALSE;
}

struct VulkanDispatch {
	HMODULE module = nullptr;
	VulkanProcResolver proc_filter = nullptr;
	PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
	PFN_vkEnumerateInstanceVersion enumerate_instance_version = nullptr;
	PFN_vkEnumerateInstanceLayerProperties enumerate_instance_layer_properties = nullptr;
	PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extension_properties = nullptr;
	PFN_vkCreateInstance create_instance = nullptr;
	bool cleanup_ok = true;
	uint64_t diagnostic_id = 0;

	~VulkanDispatch() { Close(); }

	bool IsCleared() const
	{
		return module == nullptr && get_instance_proc_addr == nullptr && enumerate_instance_version == nullptr && enumerate_instance_layer_properties == nullptr && enumerate_instance_extension_properties == nullptr && create_instance == nullptr;
	}

	bool Close()
	{
		bool ok = true;
		proc_filter = nullptr;
		get_instance_proc_addr = nullptr;
		enumerate_instance_version = nullptr;
		enumerate_instance_layer_properties = nullptr;
		enumerate_instance_extension_properties = nullptr;
		create_instance = nullptr;
		if(module) {
			if(!FreeLibrary(module))
				ok = false;
			module = nullptr;
		}
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.runtime_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		return cleanup_ok;
	}

	bool Open(String& error, VulkanProcResolver resolver = &GetProcAddress)
	{
		Close();
		cleanup_ok = true;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.runtime_next_id);
		g_runtime_device_stats.runtime_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.runtime_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.runtime_last_id.store(diagnostic_id, std::memory_order_relaxed);
		proc_filter = resolver;
		module = LoadLibraryW(L"vulkan-1.dll");
		if(!module) {
			error = "LoadLibraryW(vulkan-1.dll) failed";
			return false;
		}
		if(!ResolveGlobalProc(get_instance_proc_addr, proc_filter, module, "vkGetInstanceProcAddr", error)) {
			Close();
			return false;
		}
		enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(GetProcAddress(module, "vkEnumerateInstanceVersion"));
		if(!ResolveGlobalProc(enumerate_instance_layer_properties, proc_filter, module, "vkEnumerateInstanceLayerProperties", error)) {
			Close();
			return false;
		}
		if(!ResolveGlobalProc(enumerate_instance_extension_properties, proc_filter, module, "vkEnumerateInstanceExtensionProperties", error)) {
			Close();
			return false;
		}
		if(!ResolveGlobalProc(create_instance, proc_filter, module, "vkCreateInstance", error)) {
			Close();
			return false;
		}
		return true;
	}
};

struct VulkanInstanceContext {
	const VulkanDispatch *dispatch = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	PFN_vkDestroyInstance destroy_instance = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	PFN_vkCreateDevice create_device = nullptr;
	PFN_vkEnumeratePhysicalDevices enumerate_physical_devices = nullptr;
	PFN_vkGetPhysicalDeviceProperties get_physical_device_properties = nullptr;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties get_physical_device_queue_family_properties = nullptr;
	PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extension_properties = nullptr;
	PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2 = nullptr;
	PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger = nullptr;
	VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
	VulkanValidationCapture capture;
	bool validation_requested = false;
	bool debug_utils_requested = false;
	bool debug_utils_available = false;
	bool cleanup_ok = true;
	uint64_t diagnostic_id = 0;

	~VulkanInstanceContext() { Close(); }

	bool IsCleared() const
	{
		return dispatch == nullptr && instance == VK_NULL_HANDLE && destroy_instance == nullptr && get_device_proc_addr == nullptr && create_device == nullptr && enumerate_physical_devices == nullptr && get_physical_device_properties == nullptr && get_physical_device_queue_family_properties == nullptr && enumerate_device_extension_properties == nullptr && get_physical_device_features2 == nullptr && create_debug_utils_messenger == nullptr && destroy_debug_utils_messenger == nullptr && messenger == VK_NULL_HANDLE;
	}

	bool Close()
	{
		bool ok = true;
		if(messenger && destroy_debug_utils_messenger) {
			destroy_debug_utils_messenger(instance, messenger, nullptr);
			messenger = VK_NULL_HANDLE;
			g_runtime_device_stats.debug_messenger_live_count.fetch_sub(1, std::memory_order_relaxed);
		}
		else if(messenger) {
			ok = false;
			messenger = VK_NULL_HANDLE;
		}
		if(instance && destroy_instance)
			destroy_instance(instance, nullptr);
		else if(instance)
			ok = false;
		instance = VK_NULL_HANDLE;
		destroy_instance = nullptr;
		get_device_proc_addr = nullptr;
		create_device = nullptr;
		enumerate_physical_devices = nullptr;
		get_physical_device_properties = nullptr;
		get_physical_device_queue_family_properties = nullptr;
		enumerate_device_extension_properties = nullptr;
		get_physical_device_features2 = nullptr;
		create_debug_utils_messenger = nullptr;
		destroy_debug_utils_messenger = nullptr;
		capture = VulkanValidationCapture();
		dispatch = nullptr;
		validation_requested = false;
		debug_utils_requested = false;
		debug_utils_available = false;
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.instance_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		return cleanup_ok;
	}

	bool Open(const VulkanDispatch& d, bool request_validation, VulkanPreflightReport& preflight, VulkanBootstrapReport& bootstrap, String& error)
	{
		Close();
		cleanup_ok = true;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.instance_next_id);
		g_runtime_device_stats.instance_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_last_id.store(diagnostic_id, std::memory_order_relaxed);
		dispatch = &d;
		validation_requested = request_validation;
		auto fail = [&](const String& message) {
			error = message;
			Close();
			return false;
		};
		VkResult vr = VK_SUCCESS;

		Vector<VkExtensionProperties> instance_exts;
		if(!EnumerateResult(instance_exts, [&](uint32_t *count, VkExtensionProperties *data) { return d.enumerate_instance_extension_properties(nullptr, count, data); }, error, "instance extension"))
			return fail(error);
		for(const auto& ext : instance_exts) {
			VulkanExtensionInfo info;
			info.name = ExtensionName(ext);
			info.spec_version = ExtensionVersionToUInt(ext);
			preflight.instance_extensions.Add() = pick(info);
		}
		preflight.debug_utils_available = HasExtension(preflight.instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		Vector<VkLayerProperties> layers;
		if(!EnumerateResult(layers, [&](uint32_t *count, VkLayerProperties *data) { return d.enumerate_instance_layer_properties(count, data); }, error, "instance layer"))
			return fail(error);
		for(const auto& layer : layers) {
			VulkanLayerInfo info;
			info.name = LayerName(layer);
			info.description = layer.description;
			info.spec_version = LayerVersionToUInt(layer);
			preflight.instance_layers.Add() = pick(info);
		}
		preflight.validation_available = false;
		for(const auto& layer : preflight.instance_layers)
			if(layer.name == "VK_LAYER_KHRONOS_validation")
				preflight.validation_available = true;

		if(request_validation && !preflight.validation_available) {
			error = "VK_LAYER_KHRONOS_validation not present";
			return false;
		}
		if(request_validation && !preflight.debug_utils_available) {
			error = "VK_EXT_debug_utils not present";
			return fail(error);
		}

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "VulkanBootstrap";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "upp_render";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_3;

		Vector<const char*> enabled_layers;
		Vector<const char*> enabled_exts;
		if(request_validation) {
			enabled_layers.Add("VK_LAYER_KHRONOS_validation");
			enabled_exts.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledLayerCount = enabled_layers.GetCount();
		create_info.ppEnabledLayerNames = enabled_layers.IsEmpty() ? nullptr : enabled_layers.Begin();
		create_info.enabledExtensionCount = enabled_exts.GetCount();
		create_info.ppEnabledExtensionNames = enabled_exts.IsEmpty() ? nullptr : enabled_exts.Begin();

		vr = d.create_instance(&create_info, nullptr, &instance);
		if(vr != VK_SUCCESS) {
			error = String("vkCreateInstance failed: ") + AsString((int)vr);
			return fail(error);
		}
		preflight.instance_created = true;

		if(!ResolveInstanceProc(destroy_instance, d.proc_filter, d.get_instance_proc_addr, instance, "vkDestroyInstance", error)) return fail(error);
		if(!ResolveInstanceProc(get_device_proc_addr, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetDeviceProcAddr", error)) return fail(error);
		if(!ResolveInstanceProc(create_device, d.proc_filter, d.get_instance_proc_addr, instance, "vkCreateDevice", error)) return fail(error);
		if(!ResolveInstanceProc(enumerate_physical_devices, d.proc_filter, d.get_instance_proc_addr, instance, "vkEnumeratePhysicalDevices", error)) return fail(error);
		if(!ResolveInstanceProc(get_physical_device_properties, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceProperties", error)) return fail(error);
		if(!ResolveInstanceProc(get_physical_device_queue_family_properties, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceQueueFamilyProperties", error)) return fail(error);
		if(!ResolveInstanceProc(enumerate_device_extension_properties, d.proc_filter, d.get_instance_proc_addr, instance, "vkEnumerateDeviceExtensionProperties", error)) return fail(error);
		if(!ResolveInstanceProc(get_physical_device_features2, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceFeatures2", error)) return fail(error);

		if(request_validation) {
			if(!ResolveInstanceProc(create_debug_utils_messenger, d.proc_filter, d.get_instance_proc_addr, instance, "vkCreateDebugUtilsMessengerEXT", error)) return fail(error);
			if(!ResolveInstanceProc(destroy_debug_utils_messenger, d.proc_filter, d.get_instance_proc_addr, instance, "vkDestroyDebugUtilsMessengerEXT", error)) return fail(error);

			VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
			messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			messenger_info.pfnUserCallback = VulkanDebugCallback;
			messenger_info.pUserData = &capture;
			vr = create_debug_utils_messenger(instance, &messenger_info, nullptr, &messenger);
			if(vr != VK_SUCCESS) {
				error = String("vkCreateDebugUtilsMessengerEXT failed: ") + AsString((int)vr);
				return fail(error);
			}
			g_runtime_device_stats.debug_messenger_create_count.fetch_add(1, std::memory_order_relaxed);
			g_runtime_device_stats.debug_messenger_live_count.fetch_add(1, std::memory_order_relaxed);
			debug_utils_requested = true;
			debug_utils_available = true;
			bootstrap.debug_messenger_created = true;
		}

		return true;
	}

	bool EnumeratePhysicalDevices(Vector<VulkanDiscoveredDevice>& out, String& error) const
	{
		g_runtime_device_stats.physical_device_discovery_count.fetch_add(1, std::memory_order_relaxed);
		Vector<VkPhysicalDevice> handles;
		if(!EnumerateResult(handles, [&](uint32_t *count, VkPhysicalDevice *data) { return enumerate_physical_devices(instance, count, data); }, error, "physical device"))
			return false;
		if(handles.IsEmpty())
			return true;

		for(VkPhysicalDevice handle : handles) {
			VulkanDiscoveredDevice device;
			device.handle = handle;
			VulkanDeviceInfo& info = device.info;
			VkPhysicalDeviceProperties props{};
			get_physical_device_properties(handle, &props);
			info.name = props.deviceName;
			info.type = DeviceTypeText(props.deviceType);
			info.vendor_id = props.vendorID;
			info.device_id = props.deviceID;
			info.driver_version = props.driverVersion;
			info.api_version = props.apiVersion;

			Vector<VkQueueFamilyProperties> qprops;
			if(!EnumerateQueueFamilies(qprops, [&](uint32_t *count, VkQueueFamilyProperties *data) { get_physical_device_queue_family_properties(handle, count, data); }, error, "queue family"))
				return false;
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

			Vector<VkExtensionProperties> dev_exts;
			if(!EnumerateResult(dev_exts, [&](uint32_t *count, VkExtensionProperties *data) { return enumerate_device_extension_properties(handle, nullptr, count, data); }, error, "device extension"))
				return false;
			for(const auto& ext : dev_exts) {
				VulkanExtensionInfo einfo;
				einfo.name = ExtensionName(ext);
				einfo.spec_version = ExtensionVersionToUInt(ext);
				info.device_extensions.Add() = pick(einfo);
			}

			if(info.api_version >= VK_API_VERSION_1_3) {
				VkPhysicalDeviceVulkan13Features f13{};
				f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
				VkPhysicalDeviceFeatures2 f2{};
				f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				f2.pNext = &f13;
				get_physical_device_features2(handle, &f2);
				info.dynamic_rendering = f13.dynamicRendering == VK_TRUE;
				info.synchronization2 = f13.synchronization2 == VK_TRUE;
			}

			if(info.api_version < VK_API_VERSION_1_3)
				AppendMissing(info, "Vulkan 1.3");
			if(!info.graphics_queue)
				AppendMissing(info, "graphics queue");
			if(!info.dynamic_rendering)
				AppendMissing(info, "dynamic rendering");
			if(!info.synchronization2)
				AppendMissing(info, "synchronization2");
			info.suitable = info.api_version >= VK_API_VERSION_1_3 && info.graphics_queue && info.dynamic_rendering && info.synchronization2;
			out.Add() = pick(device);
		}
		return true;
	}
};

struct VulkanSharedInstanceContext {
	VulkanDispatch dispatch;
	VkInstance instance = VK_NULL_HANDLE;
	PFN_vkDestroyInstance destroy_instance = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	PFN_vkCreateDevice create_device = nullptr;
	PFN_vkEnumeratePhysicalDevices enumerate_physical_devices = nullptr;
	PFN_vkGetPhysicalDeviceProperties get_physical_device_properties = nullptr;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties get_physical_device_queue_family_properties = nullptr;
	PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extension_properties = nullptr;
	PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2 = nullptr;
	PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger = nullptr;
	VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
	VulkanValidationCapture capture;
	VulkanPreflightReport preflight;
	Vector<VulkanDiscoveredDevice> catalogue;
	bool validation_requested = false;
	bool debug_utils_requested = false;
	bool debug_utils_available = false;
	bool cleanup_ok = true;
	uint64_t diagnostic_id = 0;

	~VulkanSharedInstanceContext() { Close(); }

	bool IsCleared() const
	{
		return dispatch.IsCleared() && instance == VK_NULL_HANDLE && destroy_instance == nullptr && get_device_proc_addr == nullptr && create_device == nullptr && enumerate_physical_devices == nullptr && get_physical_device_properties == nullptr && get_physical_device_queue_family_properties == nullptr && enumerate_device_extension_properties == nullptr && get_physical_device_features2 == nullptr && create_debug_utils_messenger == nullptr && destroy_debug_utils_messenger == nullptr && messenger == VK_NULL_HANDLE && catalogue.IsEmpty();
	}

	bool Open(bool request_validation, VulkanProcResolver resolver, String& error)
	{
		Close();
		cleanup_ok = true;
		validation_requested = request_validation;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.runtime_next_id);
		g_runtime_device_stats.runtime_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.runtime_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.runtime_last_id.store(diagnostic_id, std::memory_order_relaxed);
		dispatch.proc_filter = resolver;
		dispatch.module = LoadLibraryW(L"vulkan-1.dll");
		if(!dispatch.module) {
			error = "LoadLibraryW(vulkan-1.dll) failed";
			return false;
		}
		if(!ResolveGlobalProc(dispatch.get_instance_proc_addr, dispatch.proc_filter, dispatch.module, "vkGetInstanceProcAddr", error)) {
			Close();
			return false;
		}
		dispatch.enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(GetProcAddress(dispatch.module, "vkEnumerateInstanceVersion"));
		if(!ResolveGlobalProc(dispatch.enumerate_instance_layer_properties, dispatch.proc_filter, dispatch.module, "vkEnumerateInstanceLayerProperties", error)) {
			Close();
			return false;
		}
		if(!ResolveGlobalProc(dispatch.enumerate_instance_extension_properties, dispatch.proc_filter, dispatch.module, "vkEnumerateInstanceExtensionProperties", error)) {
			Close();
			return false;
		}
		if(!ResolveGlobalProc(dispatch.create_instance, dispatch.proc_filter, dispatch.module, "vkCreateInstance", error)) {
			Close();
			return false;
		}

		uint32_t loader_version = VK_API_VERSION_1_0;
		if(!QueryLoaderVersion(dispatch.enumerate_instance_version, loader_version, error)) {
			Close();
			return false;
		}
		if(loader_version < VK_API_VERSION_1_3) {
			error = "loader api version older than Vulkan 1.3";
			Close();
			return false;
		}

		Vector<VkExtensionProperties> instance_exts;
		if(!EnumerateResult(instance_exts, [&](uint32_t *count, VkExtensionProperties *data) { return dispatch.enumerate_instance_extension_properties(nullptr, count, data); }, error, "instance extension")) {
			Close();
			return false;
		}
		for(const auto& ext : instance_exts) {
			VulkanExtensionInfo info;
			info.name = ExtensionName(ext);
			info.spec_version = ExtensionVersionToUInt(ext);
			preflight.instance_extensions.Add() = pick(info);
		}
		preflight.loader_available = true;
		preflight.loader_version = loader_version;
		preflight.validation_requested = request_validation;
		preflight.debug_utils_available = HasExtension(preflight.instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		preflight.validation_available = false;

		Vector<VkLayerProperties> layers;
		if(!EnumerateResult(layers, [&](uint32_t *count, VkLayerProperties *data) { return dispatch.enumerate_instance_layer_properties(count, data); }, error, "instance layer")) {
			Close();
			return false;
		}
		for(const auto& layer : layers) {
			VulkanLayerInfo info;
			info.name = LayerName(layer);
			info.description = layer.description;
			info.spec_version = LayerVersionToUInt(layer);
			preflight.instance_layers.Add() = pick(info);
		}
		for(const auto& layer : preflight.instance_layers)
			if(layer.name == "VK_LAYER_KHRONOS_validation")
				preflight.validation_available = true;

		if(request_validation && !preflight.validation_available) {
			error = "VK_LAYER_KHRONOS_validation not present";
			Close();
			return false;
		}
		if(request_validation && !preflight.debug_utils_available) {
			error = "VK_EXT_debug_utils not present";
			Close();
			return false;
		}

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = request_validation ? "GpuCtrlSharedValidation" : "GpuCtrlSharedInstance";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "upp_render";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_3;

		Vector<const char*> enabled_layers;
		Vector<const char*> enabled_exts;
		if(request_validation) {
			enabled_layers.Add("VK_LAYER_KHRONOS_validation");
			enabled_exts.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledLayerCount = enabled_layers.GetCount();
		create_info.ppEnabledLayerNames = enabled_layers.IsEmpty() ? nullptr : enabled_layers.Begin();
		create_info.enabledExtensionCount = enabled_exts.GetCount();
		create_info.ppEnabledExtensionNames = enabled_exts.IsEmpty() ? nullptr : enabled_exts.Begin();

		VkResult vr = dispatch.create_instance(&create_info, nullptr, &instance);
		if(vr != VK_SUCCESS) {
			error = String("vkCreateInstance failed: ") + AsString((int)vr);
			Close();
			return false;
		}
		g_runtime_device_stats.instance_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_last_id.store(diagnostic_id, std::memory_order_relaxed);
		preflight.instance_created = true;

		if(!ResolveInstanceProc(destroy_instance, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkDestroyInstance", error)) { Close(); return false; }
		if(!ResolveInstanceProc(get_device_proc_addr, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkGetDeviceProcAddr", error)) { Close(); return false; }
		if(!ResolveInstanceProc(create_device, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkCreateDevice", error)) { Close(); return false; }
		if(!ResolveInstanceProc(enumerate_physical_devices, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkEnumeratePhysicalDevices", error)) { Close(); return false; }
		if(!ResolveInstanceProc(get_physical_device_properties, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkGetPhysicalDeviceProperties", error)) { Close(); return false; }
		if(!ResolveInstanceProc(get_physical_device_queue_family_properties, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkGetPhysicalDeviceQueueFamilyProperties", error)) { Close(); return false; }
		if(!ResolveInstanceProc(enumerate_device_extension_properties, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkEnumerateDeviceExtensionProperties", error)) { Close(); return false; }
		if(!ResolveInstanceProc(get_physical_device_features2, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkGetPhysicalDeviceFeatures2", error)) { Close(); return false; }

		if(request_validation) {
			if(!ResolveInstanceProc(create_debug_utils_messenger, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkCreateDebugUtilsMessengerEXT", error)) { Close(); return false; }
			if(!ResolveInstanceProc(destroy_debug_utils_messenger, dispatch.proc_filter, dispatch.get_instance_proc_addr, instance, "vkDestroyDebugUtilsMessengerEXT", error)) { Close(); return false; }
			VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
			messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			messenger_info.pfnUserCallback = VulkanDebugCallback;
			messenger_info.pUserData = &capture;
			vr = create_debug_utils_messenger(instance, &messenger_info, nullptr, &messenger);
			if(vr != VK_SUCCESS) {
				error = String("vkCreateDebugUtilsMessengerEXT failed: ") + AsString((int)vr);
				Close();
				return false;
			}
			g_runtime_device_stats.debug_messenger_create_count.fetch_add(1, std::memory_order_relaxed);
			g_runtime_device_stats.debug_messenger_live_count.fetch_add(1, std::memory_order_relaxed);
			debug_utils_requested = true;
			debug_utils_available = true;
			preflight.debug_utils_available = true;
		}

		Vector<VulkanDiscoveredDevice> discovered;
		if(!EnumeratePhysicalDevices(discovered, error)) {
			Close();
			return false;
		}
		if(discovered.IsEmpty()) {
			error = "no physical devices";
			Close();
			return false;
		}
		catalogue = pick(discovered);
		for(const auto& found : catalogue) {
			preflight.devices.Add();
			CloneDeviceInfo(preflight.devices.Top(), found.info);
			if(found.info.suitable)
				preflight.suitable_device_count += 1;
		}
		preflight.status = preflight.suitable_device_count > 0 ? VulkanProbeStatus::Ok : VulkanProbeStatus::NoSuitableDevices;
		preflight.status_text = StatusText(preflight.status);
		return true;
	}

	bool Close()
	{
		bool ok = true;
		if(messenger && destroy_debug_utils_messenger) {
			destroy_debug_utils_messenger(instance, messenger, nullptr);
			messenger = VK_NULL_HANDLE;
			g_runtime_device_stats.debug_messenger_live_count.fetch_sub(1, std::memory_order_relaxed);
		}
		else if(messenger) {
			ok = false;
			messenger = VK_NULL_HANDLE;
		}
		if(instance && destroy_instance)
			destroy_instance(instance, nullptr);
		else if(instance)
			ok = false;
		instance = VK_NULL_HANDLE;
		destroy_instance = nullptr;
		get_device_proc_addr = nullptr;
		create_device = nullptr;
		enumerate_physical_devices = nullptr;
		get_physical_device_properties = nullptr;
		get_physical_device_queue_family_properties = nullptr;
		enumerate_device_extension_properties = nullptr;
		get_physical_device_features2 = nullptr;
		create_debug_utils_messenger = nullptr;
		destroy_debug_utils_messenger = nullptr;
		capture = VulkanValidationCapture();
		catalogue.Clear();
		preflight = VulkanPreflightReport();
		validation_requested = false;
		debug_utils_requested = false;
		debug_utils_available = false;
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.instance_live_count.fetch_sub(1, std::memory_order_relaxed);
			g_runtime_device_stats.runtime_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		dispatch.Close();
		cleanup_ok = cleanup_ok && ok && IsCleared();
		return cleanup_ok;
	}

	bool EnumeratePhysicalDevices(Vector<VulkanDiscoveredDevice>& out, String& error) const
	{
		if(catalogue.IsEmpty()) {
			out.Clear();
			return true;
		}
		out.Clear();
		for(const auto& device : catalogue)
			out.Add() = device;
		return true;
	}

	const Vector<VulkanDiscoveredDevice>& GetCatalogue() const { return catalogue; }
};

struct VulkanSharedInstanceKey {
	bool validation_requested = false;
	uintptr_t resolver_token = 0;

	bool operator==(const VulkanSharedInstanceKey& other) const
	{
		return validation_requested == other.validation_requested && resolver_token == other.resolver_token;
	}
};

struct VulkanSharedInstanceKeyHash {
	size_t operator()(const VulkanSharedInstanceKey& key) const noexcept
	{
		size_t h1 = std::hash<int>()(key.validation_requested ? 1 : 0);
		size_t h2 = std::hash<uintptr_t>()(key.resolver_token);
		return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
	}
};

using VulkanSharedInstanceHandle = std::shared_ptr<VulkanSharedInstanceContext>;

static std::mutex& SharedInstanceRegistryMutex()
{
	static std::mutex *mutex = new std::mutex;
	return *mutex;
}

static std::unordered_map<VulkanSharedInstanceKey, std::weak_ptr<VulkanSharedInstanceContext>, VulkanSharedInstanceKeyHash>& SharedInstanceRegistry()
{
	static std::unordered_map<VulkanSharedInstanceKey, std::weak_ptr<VulkanSharedInstanceContext>, VulkanSharedInstanceKeyHash> *registry = new std::unordered_map<VulkanSharedInstanceKey, std::weak_ptr<VulkanSharedInstanceContext>, VulkanSharedInstanceKeyHash>;
	return *registry;
}

static uintptr_t ResolverToken(VulkanProcResolver resolver)
{
	return (uintptr_t)resolver;
}

static VulkanSharedInstanceHandle AcquireSharedInstance(bool request_validation, VulkanProcResolver resolver, String& error)
{
	VulkanSharedInstanceKey key;
	key.validation_requested = request_validation;
	key.resolver_token = ResolverToken(resolver);
	std::lock_guard<std::mutex> lock(SharedInstanceRegistryMutex());
	auto& registry = SharedInstanceRegistry();
	auto it = registry.find(key);
	if(it != registry.end()) {
		auto shared = it->second.lock();
		if(shared)
			return shared;
	}
	auto shared = std::make_shared<VulkanSharedInstanceContext>();
	if(!shared->Open(request_validation, resolver, error))
		return VulkanSharedInstanceHandle();
	registry[key] = shared;
	return shared;
}

struct VulkanDeviceContext {
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphics_queue = VK_NULL_HANDLE;
	VkQueue present_queue = VK_NULL_HANDLE;
	PFN_vkDestroyDevice destroy_device = nullptr;
	PFN_vkGetDeviceQueue get_device_queue = nullptr;
	PFN_vkDeviceWaitIdle device_wait_idle = nullptr;
	bool cleanup_ok = true;
	int cleanup_result = 0;
	String cleanup_error;
	uint64_t diagnostic_id = 0;

	~VulkanDeviceContext() { Close(); }

	bool IsCleared() const
	{
		return physical_device == VK_NULL_HANDLE && device == VK_NULL_HANDLE && graphics_queue == VK_NULL_HANDLE && present_queue == VK_NULL_HANDLE && destroy_device == nullptr && get_device_queue == nullptr && device_wait_idle == nullptr;
	}

	bool Close()
	{
		bool ok = true;
		if(device) {
			VkResult vr = VK_SUCCESS;
			if(g_validation_test_injection.point == VulkanValidationTestPoint::DuringDeviceCleanup && g_validation_test_injection.force_device_cleanup_failure)
				vr = g_validation_test_injection.device_cleanup_result;
			else if(device_wait_idle)
				vr = device_wait_idle(device);
			cleanup_result = (int)vr;
			if(vr != VK_SUCCESS) {
				ok = false;
				cleanup_error = String("vkDeviceWaitIdle failed: ") + AsString((int)vr);
			}
		}
		if(device && destroy_device)
			destroy_device(device, nullptr);
		else if(device)
			ok = false;
		device = VK_NULL_HANDLE;
		graphics_queue = VK_NULL_HANDLE;
		present_queue = VK_NULL_HANDLE;
		destroy_device = nullptr;
		get_device_queue = nullptr;
		device_wait_idle = nullptr;
		physical_device = VK_NULL_HANDLE;
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.device_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		return cleanup_ok;
	}

	bool Open(const VulkanSharedInstanceContext& shared, VkPhysicalDevice physical_device, VulkanDeviceInfo& device_info, VulkanBootstrapReport& report, String& error)
	{
		this->physical_device = physical_device;
		cleanup_ok = true;
		cleanup_result = 0;
		cleanup_error.Clear();
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.device_next_id);
		g_runtime_device_stats.device_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.device_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.device_last_id.store(diagnostic_id, std::memory_order_relaxed);
		int chosen_queue_index = -1;
		int queue_rank = -1;
		for(const auto& q : device_info.queue_families) {
			if(!q.graphics || q.count == 0)
				continue;
			int score = QueueRank(q);
			if(score > queue_rank || (score == queue_rank && q.index < chosen_queue_index)) {
				queue_rank = score;
				chosen_queue_index = q.index;
			}
		}
		if(chosen_queue_index < 0)
			return false;

		VkDeviceQueueCreateInfo qci{};
		float priority = 1.0f;
		qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci.queueFamilyIndex = (uint32_t)chosen_queue_index;
		qci.queueCount = 1;
		qci.pQueuePriorities = &priority;

		VkPhysicalDeviceVulkan13Features f13{};
		f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		f13.dynamicRendering = VK_TRUE;
		f13.synchronization2 = VK_TRUE;

		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &f13;

		VkDeviceCreateInfo dci{};
		dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		dci.pNext = &features2;
		dci.queueCreateInfoCount = 1;
		dci.pQueueCreateInfos = &qci;

		auto fail = [&](const String& message) {
			error = message;
			Close();
			return false;
		};

		PFN_vkCreateDevice create_device = nullptr;
		if(!ResolveInstanceProc(create_device, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkCreateDevice", error))
			return fail(error);
		VkResult vr = create_device(physical_device, &dci, nullptr, &device);
		if(vr != VK_SUCCESS) {
			error = String("vkCreateDevice failed: ") + AsString((int)vr);
			return fail(error);
		}

		if(!ResolveDeviceProc(destroy_device, shared.dispatch.proc_filter, shared.get_device_proc_addr, device, "vkDestroyDevice", error)) return fail(error);
		if(!ResolveDeviceProc(get_device_queue, shared.dispatch.proc_filter, shared.get_device_proc_addr, device, "vkGetDeviceQueue", error)) return fail(error);
		if(!ResolveDeviceProc(device_wait_idle, shared.dispatch.proc_filter, shared.get_device_proc_addr, device, "vkDeviceWaitIdle", error)) return fail(error);

		get_device_queue(device, (uint32_t)chosen_queue_index, 0, &graphics_queue);
		if(graphics_queue == VK_NULL_HANDLE) {
			error = "vkGetDeviceQueue returned VK_NULL_HANDLE";
			return fail(error);
		}

		device_info.selected_queue_family_index = chosen_queue_index;
		device_info.logical_device_created = true;
		device_info.graphics_queue_acquired = true;
		device_info.selected_queue_count = device_info.queue_families[chosen_queue_index].count;
		device_info.selected_queue_flags = device_info.queue_families[chosen_queue_index].flags;
		device_info.selected_queue_compute = device_info.queue_families[chosen_queue_index].compute;
		device_info.selected_queue_transfer = device_info.queue_families[chosen_queue_index].transfer;
		device_info.selection_reason = DeviceRank(device_info.type) >= 2 ? (device_info.type == "discrete" ? "preferred discrete GPU" : "preferred integrated GPU") : "first suitable device in enumeration order";
		return true;
	}
};

struct VulkanSurfaceContext {
	const VulkanDispatch *dispatch = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	PFN_vkDestroyInstance destroy_instance = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	PFN_vkCreateDevice create_device = nullptr;
	PFN_vkEnumeratePhysicalDevices enumerate_physical_devices = nullptr;
	PFN_vkGetPhysicalDeviceProperties get_physical_device_properties = nullptr;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties get_physical_device_queue_family_properties = nullptr;
	PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extension_properties = nullptr;
	PFN_vkGetPhysicalDeviceFeatures2 get_physical_device_features2 = nullptr;
	PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger = nullptr;
	PFN_vkCreateWin32SurfaceKHR create_win32_surface = nullptr;
	PFN_vkDestroySurfaceKHR destroy_surface = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR get_surface_support = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_surface_capabilities = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR get_surface_formats = nullptr;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR get_surface_present_modes = nullptr;
	VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VulkanValidationCapture capture;
	bool validation_requested = false;
	bool debug_utils_requested = false;
	bool debug_utils_available = false;
	bool surface_requested = false;
	bool surface_available = false;
	bool cleanup_ok = true;
	uint64_t diagnostic_id = 0;

	~VulkanSurfaceContext() { Close(); }

	bool IsCleared() const
	{
		return dispatch == nullptr && instance == VK_NULL_HANDLE && destroy_instance == nullptr && get_device_proc_addr == nullptr && create_device == nullptr && enumerate_physical_devices == nullptr && get_physical_device_properties == nullptr && get_physical_device_queue_family_properties == nullptr && enumerate_device_extension_properties == nullptr && get_physical_device_features2 == nullptr && create_debug_utils_messenger == nullptr && destroy_debug_utils_messenger == nullptr && create_win32_surface == nullptr && destroy_surface == nullptr && get_surface_support == nullptr && get_surface_capabilities == nullptr && get_surface_formats == nullptr && get_surface_present_modes == nullptr && messenger == VK_NULL_HANDLE && surface == VK_NULL_HANDLE;
	}

	bool Close()
	{
		bool ok = true;
		if(surface && destroy_surface) {
			destroy_surface(instance, surface, nullptr);
			surface = VK_NULL_HANDLE;
		}
		else if(surface) {
			ok = false;
			surface = VK_NULL_HANDLE;
		}
		if(messenger && destroy_debug_utils_messenger) {
			destroy_debug_utils_messenger(instance, messenger, nullptr);
			messenger = VK_NULL_HANDLE;
			g_runtime_device_stats.debug_messenger_live_count.fetch_sub(1, std::memory_order_relaxed);
		}
		else if(messenger) {
			ok = false;
			messenger = VK_NULL_HANDLE;
		}
		if(instance && destroy_instance)
			destroy_instance(instance, nullptr);
		else if(instance)
			ok = false;
		if(instance)
			g_runtime_device_stats.instance_live_count.fetch_sub(1, std::memory_order_relaxed);
		instance = VK_NULL_HANDLE;
		destroy_instance = nullptr;
		get_device_proc_addr = nullptr;
		create_device = nullptr;
		enumerate_physical_devices = nullptr;
		get_physical_device_properties = nullptr;
		get_physical_device_queue_family_properties = nullptr;
		enumerate_device_extension_properties = nullptr;
		get_physical_device_features2 = nullptr;
		create_debug_utils_messenger = nullptr;
		destroy_debug_utils_messenger = nullptr;
		create_win32_surface = nullptr;
		destroy_surface = nullptr;
		get_surface_support = nullptr;
		get_surface_capabilities = nullptr;
		get_surface_formats = nullptr;
		get_surface_present_modes = nullptr;
		capture = VulkanValidationCapture();
		dispatch = nullptr;
		validation_requested = false;
		debug_utils_requested = false;
		debug_utils_available = false;
		surface_requested = false;
		surface_available = false;
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.surface_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		return cleanup_ok;
	}

	bool Open(const VulkanDispatch& d, bool request_validation, const GpuNativeWindowDesc& native_window, VulkanSurfaceReport& report, String& error)
	{
		Close();
		cleanup_ok = true;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.surface_next_id);
		g_runtime_device_stats.surface_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.surface_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.surface_last_id.store(diagnostic_id, std::memory_order_relaxed);
		dispatch = &d;
		validation_requested = request_validation;
		surface_requested = true;
		auto fail = [&](const String& message) {
			error = message;
			Close();
			return false;
		};

		Vector<VkExtensionProperties> instance_exts;
		if(!EnumerateResult(instance_exts, [&](uint32_t *count, VkExtensionProperties *data) { return d.enumerate_instance_extension_properties(nullptr, count, data); }, error, "instance extension"))
			return fail(error);
		for(const auto& ext : instance_exts) {
			VulkanExtensionInfo info;
			info.name = ExtensionName(ext);
			info.spec_version = ExtensionVersionToUInt(ext);
			report.preflight.instance_extensions.Add() = pick(info);
		}
		report.preflight.debug_utils_available = HasExtension(report.preflight.instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		surface_available = HasExtension(report.preflight.instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME) && HasExtension(report.preflight.instance_extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		report.debug_utils_available = report.preflight.debug_utils_available;

		Vector<VkLayerProperties> layers;
		if(!EnumerateResult(layers, [&](uint32_t *count, VkLayerProperties *data) { return d.enumerate_instance_layer_properties(count, data); }, error, "instance layer"))
			return fail(error);
		for(const auto& layer : layers) {
			VulkanLayerInfo info;
			info.name = LayerName(layer);
			info.description = layer.description;
			info.spec_version = LayerVersionToUInt(layer);
			report.preflight.instance_layers.Add() = pick(info);
		}
		report.preflight.validation_available = false;
		for(const auto& layer : report.preflight.instance_layers)
			if(layer.name == "VK_LAYER_KHRONOS_validation")
				report.preflight.validation_available = true;
		report.validation_available = report.preflight.validation_available;

		if(request_validation && !report.preflight.validation_available) {
			error = "VK_LAYER_KHRONOS_validation not present";
			return false;
		}
		if(request_validation && !report.preflight.debug_utils_available) {
			error = "VK_EXT_debug_utils not present";
			return fail(error);
		}
		if(native_window.kind != GpuNativeWindowKind::Win32) {
			error = "surface requires Win32 native window";
			return false;
		}
		if(native_window.handle == 0) {
			error = "invalid native handle";
			return false;
		}
		if(!HasExtension(report.preflight.instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
			error = "VK_KHR_surface not present";
			return false;
		}
		if(!HasExtension(report.preflight.instance_extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
			error = "VK_KHR_win32_surface not present";
			return false;
		}

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "VulkanSurfaceProbe";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "upp_render";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_3;

		Vector<const char*> enabled_layers;
		Vector<const char*> enabled_exts;
		if(request_validation) {
			enabled_layers.Add("VK_LAYER_KHRONOS_validation");
			enabled_exts.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		enabled_exts.Add(VK_KHR_SURFACE_EXTENSION_NAME);
		enabled_exts.Add(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledLayerCount = enabled_layers.GetCount();
		create_info.ppEnabledLayerNames = enabled_layers.IsEmpty() ? nullptr : enabled_layers.Begin();
		create_info.enabledExtensionCount = enabled_exts.GetCount();
		create_info.ppEnabledExtensionNames = enabled_exts.IsEmpty() ? nullptr : enabled_exts.Begin();

		VkResult vr = d.create_instance(&create_info, nullptr, &instance);
		if(vr != VK_SUCCESS) {
			error = String("vkCreateInstance failed: ") + AsString((int)vr);
			return fail(error);
		}
		g_runtime_device_stats.instance_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_live_count.fetch_add(1, std::memory_order_relaxed);
		report.preflight.instance_created = true;

		if(!ResolveInstanceProc(destroy_instance, d.proc_filter, d.get_instance_proc_addr, instance, "vkDestroyInstance", error)) return fail(error);
		if(!ResolveInstanceProc(get_device_proc_addr, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetDeviceProcAddr", error)) return fail(error);
		if(!ResolveInstanceProc(create_device, d.proc_filter, d.get_instance_proc_addr, instance, "vkCreateDevice", error)) return fail(error);
		if(!ResolveInstanceProc(enumerate_physical_devices, d.proc_filter, d.get_instance_proc_addr, instance, "vkEnumeratePhysicalDevices", error)) return fail(error);
		if(!ResolveInstanceProc(get_physical_device_properties, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceProperties", error)) return fail(error);
		if(!ResolveInstanceProc(get_physical_device_queue_family_properties, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceQueueFamilyProperties", error)) return fail(error);
		if(!ResolveInstanceProc(enumerate_device_extension_properties, d.proc_filter, d.get_instance_proc_addr, instance, "vkEnumerateDeviceExtensionProperties", error)) return fail(error);
		if(!ResolveInstanceProc(get_physical_device_features2, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceFeatures2", error)) return fail(error);
		if(!ResolveInstanceProc(create_win32_surface, d.proc_filter, d.get_instance_proc_addr, instance, "vkCreateWin32SurfaceKHR", error)) return fail(error);
		if(!ResolveInstanceProc(destroy_surface, d.proc_filter, d.get_instance_proc_addr, instance, "vkDestroySurfaceKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_support, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceSurfaceSupportKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_capabilities, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_formats, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceSurfaceFormatsKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_present_modes, d.proc_filter, d.get_instance_proc_addr, instance, "vkGetPhysicalDeviceSurfacePresentModesKHR", error)) return fail(error);

		if(request_validation) {
			if(!ResolveInstanceProc(create_debug_utils_messenger, d.proc_filter, d.get_instance_proc_addr, instance, "vkCreateDebugUtilsMessengerEXT", error)) return fail(error);
			if(!ResolveInstanceProc(destroy_debug_utils_messenger, d.proc_filter, d.get_instance_proc_addr, instance, "vkDestroyDebugUtilsMessengerEXT", error)) return fail(error);
			VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
			messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			messenger_info.pfnUserCallback = VulkanDebugCallback;
			messenger_info.pUserData = &capture;
			vr = create_debug_utils_messenger(instance, &messenger_info, nullptr, &messenger);
			if(vr != VK_SUCCESS) {
				error = String("vkCreateDebugUtilsMessengerEXT failed: ") + AsString((int)vr);
				return fail(error);
			}
			g_runtime_device_stats.debug_messenger_create_count.fetch_add(1, std::memory_order_relaxed);
			g_runtime_device_stats.debug_messenger_live_count.fetch_add(1, std::memory_order_relaxed);
			debug_utils_requested = true;
			debug_utils_available = true;
		}

		VkWin32SurfaceCreateInfoKHR surface_info{};
		surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_info.hwnd = (HWND)(uintptr_t)native_window.handle;
		if(!IsWindow(surface_info.hwnd)) {
			error = "invalid native handle";
			return fail(error);
		}
		surface_info.hinstance = (HINSTANCE)GetWindowLongPtr(surface_info.hwnd, GWLP_HINSTANCE);
		if(!surface_info.hinstance) {
			error = "invalid native handle";
			return fail(error);
		}
		vr = create_win32_surface(instance, &surface_info, nullptr, &surface);
		if(vr != VK_SUCCESS) {
			error = String("vkCreateWin32SurfaceKHR failed: ") + AsString((int)vr);
			return fail(error);
		}
		report.surface_created = true;
		return true;
	}

	bool QuerySurfaceCapabilities(VkPhysicalDevice handle, VulkanSurfaceReport& report, String& error) const
	{
		VkSurfaceCapabilitiesKHR caps{};
		VkResult vr = get_surface_capabilities(handle, surface, &caps);
		if(vr != VK_SUCCESS) {
			error = String("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ") + AsString((int)vr);
			return false;
		}
		report.min_image_count = (int)caps.minImageCount;
		report.max_image_count = (int)caps.maxImageCount;
		report.current_extent = Size((int)caps.currentExtent.width, (int)caps.currentExtent.height);
		report.min_extent = Size((int)caps.minImageExtent.width, (int)caps.minImageExtent.height);
		report.max_extent = Size((int)caps.maxImageExtent.width, (int)caps.maxImageExtent.height);
		report.supported_transforms = caps.supportedTransforms;
		report.current_transform = caps.currentTransform;
		report.supported_composite_alpha = caps.supportedCompositeAlpha;
		report.supported_image_usage = caps.supportedUsageFlags;

		Vector<VkSurfaceFormatKHR> formats;
		if(!EnumerateResult(formats, [&](uint32_t *count, VkSurfaceFormatKHR *data) { return get_surface_formats(handle, surface, count, data); }, error, "surface format"))
			return false;
		Sort(formats, [](const VkSurfaceFormatKHR& a, const VkSurfaceFormatKHR& b) {
			if(a.format != b.format) return (int)a.format < (int)b.format;
			return (int)a.colorSpace < (int)b.colorSpace;
		});
		report.surface_formats.Clear();
		for(const auto& f : formats) {
			report.surface_formats.Add() = SurfaceFormatText(f.format) + "/" + ColorSpaceText(f.colorSpace);
			if((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_SRGB) && (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
				report.preferred_bgra8 = true;
			if((f.format == VK_FORMAT_R8G8B8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_SRGB) && (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
				report.preferred_rgba8 = true;
			if(f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				report.preferred_srgb = true;
		}

		Vector<VkPresentModeKHR> modes;
		if(!EnumerateResult(modes, [&](uint32_t *count, VkPresentModeKHR *data) { return get_surface_present_modes(handle, surface, count, data); }, error, "present mode"))
			return false;
		Sort(modes, [](VkPresentModeKHR a, VkPresentModeKHR b) { return (int)a < (int)b; });
		report.present_modes.Clear();
		for(auto mode : modes) {
			report.present_modes.Add() = PresentModeText(mode);
			if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
				report.preferred_mailbox = true;
			if(mode == VK_PRESENT_MODE_FIFO_KHR)
				report.preferred_fifo = true;
		}
		return true;
	}

	bool EnumeratePhysicalDevices(Vector<VulkanDiscoveredDevice>& out, VulkanSurfaceReport& report, String& error) const
	{
		Vector<VkPhysicalDevice> handles;
		if(!EnumerateResult(handles, [&](uint32_t *count, VkPhysicalDevice *data) { return enumerate_physical_devices(instance, count, data); }, error, "physical device"))
			return false;
		if(handles.IsEmpty())
			return true;

		for(VkPhysicalDevice handle : handles) {
			VulkanDiscoveredDevice device;
			device.handle = handle;
			VulkanDeviceInfo& info = device.info;
			VkPhysicalDeviceProperties props{};
			get_physical_device_properties(handle, &props);
			info.name = props.deviceName;
			info.type = DeviceTypeText(props.deviceType);
			info.vendor_id = props.vendorID;
			info.device_id = props.deviceID;
			info.driver_version = props.driverVersion;
			info.api_version = props.apiVersion;

			Vector<VkQueueFamilyProperties> qprops;
			if(!EnumerateQueueFamilies(qprops, [&](uint32_t *count, VkQueueFamilyProperties *data) { get_physical_device_queue_family_properties(handle, count, data); }, error, "queue family"))
				return false;
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
				VkBool32 present = VK_FALSE;
				VkResult support_result = get_surface_support(handle, (uint32_t)i, surface, &present);
				if(support_result != VK_SUCCESS) {
					error = String("vkGetPhysicalDeviceSurfaceSupportKHR failed: ") + AsString((int)support_result);
					return false;
				}
				qinfo.present = present == VK_TRUE;
				info.queue_families.Add() = pick(qinfo);
				if(qinfo.graphics)
					info.graphics_queue = true;
			}

			Vector<VkExtensionProperties> dev_exts;
			if(!EnumerateResult(dev_exts, [&](uint32_t *count, VkExtensionProperties *data) { return enumerate_device_extension_properties(handle, nullptr, count, data); }, error, "device extension"))
				return false;
			for(const auto& ext : dev_exts) {
				VulkanExtensionInfo einfo;
				einfo.name = ExtensionName(ext);
				einfo.spec_version = ExtensionVersionToUInt(ext);
				info.device_extensions.Add() = pick(einfo);
			}

			if(info.api_version >= VK_API_VERSION_1_3) {
				VkPhysicalDeviceVulkan13Features f13{};
				f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
				VkPhysicalDeviceFeatures2 f2{};
				f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				f2.pNext = &f13;
				get_physical_device_features2(handle, &f2);
				info.dynamic_rendering = f13.dynamicRendering == VK_TRUE;
				info.synchronization2 = f13.synchronization2 == VK_TRUE;
			}

			if(info.api_version < VK_API_VERSION_1_3)
				AppendMissing(info, "Vulkan 1.3");
			if(!info.graphics_queue)
				AppendMissing(info, "graphics queue");
			if(!info.dynamic_rendering)
				AppendMissing(info, "dynamic rendering");
			if(!info.synchronization2)
				AppendMissing(info, "synchronization2");
			if(!HasExtension(info.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
				AppendMissing(info, "VK_KHR_swapchain");
			info.suitable = info.api_version >= VK_API_VERSION_1_3 && info.graphics_queue && info.dynamic_rendering && info.synchronization2 && HasExtension(info.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			out.Add() = pick(device);
		}
		return true;
	}
};

struct VulkanSurfaceSessionContext {
	VkInstance instance = VK_NULL_HANDLE;
	PFN_vkDestroySurfaceKHR destroy_surface = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR get_surface_support = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_surface_capabilities = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR get_surface_formats = nullptr;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR get_surface_present_modes = nullptr;
	PFN_vkCreateWin32SurfaceKHR create_win32_surface = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool cleanup_ok = true;
	uint64_t diagnostic_id = 0;

	~VulkanSurfaceSessionContext() { Close(); }

	bool IsCleared() const
	{
		return instance == VK_NULL_HANDLE && destroy_surface == nullptr && get_surface_support == nullptr && get_surface_capabilities == nullptr && get_surface_formats == nullptr && get_surface_present_modes == nullptr && create_win32_surface == nullptr && surface == VK_NULL_HANDLE;
	}

	bool Open(const VulkanSharedInstanceContext& shared, const GpuNativeWindowDesc& native_window, String& error)
	{
		Close();
		cleanup_ok = true;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.surface_next_id);
		g_runtime_device_stats.surface_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.surface_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.surface_last_id.store(diagnostic_id, std::memory_order_relaxed);
		instance = shared.instance;
		auto fail = [&](const String& message) {
			error = message;
			Close();
			return false;
		};

		if(native_window.kind != GpuNativeWindowKind::Win32)
			return fail("surface requires Win32 native window");
		if(native_window.handle == 0)
			return fail("invalid native handle");

		if(!ResolveInstanceProc(create_win32_surface, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkCreateWin32SurfaceKHR", error)) return fail(error);
		if(!ResolveInstanceProc(destroy_surface, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkDestroySurfaceKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_support, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkGetPhysicalDeviceSurfaceSupportKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_capabilities, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_formats, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_present_modes, shared.dispatch.proc_filter, shared.dispatch.get_instance_proc_addr, shared.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR", error)) return fail(error);

		VkWin32SurfaceCreateInfoKHR surface_info{};
		surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_info.hwnd = (HWND)(uintptr_t)native_window.handle;
		if(!IsWindow(surface_info.hwnd))
			return fail("invalid native handle");
		surface_info.hinstance = (HINSTANCE)GetWindowLongPtr(surface_info.hwnd, GWLP_HINSTANCE);
		if(!surface_info.hinstance)
			return fail("invalid native handle");
		VkResult vr = create_win32_surface(shared.instance, &surface_info, nullptr, &surface);
		if(vr != VK_SUCCESS)
			return fail(String("vkCreateWin32SurfaceKHR failed: ") + AsString((int)vr));
		return true;
	}

	bool Close()
	{
		bool ok = true;
		if(surface && destroy_surface) {
			destroy_surface(instance, surface, nullptr);
			surface = VK_NULL_HANDLE;
		}
		else if(surface) {
			ok = false;
			surface = VK_NULL_HANDLE;
		}
		instance = VK_NULL_HANDLE;
		destroy_surface = nullptr;
		get_surface_support = nullptr;
		get_surface_capabilities = nullptr;
		get_surface_formats = nullptr;
		get_surface_present_modes = nullptr;
		create_win32_surface = nullptr;
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.surface_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		return cleanup_ok;
	}

	bool QuerySurfaceCapabilities(VkPhysicalDevice handle, VulkanSurfaceReport& report, String& error) const
	{
		VkSurfaceCapabilitiesKHR caps{};
		VkResult vr = get_surface_capabilities(handle, surface, &caps);
		if(vr != VK_SUCCESS) {
			error = String("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ") + AsString((int)vr);
			return false;
		}
		report.min_image_count = (int)caps.minImageCount;
		report.max_image_count = (int)caps.maxImageCount;
		report.current_extent = Size((int)caps.currentExtent.width, (int)caps.currentExtent.height);
		report.min_extent = Size((int)caps.minImageExtent.width, (int)caps.minImageExtent.height);
		report.max_extent = Size((int)caps.maxImageExtent.width, (int)caps.maxImageExtent.height);
		report.supported_transforms = caps.supportedTransforms;
		report.current_transform = caps.currentTransform;
		report.supported_composite_alpha = caps.supportedCompositeAlpha;
		report.supported_image_usage = caps.supportedUsageFlags;

		Vector<VkSurfaceFormatKHR> formats;
		if(!EnumerateResult(formats, [&](uint32_t *count, VkSurfaceFormatKHR *data) { return get_surface_formats(handle, surface, count, data); }, error, "surface format"))
			return false;
		Sort(formats, [](const VkSurfaceFormatKHR& a, const VkSurfaceFormatKHR& b) {
			if(a.format != b.format) return (int)a.format < (int)b.format;
			return (int)a.colorSpace < (int)b.colorSpace;
		});
		report.surface_formats.Clear();
		for(const auto& f : formats) {
			report.surface_formats.Add() = SurfaceFormatText(f.format) + "/" + ColorSpaceText(f.colorSpace);
			if((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_SRGB) && (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
				report.preferred_bgra8 = true;
			if((f.format == VK_FORMAT_R8G8B8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_SRGB) && (f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
				report.preferred_rgba8 = true;
			if(f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				report.preferred_srgb = true;
		}

		Vector<VkPresentModeKHR> modes;
		if(!EnumerateResult(modes, [&](uint32_t *count, VkPresentModeKHR *data) { return get_surface_present_modes(handle, surface, count, data); }, error, "present mode"))
			return false;
		Sort(modes, [](VkPresentModeKHR a, VkPresentModeKHR b) { return (int)a < (int)b; });
		report.present_modes.Clear();
		for(auto mode : modes) {
			report.present_modes.Add() = PresentModeText(mode);
			if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
				report.preferred_mailbox = true;
			if(mode == VK_PRESENT_MODE_FIFO_KHR)
				report.preferred_fifo = true;
		}
		return true;
	}

	bool QueryPresentSupport(VkPhysicalDevice handle, int queue_family, bool& present, String& error) const
	{
		VkBool32 supported = VK_FALSE;
		VkResult vr = get_surface_support(handle, (uint32_t)queue_family, surface, &supported);
		if(vr != VK_SUCCESS) {
			error = String("vkGetPhysicalDeviceSurfaceSupportKHR failed: ") + AsString((int)vr);
			return false;
		}
		present = supported == VK_TRUE;
		return true;
	}
};

static String StatusText(VulkanProbeStatus status)
{
	switch(status) {
	case VulkanProbeStatus::Ok: return "ok";
	case VulkanProbeStatus::RuntimeUnavailable: return "runtime unavailable";
	case VulkanProbeStatus::RequiredLoaderFunctionUnavailable: return "required loader function unavailable";
	case VulkanProbeStatus::LoaderTooOld: return "loader api version older than Vulkan 1.3";
	case VulkanProbeStatus::LayerEnumerationFailed: return "layer enumeration failed";
	case VulkanProbeStatus::ExtensionEnumerationFailed: return "extension enumeration failed";
	case VulkanProbeStatus::ValidationUnavailable: return "validation layer unavailable";
	case VulkanProbeStatus::DebugUtilsUnavailable: return "debug utils unavailable";
	case VulkanProbeStatus::InstanceCreationFailed: return "instance creation failed";
	case VulkanProbeStatus::PhysicalDeviceEnumerationFailed: return "physical device enumeration failed";
	case VulkanProbeStatus::NoPhysicalDevices: return "no physical devices";
	case VulkanProbeStatus::NoSuitableDevices: return "no suitable devices";
	case VulkanProbeStatus::DeviceCreationFailed: return "device creation failed";
	case VulkanProbeStatus::CleanupFailed: return "cleanup failed";
	case VulkanProbeStatus::ValidationErrorsReported: return "validation errors reported";
	case VulkanProbeStatus::SurfaceUnsupported: return "surface unsupported";
	case VulkanProbeStatus::SurfaceCreationFailed: return "surface creation failed";
	case VulkanProbeStatus::SurfaceCapabilitiesFailed: return "surface capabilities failed";
	case VulkanProbeStatus::PresentationUnsupported: return "presentation unsupported";
	case VulkanProbeStatus::SurfaceDeviceSelectionFailed: return "surface device selection failed";
	}
	return "unknown";
}

static int DeviceRank(const String& type)
{
	if(type == "discrete") return 3;
	if(type == "integrated") return 2;
	if(type == "other") return 1;
	return 0;
}

static int QueueRank(const VulkanQueueFamilyInfo& family)
{
	if(!family.graphics || family.count == 0)
		return -1;
	return 1 + (family.compute ? 1 : 0) + (family.transfer ? 1 : 0);
}

static String SurfaceFormatText(VkFormat format)
{
	switch(format) {
	case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
	case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
	case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
	case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
	default: return String("format(") + AsString((int)format) + ")";
	}
}

static String ColorSpaceText(VkColorSpaceKHR color_space)
{
	switch(color_space) {
	case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "SRGB_NONLINEAR";
#ifdef VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT
	case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "DISPLAY_P3_NONLINEAR";
#endif
	default: return String("colorspace(") + AsString((int)color_space) + ")";
	}
}

static String PresentModeText(VkPresentModeKHR mode)
{
	switch(mode) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
	case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
	case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
#ifdef VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR
	case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR: return "SHARED_DEMAND_REFRESH";
#endif
#ifdef VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR
	case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR: return "SHARED_CONTINUOUS_REFRESH";
#endif
	default: return String("present(") + AsString((int)mode) + ")";
	}
}

static String SurfaceTransformText(VkSurfaceTransformFlagsKHR flags)
{
	static const char *const names[] = {
		"Identity",
		"Rotate90",
		"Rotate180",
		"Rotate270",
		"HorizontalMirror",
		"HorizontalMirrorRotate90",
		"HorizontalMirrorRotate180",
		"HorizontalMirrorRotate270",
		"Inherited",
	};
	static const int bits[] = {
		VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
		VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
		VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
		VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR,
		VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR,
		VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR,
		VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR,
		VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR,
	};
	return DumpFlags((int)flags, names, bits, 9);
}

static String CompositeAlphaText(VkCompositeAlphaFlagsKHR flags)
{
	static const char *const names[] = { "Opaque", "PreMultiplied", "PostMultiplied", "Inherit" };
	static const int bits[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	return DumpFlags((int)flags, names, bits, 4);
}

static String ImageUsageText(VkImageUsageFlags flags)
{
	static const char *const names[] = { "TransferSrc", "TransferDst", "Sampled", "Storage", "ColorAttachment", "DepthStencilAttachment", "TransientAttachment", "InputAttachment" };
	static const int bits[] = {
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
	};
	return DumpFlags((int)flags, names, bits, 8);
}

static bool IsSuitableDevice(const VulkanDeviceInfo& device)
{
	return device.api_version >= VK_API_VERSION_1_3 && device.graphics_queue && device.dynamic_rendering && device.synchronization2;
}

static VulkanDiscoveredDevice* PickSelectedDevice(Vector<VulkanDiscoveredDevice>& devices)
{
	VulkanDiscoveredDevice *best = nullptr;
	int best_rank = -1;
	int best_queue = -1;
	for(auto& device : devices) {
		if(!IsSuitableDevice(device.info))
			continue;
		int rank = DeviceRank(device.info.type);
		if(rank < 0)
			continue;
		int queue_index = -1;
		int queue_rank = -1;
		for(const auto& family : device.info.queue_families) {
			int qr = QueueRank(family);
			if(qr > queue_rank || (qr == queue_rank && family.index < queue_index)) {
				queue_rank = qr;
				queue_index = family.index;
			}
		}
		if(queue_index < 0)
			continue;
		if(rank > best_rank || (rank == best_rank && queue_rank > best_queue)) {
			best = &device;
			best_rank = rank;
			best_queue = queue_rank;
		}
	}
	if(best) {
		int queue_index = -1;
		int queue_rank = -1;
		for(const auto& family : best->info.queue_families) {
			int qr = QueueRank(family);
			if(qr > queue_rank || (qr == queue_rank && family.index < queue_index)) {
				queue_rank = qr;
				queue_index = family.index;
			}
		}
		best->info.selected_queue_family_index = queue_index;
		best->info.selected_queue_count = best->info.queue_families[queue_index].count;
		best->info.selected_queue_flags = best->info.queue_families[queue_index].flags;
		best->info.selected_queue_compute = best->info.queue_families[queue_index].compute;
		best->info.selected_queue_transfer = best->info.queue_families[queue_index].transfer;
		best->info.selection_reason = best_rank >= 2 ? (best_rank == 3 ? "preferred discrete GPU" : "preferred integrated GPU") : "first suitable device in enumeration order";
	}
	return best;
}

struct VulkanSurfaceSelection {
	VulkanDiscoveredDevice *device = nullptr;
	int graphics_family = -1;
	int present_family = -1;
	bool same_family = false;
	int graphics_score = -1;
	int present_score = -1;
};

static bool ChooseSurfaceDevice(Vector<VulkanDiscoveredDevice>& devices, VulkanSurfaceSelection& choice)
{
	VulkanDiscoveredDevice *best_combined = nullptr;
	int best_combined_rank = -1;
	int best_combined_score = -1;
	for(auto& device : devices) {
		if(!IsSuitableDevice(device.info) || !HasExtension(device.info.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
			continue;
		for(const auto& family : device.info.queue_families) {
			if(!(family.graphics && family.present && family.count > 0))
				continue;
			int score = QueueRank(family) + 10;
			int rank = DeviceRank(device.info.type);
			if(rank > best_combined_rank || (rank == best_combined_rank && (score > best_combined_score || (score == best_combined_score && device.info.selected_queue_family_index < 0)))) {
				best_combined = &device;
				best_combined_rank = rank;
				best_combined_score = score;
			}
		}
	}
	if(best_combined) {
		int family_index = -1;
		int family_score = -1;
		for(const auto& family : best_combined->info.queue_families) {
			if(!(family.graphics && family.present && family.count > 0))
				continue;
			int score = QueueRank(family);
			if(score > family_score || (score == family_score && family.index < family_index)) {
				family_score = score;
				family_index = family.index;
			}
		}
		if(family_index < 0)
			return false;
		choice.device = best_combined;
		choice.graphics_family = family_index;
		choice.present_family = family_index;
		choice.same_family = true;
		choice.graphics_score = family_score;
		choice.present_score = family_score;
		best_combined->info.selected_queue_family_index = family_index;
		best_combined->info.selected_queue_count = best_combined->info.queue_families[family_index].count;
		best_combined->info.selected_queue_flags = best_combined->info.queue_families[family_index].flags;
		best_combined->info.selected_queue_compute = best_combined->info.queue_families[family_index].compute;
		best_combined->info.selected_queue_transfer = best_combined->info.queue_families[family_index].transfer;
		best_combined->info.selection_reason = best_combined_rank >= 2 ? (best_combined_rank == 3 ? "preferred discrete GPU with graphics+present queue" : "preferred integrated GPU with graphics+present queue") : "first suitable device in enumeration order";
		return true;
	}

	VulkanDiscoveredDevice *best = nullptr;
	int best_rank = -1;
	int best_graphics_score = -1;
	int best_present_score = -1;
	int best_graphics_family = -1;
	int best_present_family = -1;
	for(auto& device : devices) {
		if(!IsSuitableDevice(device.info) || !HasExtension(device.info.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
			continue;
		int graphics_family = -1;
		int graphics_score = -1;
		int present_family = -1;
		int present_score = -1;
		for(const auto& family : device.info.queue_families) {
			if(family.graphics && family.count > 0) {
				int score = QueueRank(family);
				if(score > graphics_score || (score == graphics_score && family.index < graphics_family)) {
					graphics_score = score;
					graphics_family = family.index;
				}
			}
			if(family.present && family.count > 0) {
				int score = QueueRank(family);
				if(score > present_score || (score == present_score && family.index < present_family)) {
					present_score = score;
					present_family = family.index;
				}
			}
		}
		if(graphics_family < 0 || present_family < 0)
			continue;
		int rank = DeviceRank(device.info.type);
		if(rank > best_rank || (rank == best_rank && (graphics_score + present_score > best_graphics_score + best_present_score))) {
			best = &device;
			best_rank = rank;
			best_graphics_score = graphics_score;
			best_present_score = present_score;
			best_graphics_family = graphics_family;
			best_present_family = present_family;
		}
	}
	if(!best)
		return false;
	choice.device = best;
	choice.graphics_family = best_graphics_family;
	choice.present_family = best_present_family;
	choice.same_family = best_graphics_family == best_present_family;
	choice.graphics_score = best_graphics_score;
	choice.present_score = best_present_score;
	best->info.selected_queue_family_index = best_graphics_family;
	best->info.selected_queue_count = best->info.queue_families[best_graphics_family].count;
	best->info.selected_queue_flags = best->info.queue_families[best_graphics_family].flags;
	best->info.selected_queue_compute = best->info.queue_families[best_graphics_family].compute;
	best->info.selected_queue_transfer = best->info.queue_families[best_graphics_family].transfer;
	best->info.selection_reason = best_rank >= 2 ? (best_rank == 3 ? "preferred discrete GPU with separate graphics/present queues" : "preferred integrated GPU with separate graphics/present queues") : "first suitable device in enumeration order";
	return true;
}

static void AppendPreflightDump(String& out, const VulkanPreflightReport& report)
{
	out << "Vulkan preflight summary\n";
	out << "Status: " << report.status_text << '\n';
	out << "Loader/runtime available: " << BoolText(report.loader_available) << '\n';
	if(report.loader_available)
		out << "Loader API version: " << FormatVersion(report.loader_version) << '\n';
	out << "Validation layer available: " << BoolText(report.validation_available) << '\n';
	out << "Debug utils available: " << BoolText(report.debug_utils_available) << '\n';
	out << "Validation requested: " << BoolText(report.validation_requested) << '\n';
	out << "Validation warnings: " << AsString(report.validation_warning_count) << '\n';
	out << "Validation errors: " << AsString(report.validation_error_count) << '\n';
	out << "Instance created: " << BoolText(report.instance_created) << '\n';
	out << "Clean shutdown: " << BoolText(report.clean_shutdown) << '\n';
	out << "Cleanup state cleared: " << BoolText(report.cleanup_state_cleared) << '\n';
	if(!report.runtime_error.IsEmpty())
		out << "Runtime error: " << report.runtime_error << '\n';
	if(!report.loader_error.IsEmpty())
		out << "Loader error: " << report.loader_error << '\n';
	if(!report.layer_error.IsEmpty())
		out << "Layer error: " << report.layer_error << '\n';
	if(!report.extension_error.IsEmpty())
		out << "Extension error: " << report.extension_error << '\n';
	if(!report.instance_error.IsEmpty())
		out << "Instance error: " << report.instance_error << '\n';
	if(!report.physical_device_error.IsEmpty())
		out << "Physical device error: " << report.physical_device_error << '\n';
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
	for(const String& msg : report.validation_messages)
		out << "Validation: " << msg << '\n';
	out << "VulkanProbe " << (report.status == VulkanProbeStatus::Ok ? "passed" : "failed") << '\n';
}

static void FinalizePreflightCleanup(VulkanPreflightReport& report, const VulkanDispatch& dispatch, const VulkanInstanceContext& instance, bool cleanup_ok)
{
	report.cleanup_state_cleared = dispatch.IsCleared() && instance.IsCleared();
	report.clean_shutdown = cleanup_ok && report.cleanup_state_cleared;
}

static void FinalizeBootstrapCleanup(VulkanBootstrapReport& report, const VulkanDispatch& dispatch, const VulkanInstanceContext& instance, const VulkanDeviceContext& device, bool cleanup_ok)
{
	report.cleanup_state_cleared = dispatch.IsCleared() && instance.IsCleared() && device.IsCleared();
	report.clean_shutdown = cleanup_ok && report.cleanup_state_cleared;
}

static bool CloseBootstrapContexts(VulkanDispatch& dispatch, VulkanInstanceContext& instance, VulkanDeviceContext& device)
{
	bool ok = device.Close();
	ok = instance.Close() && ok;
	ok = dispatch.Close() && ok;
	return ok;
}

static VulkanProbeStatus MapInstanceError(const String& error)
{
	if(error == "VK_LAYER_KHRONOS_validation not present")
		return VulkanProbeStatus::ValidationUnavailable;
	if(error == "VK_EXT_debug_utils not present")
		return VulkanProbeStatus::DebugUtilsUnavailable;
	if(error.StartsWith("instance layer count query failed") || error.StartsWith("instance layer enumeration failed"))
		return VulkanProbeStatus::LayerEnumerationFailed;
	if(error.StartsWith("instance extension count query failed") || error.StartsWith("instance extension enumeration failed"))
		return VulkanProbeStatus::ExtensionEnumerationFailed;
	if(error.StartsWith("vkCreateInstance failed"))
		return VulkanProbeStatus::InstanceCreationFailed;
	return VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
}

static VulkanProbeStatus MapDeviceError(const String& error)
{
	if(error == "vkDestroyDevice" || error == "vkGetDeviceQueue" || error == "vkDeviceWaitIdle")
		return VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
	if(error.StartsWith("physical device count query failed") || error.StartsWith("physical device enumeration failed"))
		return VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
	if(error.StartsWith("queue family"))
		return VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
	if(error.StartsWith("device extension"))
		return VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
	if(error.StartsWith("vkCreateDevice failed"))
		return VulkanProbeStatus::DeviceCreationFailed;
	if(error == "vkGetDeviceQueue returned VK_NULL_HANDLE")
		return VulkanProbeStatus::DeviceCreationFailed;
	return VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
}

static VulkanProbeStatus MapDispatchError(const String& error)
{
	if(error == "LoadLibraryW(vulkan-1.dll) failed")
		return VulkanProbeStatus::RuntimeUnavailable;
	return VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
}

static bool CleanupFailed(const VulkanBootstrapReport& report, bool create_device)
{
	return !report.instance_cleanup_ok || !report.dispatch_cleanup_ok || (create_device && !report.device_cleanup_ok);
}

static void FinalizeBootstrapStatus(VulkanBootstrapReport& report, bool create_device)
{
	if(report.validation_error_count > 0)
		report.validation_error = "validation errors reported";
	bool cleanup_failed = CleanupFailed(report, create_device);
	if(cleanup_failed && (report.status == VulkanProbeStatus::Ok || report.status == VulkanProbeStatus::ValidationErrorsReported))
		report.status = VulkanProbeStatus::CleanupFailed;
	else if(report.status == VulkanProbeStatus::Ok && report.validation_error_count > 0)
		report.status = VulkanProbeStatus::ValidationErrorsReported;
	report.status_text = StatusText(report.status);
	report.preflight.status = report.status;
	report.preflight.status_text = report.status_text;
}

} // namespace

namespace VulkanTestHooks {

void SetVulkanValidationTestInjection(const VulkanValidationTestInjection& injection)
{
	g_validation_test_injection = injection;
}

void ClearVulkanValidationTestInjection()
{
	g_validation_test_injection = VulkanValidationTestInjection();
}

VulkanRuntimeDeviceDiagnostics GetVulkanRuntimeDeviceDiagnostics()
{
	VulkanRuntimeDeviceDiagnostics diag;
	diag.runtime_create_count = g_runtime_device_stats.runtime_create_count.load(std::memory_order_relaxed);
	diag.runtime_live_count = g_runtime_device_stats.runtime_live_count.load(std::memory_order_relaxed);
	diag.runtime_id = g_runtime_device_stats.runtime_last_id.load(std::memory_order_relaxed);
	diag.instance_create_count = g_runtime_device_stats.instance_create_count.load(std::memory_order_relaxed);
	diag.instance_live_count = g_runtime_device_stats.instance_live_count.load(std::memory_order_relaxed);
	diag.instance_id = g_runtime_device_stats.instance_last_id.load(std::memory_order_relaxed);
	diag.debug_messenger_create_count = g_runtime_device_stats.debug_messenger_create_count.load(std::memory_order_relaxed);
	diag.debug_messenger_live_count = g_runtime_device_stats.debug_messenger_live_count.load(std::memory_order_relaxed);
	diag.physical_device_discovery_count = g_runtime_device_stats.physical_device_discovery_count.load(std::memory_order_relaxed);
	diag.device_create_count = g_runtime_device_stats.device_create_count.load(std::memory_order_relaxed);
	diag.device_live_count = g_runtime_device_stats.device_live_count.load(std::memory_order_relaxed);
	diag.device_id = g_runtime_device_stats.device_last_id.load(std::memory_order_relaxed);
	diag.surface_create_count = g_runtime_device_stats.surface_create_count.load(std::memory_order_relaxed);
	diag.surface_live_count = g_runtime_device_stats.surface_live_count.load(std::memory_order_relaxed);
	diag.surface_id = g_runtime_device_stats.surface_last_id.load(std::memory_order_relaxed);
	return diag;
}

void ClearVulkanRuntimeDeviceDiagnostics()
{
	g_runtime_device_stats.runtime_create_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.runtime_live_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.runtime_next_id.store(1, std::memory_order_relaxed);
	g_runtime_device_stats.runtime_last_id.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.instance_next_id.store(1, std::memory_order_relaxed);
	g_runtime_device_stats.instance_last_id.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.instance_create_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.instance_live_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.debug_messenger_create_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.debug_messenger_live_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.physical_device_discovery_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.device_create_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.device_live_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.device_next_id.store(1, std::memory_order_relaxed);
	g_runtime_device_stats.device_last_id.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.surface_create_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.surface_live_count.store(0, std::memory_order_relaxed);
	g_runtime_device_stats.surface_next_id.store(1, std::memory_order_relaxed);
	g_runtime_device_stats.surface_last_id.store(0, std::memory_order_relaxed);
}

} // namespace VulkanTestHooks

VulkanPreflight::VulkanPreflight()
{
}

String VulkanPreflight::BoolText(bool value) { return ::Upp::BoolText(value); }
String VulkanPreflight::StatusText(VulkanProbeStatus status) { return ::Upp::StatusText(status); }
String VulkanPreflight::FormatVersion(uint32_t version) { return ::Upp::FormatVersion(version); }
String VulkanPreflight::DeviceTypeText(VkPhysicalDeviceType type) { return ::Upp::DeviceTypeText(type); }
String VulkanPreflight::QueueFlagsText(VkQueueFlags flags) { return ::Upp::QueueFlagsText(flags); }
String VulkanPreflight::LayerName(const VkLayerProperties& prop) { return ::Upp::LayerName(prop); }
String VulkanPreflight::ExtensionName(const VkExtensionProperties& prop) { return ::Upp::ExtensionName(prop); }
uint32_t VulkanPreflight::LayerVersionToUInt(const VkLayerProperties& prop) { return ::Upp::LayerVersionToUInt(prop); }
uint32_t VulkanPreflight::ExtensionVersionToUInt(const VkExtensionProperties& prop) { return ::Upp::ExtensionVersionToUInt(prop); }
bool VulkanPreflight::HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name) { return ::Upp::HasExtension(extensions, name); }
void VulkanPreflight::AppendMissing(VulkanDeviceInfo& device, const char *text) { ::Upp::AppendMissing(device, text); }

VulkanPreflightReport VulkanPreflight::Run(bool request_validation)
{
	return Run(request_validation, nullptr);
}

VulkanPreflightReport VulkanPreflight::Run(bool request_validation, VulkanProcResolver resolver)
{
	VulkanBootstrap bootstrap;
	VulkanBootstrapReport bootstrap_report = bootstrap.Run(request_validation, false, resolver);
	VulkanPreflightReport report = pick(bootstrap_report.preflight);
	if(bootstrap_report.status == VulkanProbeStatus::ValidationErrorsReported) {
		report.status = VulkanProbeStatus::ValidationErrorsReported;
		report.status_text = StatusText(report.status);
	}
	return report;
}

String VulkanPreflight::Dump(const VulkanPreflightReport& report) const
{
	String out;
	AppendPreflightDump(out, report);
	return out;
}

VulkanBootstrap::VulkanBootstrap()
{
}

String VulkanBootstrap::BoolText(bool value) { return ::Upp::BoolText(value); }
String VulkanBootstrap::StatusText(VulkanProbeStatus status) { return ::Upp::StatusText(status); }
String VulkanBootstrap::FormatVersion(uint32_t version) { return ::Upp::FormatVersion(version); }
String VulkanBootstrap::DeviceTypeText(VkPhysicalDeviceType type) { return ::Upp::DeviceTypeText(type); }
String VulkanBootstrap::QueueFlagsText(VkQueueFlags flags) { return ::Upp::QueueFlagsText(flags); }
String VulkanBootstrap::LayerName(const VkLayerProperties& prop) { return ::Upp::LayerName(prop); }
String VulkanBootstrap::ExtensionName(const VkExtensionProperties& prop) { return ::Upp::ExtensionName(prop); }
uint32_t VulkanBootstrap::LayerVersionToUInt(const VkLayerProperties& prop) { return ::Upp::LayerVersionToUInt(prop); }
uint32_t VulkanBootstrap::ExtensionVersionToUInt(const VkExtensionProperties& prop) { return ::Upp::ExtensionVersionToUInt(prop); }
bool VulkanBootstrap::HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name) { return ::Upp::HasExtension(extensions, name); }
bool VulkanBootstrap::IsSuitableDevice(const VulkanDeviceInfo& device) { return ::Upp::IsSuitableDevice(device); }
int VulkanBootstrap::DeviceRank(VkPhysicalDeviceType type) { return type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : type == VK_PHYSICAL_DEVICE_TYPE_OTHER ? 1 : 0; }
int VulkanBootstrap::QueueRank(const VulkanQueueFamilyInfo& family) { return ::Upp::QueueRank(family); }
String VulkanBootstrap::SanitizeValidationMessage(const String& text) { return ::Upp::SanitizeValidationMessage(text); }

VulkanPreflightReport VulkanBootstrap::BuildPreflight(bool request_validation, bool, bool, VulkanProcResolver resolver)
{
	VulkanBootstrapReport report;
	BuildBootstrap(report, request_validation, false, resolver);
	VulkanPreflightReport preflight = pick(report.preflight);
	if(report.status == VulkanProbeStatus::ValidationErrorsReported) {
		preflight.status = VulkanProbeStatus::ValidationErrorsReported;
		preflight.status_text = StatusText(preflight.status);
	}
	return preflight;
}

bool VulkanBootstrap::BuildBootstrap(VulkanBootstrapReport& report, bool request_validation, bool create_device, VulkanProcResolver resolver)
{
	report = VulkanBootstrapReport();
	report.validation_requested = request_validation;
	report.create_device_requested = create_device;
	report.preflight.validation_requested = request_validation;

	VulkanDispatch dispatch;
	VulkanInstanceContext instance;
	VulkanDeviceContext device;
	String error;
	if(!dispatch.Open(error, resolver)) {
		report.status = MapDispatchError(error);
		if(report.status == VulkanProbeStatus::RuntimeUnavailable)
			report.runtime_error = error;
		else
			report.loader_error = error;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	uint32_t loader_version = VK_API_VERSION_1_0;
	if(!QueryLoaderVersion(dispatch.enumerate_instance_version, loader_version, error)) {
		report.status = VulkanProbeStatus::LoaderTooOld;
		report.loader_error = error;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}
	report.preflight.loader_available = true;
	report.preflight.loader_version = loader_version;
	if(loader_version < VK_API_VERSION_1_3) {
		report.status = VulkanProbeStatus::LoaderTooOld;
		report.loader_error = "loader api version older than Vulkan 1.3";
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	if(!instance.Open(dispatch, request_validation, report.preflight, report, error)) {
		report.status = MapInstanceError(error);
		report.instance_error = error;
		report.status_text = StatusText(report.status);
		report.validation_available = report.preflight.validation_available;
		report.debug_utils_available = report.preflight.debug_utils_available;
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	report.preflight.instance_created = true;
	report.validation_available = report.preflight.validation_available;
	report.debug_utils_available = report.preflight.debug_utils_available;

	Vector<VulkanDiscoveredDevice> discovered;
	if(!instance.EnumeratePhysicalDevices(discovered, error)) {
		report.status = MapDeviceError(error);
		report.physical_device_error = error;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}
	if(discovered.IsEmpty()) {
		report.status = VulkanProbeStatus::NoPhysicalDevices;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	VulkanDiscoveredDevice *selected = PickSelectedDevice(discovered);
	if(!selected) {
		report.status = VulkanProbeStatus::NoSuitableDevices;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, dispatch, instance, device, CloseBootstrapContexts(dispatch, instance, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	for(auto& found : discovered) {
	report.preflight.devices.Add();
		CloneDeviceInfo(report.preflight.devices.Top(), found.info);
		if(found.info.suitable)
			report.preflight.suitable_device_count += 1;
	}
	report.preflight.status = report.preflight.suitable_device_count > 0 ? VulkanProbeStatus::Ok : VulkanProbeStatus::NoSuitableDevices;
	report.preflight.status_text = StatusText(report.preflight.status);
	CloneDeviceInfo(report.selected_device, selected->info);

	report.validation_warning_count = instance.capture.warnings;
	report.validation_error_count = instance.capture.errors;
	CopyMessages(report.validation_messages, instance.capture.messages);
	report.preflight.validation_warning_count = report.validation_warning_count;
	report.preflight.validation_error_count = report.validation_error_count;
	CopyMessages(report.preflight.validation_messages, report.validation_messages);
	report.status = report.validation_error_count > 0 ? VulkanProbeStatus::ValidationErrorsReported : VulkanProbeStatus::Ok;
	report.status_text = StatusText(report.status);
	report.preflight.status = report.status;
	report.preflight.status_text = report.status_text;

	if(request_validation)
		InjectValidationIfRequested(instance.capture, VulkanValidationTestPoint::BeforeDeviceCreation);
	if(create_device) {
		if(!device.Open(instance, selected->handle, report.selected_device, report, error)) {
			report.status = MapDeviceError(error);
			report.device_error = error;
			report.device_cleanup_ok = device.Close();
			report.device_cleanup_result = device.cleanup_result;
			report.device_cleanup_error = device.cleanup_error;
			CopyValidationCapture(report, instance.capture);
			CopyMessages(report.preflight.validation_messages, report.validation_messages);
			report.preflight.validation_warning_count = report.validation_warning_count;
			report.preflight.validation_error_count = report.validation_error_count;
			report.preflight.debug_utils_available = instance.debug_utils_available;
			report.preflight.validation_available = report.validation_available;
			report.instance_cleanup_ok = instance.Close();
			report.dispatch_cleanup_ok = dispatch.Close();
			FinalizeBootstrapStatus(report, create_device);
			FinalizeBootstrapCleanup(report, dispatch, instance, device, report.instance_cleanup_ok && report.dispatch_cleanup_ok && report.device_cleanup_ok);
			report.preflight.clean_shutdown = report.clean_shutdown;
			report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
			return false;
		}
		report.logical_device_created = true;
		report.graphics_queue_acquired = true;
		report.selected_device.logical_device_created = true;
		report.selected_device.graphics_queue_acquired = true;
		if(request_validation) {
			InjectValidationIfRequested(instance.capture, VulkanValidationTestPoint::AfterDeviceCreation);
			InjectValidationIfRequested(instance.capture, VulkanValidationTestPoint::DuringDeviceCleanup);
		}
		report.device_cleanup_ok = device.Close();
		report.device_cleanup_result = device.cleanup_result;
		report.device_cleanup_error = device.cleanup_error;
	}

	CopyValidationCapture(report, instance.capture);
	CopyMessages(report.preflight.validation_messages, report.validation_messages);
	report.preflight.validation_warning_count = report.validation_warning_count;
	report.preflight.validation_error_count = report.validation_error_count;
	report.preflight.debug_utils_available = instance.debug_utils_available;
	report.preflight.validation_available = report.validation_available;
	report.instance_cleanup_ok = instance.Close();
	report.dispatch_cleanup_ok = dispatch.Close();
	FinalizeBootstrapStatus(report, create_device);
	FinalizeBootstrapCleanup(report, dispatch, instance, device, report.instance_cleanup_ok && report.dispatch_cleanup_ok && (!create_device || report.device_cleanup_ok));
	report.preflight.clean_shutdown = report.clean_shutdown;
	report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
	return report.status == VulkanProbeStatus::Ok || report.status == VulkanProbeStatus::ValidationErrorsReported;
}

VulkanBootstrapReport VulkanBootstrap::Run(bool request_validation, bool create_device)
{
	return Run(request_validation, create_device, nullptr);

}

VulkanBootstrapReport VulkanBootstrap::Run(bool request_validation, bool create_device, VulkanProcResolver resolver)
{
	VulkanBootstrapReport report;
	BuildBootstrap(report, request_validation, create_device, resolver);
	return pick(report);
}

String VulkanBootstrap::Dump(const VulkanBootstrapReport& report) const
{
	String out;
	AppendPreflightDump(out, report.preflight);
	if(report.create_device_requested) {
		out << "Logical device created: " << BoolText(report.logical_device_created) << '\n';
		out << "Selected device: " << report.selected_device.name << '\n';
		out << "Selected device type: " << report.selected_device.type << '\n';
		out << "Selected device selection reason: " << report.selected_device.selection_reason << '\n';
		out << "Selected device vendor ID: " << AsString(report.selected_device.vendor_id) << '\n';
		out << "Selected device device ID: " << AsString(report.selected_device.device_id) << '\n';
		out << "Selected device API version: " << FormatVersion(report.selected_device.api_version) << '\n';
		out << "Selected device driver version: " << AsString(report.selected_device.driver_version) << '\n';
		out << "Selected graphics queue family: " << AsString(report.selected_device.selected_queue_family_index) << '\n';
		out << "Graphics queue acquired: " << BoolText(report.graphics_queue_acquired) << '\n';
		out << "Dynamic Rendering enabled: " << BoolText(report.selected_device.dynamic_rendering) << '\n';
		out << "Synchronization2 enabled: " << BoolText(report.selected_device.synchronization2) << '\n';
		out << "Validation warnings: " << AsString(report.validation_warning_count) << '\n';
		out << "Validation errors: " << AsString(report.validation_error_count) << '\n';
		if(!report.device_cleanup_error.IsEmpty())
			out << "Device cleanup error: " << report.device_cleanup_error << '\n';
		if(report.device_cleanup_result != 0)
			out << "Device cleanup VkResult: " << AsString(report.device_cleanup_result) << '\n';
		out << "Clean shutdown: " << BoolText(report.clean_shutdown) << '\n';
		out << "Bootstrap status: " << report.status_text << '\n';
	}
	else if(report.validation_requested) {
		out << "Validation warnings: " << AsString(report.validation_warning_count) << '\n';
		out << "Validation errors: " << AsString(report.validation_error_count) << '\n';
	}
	if(!report.runtime_error.IsEmpty())
		out << "Runtime error: " << report.runtime_error << '\n';
	if(!report.loader_error.IsEmpty())
		out << "Loader error: " << report.loader_error << '\n';
	if(!report.validation_error.IsEmpty())
		out << "Validation error: " << report.validation_error << '\n';
	if(!report.instance_error.IsEmpty())
		out << "Instance error: " << report.instance_error << '\n';
	if(!report.device_error.IsEmpty())
		out << "Device error: " << report.device_error << '\n';
	for(const String& msg : report.validation_messages)
		out << "Validation: " << msg << '\n';
	return out;
}

VulkanSurfaceProbe::VulkanSurfaceProbe()
{
}

String VulkanSurfaceProbe::BoolText(bool value) { return ::Upp::BoolText(value); }
String VulkanSurfaceProbe::StatusText(VulkanProbeStatus status) { return ::Upp::StatusText(status); }
String VulkanSurfaceProbe::FormatVersion(uint32_t version) { return ::Upp::FormatVersion(version); }
String VulkanSurfaceProbe::DeviceTypeText(VkPhysicalDeviceType type) { return ::Upp::DeviceTypeText(type); }
String VulkanSurfaceProbe::QueueFlagsText(VkQueueFlags flags) { return ::Upp::QueueFlagsText(flags); }
String VulkanSurfaceProbe::LayerName(const VkLayerProperties& prop) { return ::Upp::LayerName(prop); }
String VulkanSurfaceProbe::ExtensionName(const VkExtensionProperties& prop) { return ::Upp::ExtensionName(prop); }
uint32_t VulkanSurfaceProbe::LayerVersionToUInt(const VkLayerProperties& prop) { return ::Upp::LayerVersionToUInt(prop); }
uint32_t VulkanSurfaceProbe::ExtensionVersionToUInt(const VkExtensionProperties& prop) { return ::Upp::ExtensionVersionToUInt(prop); }
bool VulkanSurfaceProbe::HasExtension(const Vector<VulkanExtensionInfo>& extensions, const char *name) { return ::Upp::HasExtension(extensions, name); }
bool VulkanSurfaceProbe::IsSuitableDevice(const VulkanDeviceInfo& device) { return ::Upp::IsSuitableDevice(device); }
int VulkanSurfaceProbe::DeviceRank(VkPhysicalDeviceType type) { return type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : type == VK_PHYSICAL_DEVICE_TYPE_OTHER ? 1 : 0; }
int VulkanSurfaceProbe::QueueRank(const VulkanQueueFamilyInfo& family) { return ::Upp::QueueRank(family); }
String VulkanSurfaceProbe::SanitizeValidationMessage(const String& text) { return ::Upp::SanitizeValidationMessage(text); }

static void FinalizeSurfaceCleanup(VulkanSurfaceReport& report, const VulkanDispatch& dispatch, const VulkanSurfaceContext& ctx, const VulkanDeviceContext& device, bool cleanup_ok)
{
	report.cleanup_state_cleared = dispatch.IsCleared() && ctx.IsCleared() && device.IsCleared();
	report.surface_cleanup_ok = ctx.surface == VK_NULL_HANDLE;
	report.clean_shutdown = cleanup_ok && report.cleanup_state_cleared;
}

static void CopySurfaceValidationCapture(VulkanSurfaceReport& report, const VulkanValidationCapture& capture)
{
	report.validation_warning_count = capture.warnings;
	report.validation_error_count = capture.errors;
	report.validation_messages.Clear();
	for(const String& msg : capture.messages)
		report.validation_messages.Add(msg);
	report.preflight.validation_warning_count = report.validation_warning_count;
	report.preflight.validation_error_count = report.validation_error_count;
	report.preflight.validation_messages.Clear();
	for(const String& msg : report.validation_messages)
		report.preflight.validation_messages.Add(msg);
}

template <class T>
static void CopyVector(Vector<T>& dst, const Vector<T>& src)
{
	dst.Clear();
	for(const auto& item : src)
		dst.Add(item);
}

VulkanDeviceInfo::VulkanDeviceInfo(const VulkanDeviceInfo& src)
{
	*this = src;
}

VulkanDeviceInfo& VulkanDeviceInfo::operator=(const VulkanDeviceInfo& src)
{
	if(this == &src)
		return *this;
	name = src.name;
	type = src.type;
	vendor_id = src.vendor_id;
	device_id = src.device_id;
	driver_version = src.driver_version;
	api_version = src.api_version;
	graphics_queue = src.graphics_queue;
	dynamic_rendering = src.dynamic_rendering;
	synchronization2 = src.synchronization2;
	suitable = src.suitable;
	selection_reason = src.selection_reason;
	selected_queue_family_index = src.selected_queue_family_index;
	selected_queue_count = src.selected_queue_count;
	selected_queue_flags = src.selected_queue_flags;
	selected_queue_compute = src.selected_queue_compute;
	selected_queue_transfer = src.selected_queue_transfer;
	logical_device_created = src.logical_device_created;
	graphics_queue_acquired = src.graphics_queue_acquired;
	CopyVector(missing_requirements, src.missing_requirements);
	CopyVector(queue_families, src.queue_families);
	CopyVector(device_extensions, src.device_extensions);
	return *this;
}

VulkanPreflightReport::VulkanPreflightReport(const VulkanPreflightReport& src)
{
	*this = src;
}

VulkanPreflightReport& VulkanPreflightReport::operator=(const VulkanPreflightReport& src)
{
	if(this == &src)
		return *this;
	status = src.status;
	status_text = src.status_text;
	loader_available = src.loader_available;
	loader_version = src.loader_version;
	validation_requested = src.validation_requested;
	validation_available = src.validation_available;
	debug_utils_available = src.debug_utils_available;
	instance_created = src.instance_created;
	clean_shutdown = src.clean_shutdown;
	cleanup_state_cleared = src.cleanup_state_cleared;
	validation_warning_count = src.validation_warning_count;
	validation_error_count = src.validation_error_count;
	CopyVector(validation_messages, src.validation_messages);
	runtime_error = src.runtime_error;
	loader_error = src.loader_error;
	layer_error = src.layer_error;
	extension_error = src.extension_error;
	instance_error = src.instance_error;
	physical_device_error = src.physical_device_error;
	CopyVector(instance_layers, src.instance_layers);
	CopyVector(instance_extensions, src.instance_extensions);
	CopyVector(devices, src.devices);
	suitable_device_count = src.suitable_device_count;
	return *this;
}

VulkanBootstrapReport::VulkanBootstrapReport(const VulkanBootstrapReport& src)
{
	*this = src;
}

VulkanBootstrapReport& VulkanBootstrapReport::operator=(const VulkanBootstrapReport& src)
{
	if(this == &src)
		return *this;
	status = src.status;
	status_text = src.status_text;
	preflight = src.preflight;
	validation_requested = src.validation_requested;
	create_device_requested = src.create_device_requested;
	validation_available = src.validation_available;
	debug_utils_available = src.debug_utils_available;
	debug_messenger_created = src.debug_messenger_created;
	logical_device_created = src.logical_device_created;
	graphics_queue_acquired = src.graphics_queue_acquired;
	clean_shutdown = src.clean_shutdown;
	cleanup_state_cleared = src.cleanup_state_cleared;
	instance_cleanup_ok = src.instance_cleanup_ok;
	device_cleanup_ok = src.device_cleanup_ok;
	dispatch_cleanup_ok = src.dispatch_cleanup_ok;
	validation_warning_count = src.validation_warning_count;
	validation_error_count = src.validation_error_count;
	CopyVector(validation_messages, src.validation_messages);
	device_cleanup_result = src.device_cleanup_result;
	device_cleanup_error = src.device_cleanup_error;
	runtime_error = src.runtime_error;
	loader_error = src.loader_error;
	validation_error = src.validation_error;
	instance_error = src.instance_error;
	physical_device_error = src.physical_device_error;
	device_error = src.device_error;
	selected_device = src.selected_device;
	return *this;
}

VulkanSurfaceReport::VulkanSurfaceReport(const VulkanSurfaceReport& src)
{
	*this = src;
}

VulkanSurfaceReport& VulkanSurfaceReport::operator=(const VulkanSurfaceReport& src)
{
	if(this == &src)
		return *this;
	status = src.status;
	status_text = src.status_text;
	preflight = src.preflight;
	validation_requested = src.validation_requested;
	surface_requested = src.surface_requested;
	validation_available = src.validation_available;
	debug_utils_available = src.debug_utils_available;
	surface_created = src.surface_created;
	logical_device_created = src.logical_device_created;
	graphics_queue_acquired = src.graphics_queue_acquired;
	present_queue_acquired = src.present_queue_acquired;
	same_queue_family = src.same_queue_family;
	swapchain_enabled = src.swapchain_enabled;
	clean_shutdown = src.clean_shutdown;
	cleanup_state_cleared = src.cleanup_state_cleared;
	instance_cleanup_ok = src.instance_cleanup_ok;
	surface_cleanup_ok = src.surface_cleanup_ok;
	device_cleanup_ok = src.device_cleanup_ok;
	dispatch_cleanup_ok = src.dispatch_cleanup_ok;
	validation_warning_count = src.validation_warning_count;
	validation_error_count = src.validation_error_count;
	CopyVector(validation_messages, src.validation_messages);
	runtime_error = src.runtime_error;
	loader_error = src.loader_error;
	validation_error = src.validation_error;
	instance_error = src.instance_error;
	surface_error = src.surface_error;
	physical_device_error = src.physical_device_error;
	device_error = src.device_error;
	native_window = src.native_window;
	selected_device = src.selected_device;
	graphics_queue_family_index = src.graphics_queue_family_index;
	present_queue_family_index = src.present_queue_family_index;
	graphics_queue_count = src.graphics_queue_count;
	present_queue_count = src.present_queue_count;
	graphics_queue_flags = src.graphics_queue_flags;
	present_queue_flags = src.present_queue_flags;
	CopyVector(surface_formats, src.surface_formats);
	CopyVector(present_modes, src.present_modes);
	min_image_count = src.min_image_count;
	max_image_count = src.max_image_count;
	current_extent = src.current_extent;
	min_extent = src.min_extent;
	max_extent = src.max_extent;
	supported_transforms = src.supported_transforms;
	current_transform = src.current_transform;
	supported_composite_alpha = src.supported_composite_alpha;
	supported_image_usage = src.supported_image_usage;
	preferred_bgra8 = src.preferred_bgra8;
	preferred_rgba8 = src.preferred_rgba8;
	preferred_srgb = src.preferred_srgb;
	preferred_mailbox = src.preferred_mailbox;
	preferred_fifo = src.preferred_fifo;
	return *this;
}

struct VulkanSurfaceSession::Impl {
	VulkanSharedInstanceHandle shared;
	VulkanSurfaceSessionContext ctx;
	VulkanDeviceContext device;
	VulkanSurfaceReport report;
	String error;
	bool open = false;
	bool ready = false;
};

VulkanSurfaceSession::VulkanSurfaceSession()
    : impl(new Impl)
{
}

VulkanSurfaceSession::~VulkanSurfaceSession()
{
	Close();
}

bool VulkanSurfaceSession::IsOpen() const
{
	return impl && impl->open;
}

bool VulkanSurfaceSession::IsReady() const
{
	return IsOpen() && impl->ready;
}

const VulkanSurfaceReport& VulkanSurfaceSession::GetReport() const
{
	return impl->report;
}

const String& VulkanSurfaceSession::GetError() const
{
	return impl->error;
}

static void FinalizeSurfaceSession(VulkanSurfaceSession::Impl& impl, bool cleanup_ok)
{
	impl.report.instance_cleanup_ok = impl.shared == nullptr;
	impl.report.surface_cleanup_ok = impl.ctx.surface == VK_NULL_HANDLE;
	impl.report.device_cleanup_ok = impl.device.device == VK_NULL_HANDLE;
	impl.report.dispatch_cleanup_ok = true;
	impl.report.native_window = GpuNativeWindowDesc();
	impl.report.cleanup_state_cleared = impl.shared == nullptr && impl.ctx.IsCleared() && impl.device.IsCleared();
	impl.report.clean_shutdown = cleanup_ok && impl.report.cleanup_state_cleared;
	impl.ready = false;
	impl.open = false;
}

bool VulkanSurfaceSession::Open(bool request_validation, const GpuNativeWindowDesc& native_window, VulkanProcResolver resolver)
{
	Close();
	impl->report = VulkanSurfaceReport();
	impl->error.Clear();
	impl->report.validation_requested = request_validation;
	impl->report.surface_requested = true;
	impl->report.native_window = native_window;
	impl->report.preflight.validation_requested = request_validation;
	impl->report.preflight.status = VulkanProbeStatus::RuntimeUnavailable;
	impl->report.preflight.status_text = StatusText(impl->report.preflight.status);

	auto fail = [&](const String& message) {
		impl->error = message;
		Close();
		return false;
	};

	impl->shared = AcquireSharedInstance(request_validation, resolver, impl->error);
	if(!impl->shared) {
		impl->report.status = impl->error == "LoadLibraryW(vulkan-1.dll) failed" ? VulkanProbeStatus::RuntimeUnavailable : impl->error == "loader api version older than Vulkan 1.3" ? VulkanProbeStatus::LoaderTooOld : impl->error == "VK_LAYER_KHRONOS_validation not present" ? VulkanProbeStatus::ValidationUnavailable : impl->error == "VK_EXT_debug_utils not present" ? VulkanProbeStatus::DebugUtilsUnavailable : impl->error.StartsWith("vkCreateInstance failed") ? VulkanProbeStatus::InstanceCreationFailed : VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
		impl->report.runtime_error = impl->report.status == VulkanProbeStatus::RuntimeUnavailable ? impl->error : String();
		impl->report.loader_error = impl->report.status == VulkanProbeStatus::LoaderTooOld ? impl->error : String();
		impl->report.instance_error = impl->report.status == VulkanProbeStatus::InstanceCreationFailed ? impl->error : String();
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->error);
	}

	impl->report.preflight = impl->shared->preflight;
	impl->report.preflight.validation_requested = request_validation;
	impl->report.preflight.status = VulkanProbeStatus::Ok;
	impl->report.preflight.status_text = StatusText(impl->report.preflight.status);
	impl->report.validation_available = impl->shared->preflight.validation_available;
	impl->report.debug_utils_available = impl->shared->preflight.debug_utils_available;

	if(native_window.kind != GpuNativeWindowKind::Win32) {
		impl->report.status = VulkanProbeStatus::SurfaceUnsupported;
		impl->report.instance_error = "surface requires Win32 native window";
		impl->report.surface_error = impl->report.instance_error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->report.instance_error);
	}
	if(native_window.handle == 0) {
		impl->report.status = VulkanProbeStatus::SurfaceUnsupported;
		impl->report.instance_error = "invalid native handle";
		impl->report.surface_error = impl->report.instance_error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->report.instance_error);
	}

	if(!impl->ctx.Open(*impl->shared, native_window, impl->error)) {
		impl->report.status = impl->error.StartsWith("vkCreateWin32SurfaceKHR failed") ? VulkanProbeStatus::SurfaceCreationFailed : VulkanProbeStatus::SurfaceUnsupported;
		impl->report.instance_error = impl->error;
		impl->report.surface_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->error);
	}
	impl->report.surface_created = true;

	Vector<VulkanDiscoveredDevice> discovered;
	if(!impl->shared->EnumeratePhysicalDevices(discovered, impl->error)) {
		impl->report.status = VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
		impl->report.physical_device_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->error);
	}
	if(discovered.IsEmpty()) {
		impl->report.status = VulkanProbeStatus::NoPhysicalDevices;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail("no physical devices");
	}

	impl->report.preflight.devices.Clear();
	impl->report.preflight.suitable_device_count = 0;
	for(auto& found : discovered) {
		for(auto& family : found.info.queue_families) {
			if(!impl->ctx.QueryPresentSupport(found.handle, family.index, family.present, impl->error)) {
				impl->report.status = VulkanProbeStatus::PresentationUnsupported;
				impl->report.physical_device_error = impl->error;
				impl->report.status_text = StatusText(impl->report.status);
				impl->report.preflight.status = impl->report.status;
				impl->report.preflight.status_text = impl->report.status_text;
				return fail(impl->error);
			}
		}
		impl->report.preflight.devices.Add();
		CloneDeviceInfo(impl->report.preflight.devices.Top(), found.info);
		if(found.info.suitable)
			impl->report.preflight.suitable_device_count += 1;
	}

	VulkanSurfaceSelection choice;
	if(!ChooseSurfaceDevice(discovered, choice)) {
		impl->report.status = VulkanProbeStatus::PresentationUnsupported;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail("presentation unsupported");
	}

	CloneDeviceInfo(impl->report.selected_device, choice.device->info);
	impl->report.graphics_queue_family_index = choice.graphics_family;
	impl->report.present_queue_family_index = choice.present_family;
	impl->report.same_queue_family = choice.same_family;
	impl->report.graphics_queue_count = impl->report.selected_device.queue_families[choice.graphics_family].count;
	impl->report.present_queue_count = impl->report.selected_device.queue_families[choice.present_family].count;
	impl->report.graphics_queue_flags = impl->report.selected_device.queue_families[choice.graphics_family].flags;
	impl->report.present_queue_flags = impl->report.selected_device.queue_families[choice.present_family].flags;
	impl->report.swapchain_enabled = HasExtension(impl->report.selected_device.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	if(!impl->ctx.QuerySurfaceCapabilities(choice.device->handle, impl->report, impl->error)) {
		impl->report.status = VulkanProbeStatus::SurfaceCapabilitiesFailed;
		impl->report.surface_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->error);
	}

	VkDeviceQueueCreateInfo qcis[2]{};
	float priority = 1.0f;
	int queue_info_count = 0;
	qcis[queue_info_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qcis[queue_info_count].queueFamilyIndex = (uint32_t)choice.graphics_family;
	qcis[queue_info_count].queueCount = 1;
	qcis[queue_info_count].pQueuePriorities = &priority;
	queue_info_count++;
	if(!choice.same_family) {
		qcis[queue_info_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qcis[queue_info_count].queueFamilyIndex = (uint32_t)choice.present_family;
		qcis[queue_info_count].queueCount = 1;
		qcis[queue_info_count].pQueuePriorities = &priority;
		queue_info_count++;
	}

	VkPhysicalDeviceVulkan13Features f13{};
	f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	f13.dynamicRendering = VK_TRUE;
	f13.synchronization2 = VK_TRUE;
	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &f13;
	Vector<const char*> enabled_exts;
	enabled_exts.Add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	VkDeviceCreateInfo dci{};
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.pNext = &features2;
	dci.queueCreateInfoCount = queue_info_count;
	dci.pQueueCreateInfos = qcis;
	dci.enabledExtensionCount = enabled_exts.GetCount();
	dci.ppEnabledExtensionNames = enabled_exts.Begin();

	if(!impl->device.Open(*impl->shared, choice.device->handle, impl->report.selected_device, impl->report, impl->error)) {
		impl->report.status = VulkanProbeStatus::DeviceCreationFailed;
		impl->report.device_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(impl->error);
	}
	impl->device.get_device_queue(impl->device.device, (uint32_t)choice.graphics_family, 0, &impl->device.graphics_queue);
	if(impl->device.graphics_queue == VK_NULL_HANDLE) {
		impl->error = "vkGetDeviceQueue returned VK_NULL_HANDLE";
		impl->report.status = VulkanProbeStatus::DeviceCreationFailed;
		impl->report.device_error = impl->error;
		return fail(impl->error);
	}
	if(choice.same_family)
		impl->device.present_queue = impl->device.graphics_queue;
	else {
		impl->device.get_device_queue(impl->device.device, (uint32_t)choice.present_family, 0, &impl->device.present_queue);
		if(impl->device.present_queue == VK_NULL_HANDLE) {
			impl->error = "vkGetDeviceQueue returned VK_NULL_HANDLE";
			impl->report.status = VulkanProbeStatus::DeviceCreationFailed;
			impl->report.device_error = impl->error;
			return fail(impl->error);
		}
	}

	impl->device.physical_device = choice.device->handle;
	impl->report.logical_device_created = true;
	impl->report.graphics_queue_acquired = true;
	impl->report.present_queue_acquired = true;
	impl->report.selected_device.logical_device_created = true;
	impl->report.selected_device.graphics_queue_acquired = true;
	impl->report.selected_device.selected_queue_family_index = choice.graphics_family;
	impl->report.selected_device.selected_queue_count = impl->report.selected_device.queue_families[choice.graphics_family].count;
	impl->report.selected_device.selected_queue_flags = impl->report.selected_device.queue_families[choice.graphics_family].flags;
	impl->report.selected_device.selected_queue_compute = impl->report.selected_device.queue_families[choice.graphics_family].compute;
	impl->report.selected_device.selected_queue_transfer = impl->report.selected_device.queue_families[choice.graphics_family].transfer;
	impl->report.status = VulkanProbeStatus::Ok;
	CopySurfaceValidationCapture(impl->report, impl->shared->capture);
	impl->report.preflight.debug_utils_available = impl->shared->preflight.debug_utils_available;
	impl->report.debug_utils_available = impl->shared->preflight.debug_utils_available;
	if(impl->report.validation_error_count > 0) {
		impl->report.status = VulkanProbeStatus::ValidationErrorsReported;
		impl->report.validation_error = "validation errors reported";
	}
	impl->report.status_text = StatusText(impl->report.status);
	impl->report.preflight.status = impl->report.status;
	impl->report.preflight.status_text = impl->report.status_text;
	impl->open = true;
	impl->ready = true;
	return true;
}

void VulkanSurfaceSession::Close()
{
	if(!impl)
		return;
	bool cleanup_ok = true;
	cleanup_ok = impl->device.Close() && cleanup_ok;
	cleanup_ok = impl->ctx.Close() && cleanup_ok;
	impl->shared.reset();
	FinalizeSurfaceSession(*impl, cleanup_ok);
	impl->report.clean_shutdown = cleanup_ok && impl->report.cleanup_state_cleared;
	impl->report.instance_cleanup_ok = impl->shared == nullptr;
	impl->report.surface_cleanup_ok = impl->ctx.surface == VK_NULL_HANDLE;
	impl->report.dispatch_cleanup_ok = true;
	impl->report.native_window = GpuNativeWindowDesc();
	impl->open = false;
	impl->ready = false;
}

VulkanSurfaceReport VulkanSurfaceProbe::Run(bool request_validation, const GpuNativeWindowDesc& native_window)
{
	return Run(request_validation, native_window, nullptr);
}

VulkanSurfaceReport VulkanSurfaceProbe::Run(bool request_validation, const GpuNativeWindowDesc& native_window, VulkanProcResolver resolver)
{
	VulkanSurfaceSession session;
	VulkanSurfaceReport report;
	if(session.Open(request_validation, native_window, resolver))
		session.Close();
	report = session.GetReport();
	return pick(report);
}

String VulkanSurfaceProbe::Dump(const VulkanSurfaceReport& report) const
{
	String out;
	AppendPreflightDump(out, report.preflight);
	out << "Surface requested: " << BoolText(report.surface_requested) << '\n';
	out << "Native window: " << DumpGpuNativeWindowDesc(report.native_window) << '\n';
	out << "Surface created: " << BoolText(report.surface_created) << '\n';
	out << "Selected device: " << report.selected_device.name << '\n';
	out << "Selected device type: " << report.selected_device.type << '\n';
	out << "Graphics queue family: " << AsString(report.graphics_queue_family_index) << '\n';
	out << "Present queue family: " << AsString(report.present_queue_family_index) << '\n';
	out << "Same queue family: " << BoolText(report.same_queue_family) << '\n';
	out << "Graphics queue count: " << AsString(report.graphics_queue_count) << '\n';
	out << "Present queue count: " << AsString(report.present_queue_count) << '\n';
	out << "Graphics queue flags: " << QueueFlagsText(report.graphics_queue_flags) << '\n';
	out << "Present queue flags: " << QueueFlagsText(report.present_queue_flags) << '\n';
	out << "VK_KHR_swapchain enabled: " << BoolText(report.swapchain_enabled) << '\n';
	out << "Min image count: " << AsString(report.min_image_count) << '\n';
	out << "Max image count: " << AsString(report.max_image_count) << '\n';
	out << "Current extent: " << report.current_extent.cx << "x" << report.current_extent.cy << '\n';
	out << "Min extent: " << report.min_extent.cx << "x" << report.min_extent.cy << '\n';
	out << "Max extent: " << report.max_extent.cx << "x" << report.max_extent.cy << '\n';
	out << "Supported transforms: " << AsString((int64)report.supported_transforms) << '\n';
	out << "Current transform: " << AsString((int64)report.current_transform) << '\n';
	out << "Supported composite alpha: " << AsString((int64)report.supported_composite_alpha) << '\n';
	out << "Supported image usage: " << AsString((int64)report.supported_image_usage) << '\n';
	out << "Preferred BGRA8: " << BoolText(report.preferred_bgra8) << '\n';
	out << "Preferred RGBA8: " << BoolText(report.preferred_rgba8) << '\n';
	out << "Preferred SRGB: " << BoolText(report.preferred_srgb) << '\n';
	out << "Preferred Mailbox: " << BoolText(report.preferred_mailbox) << '\n';
	out << "Preferred FIFO: " << BoolText(report.preferred_fifo) << '\n';
	out << "Surface cleanup ok: " << BoolText(report.surface_cleanup_ok) << '\n';
	out << "Device cleanup ok: " << BoolText(report.device_cleanup_ok) << '\n';
	out << "Instance cleanup ok: " << BoolText(report.instance_cleanup_ok) << '\n';
	out << "Dispatch cleanup ok: " << BoolText(report.dispatch_cleanup_ok) << '\n';
	out << "Clean shutdown: " << BoolText(report.clean_shutdown) << '\n';
	out << "Cleanup state cleared: " << BoolText(report.cleanup_state_cleared) << '\n';
	out << "Validation warnings: " << AsString(report.validation_warning_count) << '\n';
	out << "Validation errors: " << AsString(report.validation_error_count) << '\n';
	for(const String& msg : report.validation_messages)
		out << "Validation: " << msg << '\n';
	for(const String& f : report.surface_formats)
		out << "Surface format: " << f << '\n';
	for(const String& mode : report.present_modes)
		out << "Present mode: " << mode << '\n';
	out << "VulkanSurfaceProbe " << (report.status == VulkanProbeStatus::Ok ? "passed" : "failed") << '\n';
	if(!report.runtime_error.IsEmpty()) out << "Runtime error: " << report.runtime_error << '\n';
	if(!report.loader_error.IsEmpty()) out << "Loader error: " << report.loader_error << '\n';
	if(!report.validation_error.IsEmpty()) out << "Validation error: " << report.validation_error << '\n';
	if(!report.instance_error.IsEmpty()) out << "Instance error: " << report.instance_error << '\n';
	if(!report.surface_error.IsEmpty()) out << "Surface error: " << report.surface_error << '\n';
	if(!report.physical_device_error.IsEmpty()) out << "Physical device error: " << report.physical_device_error << '\n';
	if(!report.device_error.IsEmpty()) out << "Device error: " << report.device_error << '\n';
	out << "Status: " << report.status_text << '\n';
	return out;
}

}
