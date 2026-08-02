#include "RenderVulkan.h"
#include "RenderVulkanSurfaceSession.h"
#include "RenderVulkanTestHooks.h"

#include <atomic>
#include <type_traits>

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
static const char *g_registry_test_missing_proc = nullptr;

struct VulkanInstanceOptions {
	bool validation = false;
	bool win32_surface = false;
	const char *application_name = nullptr;
};

struct VulkanInstanceCompatibility {
	bool validation = false;
	bool win32_surface = false;
};

static VulkanInstanceCompatibility GetVulkanInstanceCompatibility(const VulkanInstanceOptions& options)
{
	VulkanInstanceCompatibility key;
	key.validation = options.validation;
	key.win32_surface = options.win32_surface;
	return key;
}

static bool IsVulkanInstanceCompatible(const VulkanInstanceCompatibility& a, const VulkanInstanceCompatibility& b)
{
	return a.validation == b.validation && a.win32_surface == b.win32_surface;
}

static bool ResolveVulkanInstanceOptions(const VulkanInstanceOptions& options,
	const Vector<VulkanExtensionInfo>& available_extensions,
	const Vector<VulkanLayerInfo>& available_layers,
	Vector<String>& enabled_layers,
	Vector<String>& enabled_extensions,
	String& error)
{
	error.Clear();
	auto has_extension = [&](const char *name) {
		for(const auto& ext : available_extensions)
			if(ext.name == name)
				return true;
		return false;
	};
	auto has_layer = [&](const char *name) {
		for(const auto& layer : available_layers)
			if(layer.name == name)
				return true;
		return false;
	};

	Vector<String> tmp_layers;
	Vector<String> tmp_extensions;

	if(options.validation) {
		if(!has_layer("VK_LAYER_KHRONOS_validation")) {
			error = "VK_LAYER_KHRONOS_validation not present";
			return false;
		}
		if(!has_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
			error = "VK_EXT_debug_utils not present";
			return false;
		}
		tmp_layers.Add("VK_LAYER_KHRONOS_validation");
		tmp_extensions.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	if(options.win32_surface) {
		if(!has_extension(VK_KHR_SURFACE_EXTENSION_NAME)) {
			error = "VK_KHR_surface not present";
			return false;
		}
		if(!has_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
			error = "VK_KHR_win32_surface not present";
			return false;
		}
		tmp_extensions.Add(VK_KHR_SURFACE_EXTENSION_NAME);
		tmp_extensions.Add(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	}

	enabled_layers = pick(tmp_layers);
	enabled_extensions = pick(tmp_extensions);
	return true;
}

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
	if(g_registry_test_missing_proc && String(name) == g_registry_test_missing_proc) {
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
	if(g_registry_test_missing_proc && String(name) == g_registry_test_missing_proc) {
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
	if(g_registry_test_missing_proc && String(name) == g_registry_test_missing_proc) {
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

	bool Open(const VulkanDispatch& d, const VulkanInstanceOptions& options, VulkanPreflightReport& preflight, bool& debug_messenger_created, String& error)
	{
		debug_messenger_created = false;
		Close();
		cleanup_ok = true;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.instance_next_id);
		g_runtime_device_stats.instance_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.instance_last_id.store(diagnostic_id, std::memory_order_relaxed);
		dispatch = &d;
		validation_requested = options.validation;
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

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = options.application_name ? options.application_name : "VulkanBootstrap";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "upp_render";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_3;

		Vector<String> enabled_layer_names;
		Vector<String> enabled_extension_names;
		if(!ResolveVulkanInstanceOptions(options, preflight.instance_extensions, preflight.instance_layers, enabled_layer_names, enabled_extension_names, error))
			return fail(error);

		Vector<const char*> enabled_layers;
		Vector<const char*> enabled_exts;
		for(const String& name : enabled_layer_names)
			enabled_layers.Add(name);
		for(const String& name : enabled_extension_names)
			enabled_exts.Add(name);

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

		if(options.validation) {
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
			debug_messenger_created = true;
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

enum class VulkanInstanceOwnerOpenFailure {
	None,
	Dispatch,
	Instance,
};

struct VulkanInstanceOwner {
	VulkanDispatch dispatch;
	VulkanInstanceContext instance;
	VulkanInstanceCompatibility compatibility;
	bool has_compatibility = false;
	bool cleanup_ok = true;

	~VulkanInstanceOwner() { Close(); }

	bool IsCleared() const
	{
		return dispatch.IsCleared() && instance.IsCleared() && !has_compatibility;
	}

	bool Open(const VulkanInstanceOptions& options, VulkanPreflightReport& preflight,
		bool& debug_messenger_created, String& error, VulkanInstanceOwnerOpenFailure& failure_stage,
		VulkanProcResolver resolver = nullptr)
	{
		debug_messenger_created = false;
		failure_stage = VulkanInstanceOwnerOpenFailure::None;
		error.Clear();
		Close();
		cleanup_ok = true;
		if(!dispatch.Open(error, resolver)) {
			failure_stage = VulkanInstanceOwnerOpenFailure::Dispatch;
			Close();
			return false;
		}
		compatibility = GetVulkanInstanceCompatibility(options);
		has_compatibility = true;
		if(!instance.Open(dispatch, options, preflight, debug_messenger_created, error)) {
			failure_stage = VulkanInstanceOwnerOpenFailure::Instance;
			Close();
			return false;
		}
		return true;
	}

	bool Close()
	{
		bool ok = true;
		ok = instance.Close() && ok;
		ok = dispatch.Close() && ok;
		cleanup_ok = cleanup_ok && ok;
		has_compatibility = false;
		return cleanup_ok;
	}

	const VulkanInstanceCompatibility& GetCompatibility() const { return compatibility; }
};

struct VulkanSharedInstanceEntry {
	VulkanInstanceOwner owner;
	VulkanInstanceCompatibility compatibility;
	VulkanPreflightReport reusable_preflight;
	bool reusable_debug_messenger_created = false;
	int acquire_count = 0;
	bool opened = false;
	bool cleanup_ok = true;

	~VulkanSharedInstanceEntry() { ASSERT(acquire_count == 0); Close(); }

	bool IsCleared() const
	{
		return !opened && acquire_count == 0 && owner.IsCleared();
	}

	bool IsCompatible(const VulkanInstanceOptions& options) const
	{
		return opened && IsVulkanInstanceCompatible(compatibility, GetVulkanInstanceCompatibility(options));
	}

	bool Open(const VulkanInstanceOptions& options, VulkanPreflightReport& preflight,
		bool& debug_messenger_created, String& error, VulkanInstanceOwnerOpenFailure& failure_stage,
		VulkanProcResolver resolver = nullptr)
	{
		debug_messenger_created = false;
		failure_stage = VulkanInstanceOwnerOpenFailure::None;
		error.Clear();
		if(acquire_count > 0) {
			error = "shared instance entry is still acquired";
			return false;
		}
		if(!Close()) {
			error = "shared instance entry cleanup failed";
			return false;
		}
		cleanup_ok = true;
		if(!owner.Open(options, preflight, debug_messenger_created, error, failure_stage, resolver))
			return false;
		compatibility = owner.GetCompatibility();
		reusable_preflight = preflight;
		reusable_debug_messenger_created = debug_messenger_created;
		acquire_count = 1;
		opened = true;
		return true;
	}

	bool Acquire(const VulkanInstanceOptions& options)
	{
		if(!IsCompatible(options))
			return false;
		acquire_count++;
		return true;
	}

	void PublishReusableOutputs(VulkanPreflightReport& preflight, bool& debug_messenger_created) const
	{
		preflight = reusable_preflight;
		debug_messenger_created = reusable_debug_messenger_created;
	}

	bool Release()
	{
		if(!opened || acquire_count <= 0)
			return false;
		acquire_count--;
		return true;
	}

	bool IsUnused() const
	{
		return acquire_count == 0;
	}

	bool Close()
	{
		if(!opened) {
			cleanup_ok = cleanup_ok && IsCleared();
			return cleanup_ok;
		}
		if(acquire_count > 0)
			return false;
		bool ok = owner.Close();
		cleanup_ok = cleanup_ok && ok;
		acquire_count = 0;
		opened = false;
		return cleanup_ok;
	}
};

struct VulkanSharedInstanceRegistry {
	Vector<One<VulkanSharedInstanceEntry>> entries;
	struct ReleaseOutcome {
		bool acquisition_released = false;
		bool entry_retained = false;
		bool entry_removed = false;
		bool cleanup_ok = false;
	};

	~VulkanSharedInstanceRegistry()
	{
		for(auto& slot : entries)
			ASSERT(!slot || slot->acquire_count == 0);
	}

	VulkanSharedInstanceEntry *FindCompatible(const VulkanInstanceOptions& options)
	{
		for(auto& slot : entries)
			if(slot && slot->opened && slot->IsCompatible(options))
				return &*slot;
		return nullptr;
	}

	VulkanSharedInstanceEntry *FindRetainedFailure(const VulkanInstanceOptions& options)
	{
		VulkanInstanceCompatibility key = GetVulkanInstanceCompatibility(options);
		for(auto& slot : entries)
			if(slot && !slot->opened && slot->acquire_count == 0 && !slot->cleanup_ok && IsVulkanInstanceCompatible(slot->compatibility, key))
				return &*slot;
		return nullptr;
	}

	bool Acquire(const VulkanInstanceOptions& options, VulkanPreflightReport& preflight, bool& debug_messenger_created,
		String& error, VulkanInstanceOwnerOpenFailure& failure_stage, VulkanProcResolver resolver,
		VulkanSharedInstanceEntry*& out_entry, bool& newly_created)
	{
		out_entry = nullptr;
		newly_created = false;
		debug_messenger_created = false;
		failure_stage = VulkanInstanceOwnerOpenFailure::None;
		error.Clear();
		preflight = VulkanPreflightReport();

		if(VulkanSharedInstanceEntry *retained = FindRetainedFailure(options)) {
			error = "shared instance entry cleanup failed";
			return false;
		}
		if(VulkanSharedInstanceEntry *existing = FindCompatible(options)) {
			if(!existing->Acquire(options)) {
				error = "shared instance entry acquire failed";
				return false;
			}
			existing->PublishReusableOutputs(preflight, debug_messenger_created);
			out_entry = existing;
			return true;
		}
		entries.Add().Create();
		VulkanSharedInstanceEntry& entry = *entries.Top();
		if(!entry.Open(options, preflight, debug_messenger_created, error, failure_stage, resolver)) {
			entries.Drop();
			return false;
		}
		out_entry = &entry;
		newly_created = true;
		return true;
	}

	ReleaseOutcome ReleaseDetailed(VulkanSharedInstanceEntry *entry)
	{
		ReleaseOutcome outcome;
		if(!entry)
			return outcome;
		for(int i = 0; i < entries.GetCount(); ++i) {
			if(&*entries[i] != entry)
				continue;
			if(!entry->Release())
				return outcome;
			outcome.acquisition_released = true;
			if(entry->IsUnused()) {
				outcome.cleanup_ok = entry->Close();
				if(!outcome.cleanup_ok) {
					outcome.entry_retained = true;
					return outcome;
				}
				entries.Remove(i);
				outcome.entry_removed = true;
				return outcome;
			}
			outcome.entry_retained = true;
			outcome.cleanup_ok = true;
			return outcome;
		}
		return outcome;
	}

	bool Release(VulkanSharedInstanceEntry *entry)
	{
		ReleaseOutcome outcome = ReleaseDetailed(entry);
		return outcome.acquisition_released && outcome.cleanup_ok;
	}

	int GetEntryCount() const { return entries.GetCount(); }
};

struct VulkanSharedInstanceLease {
	VulkanSharedInstanceRegistry *registry = nullptr;
	VulkanSharedInstanceEntry *entry = nullptr;

	VulkanSharedInstanceLease() = default;
	VulkanSharedInstanceLease(const VulkanSharedInstanceLease&) = delete;
	VulkanSharedInstanceLease& operator=(const VulkanSharedInstanceLease&) = delete;
	VulkanSharedInstanceLease& operator=(VulkanSharedInstanceLease&&) = delete;
	VulkanSharedInstanceLease(VulkanSharedInstanceLease&& other) noexcept
		: registry(other.registry), entry(other.entry)
	{
		other.registry = nullptr;
		other.entry = nullptr;
	}
	~VulkanSharedInstanceLease()
	{
		if(!IsAcquired())
			return;
		bool cleanup_ok = Reset();
		ASSERT(cleanup_ok || IsEmpty());
	}

	bool IsAcquired() const { return registry != nullptr && entry != nullptr; }
	bool IsEmpty() const { return registry == nullptr && entry == nullptr; }
	VulkanInstanceOwner *GetOwner() const { return IsAcquired() ? &entry->owner : nullptr; }

	bool Acquire(VulkanSharedInstanceRegistry& target, const VulkanInstanceOptions& options,
		VulkanPreflightReport& preflight, bool& debug_messenger_created, String& error,
		VulkanInstanceOwnerOpenFailure& failure_stage, VulkanProcResolver resolver = nullptr,
		bool *newly_created = nullptr)
	{
		if(IsAcquired()) {
			preflight = VulkanPreflightReport();
			debug_messenger_created = false;
			failure_stage = VulkanInstanceOwnerOpenFailure::None;
			if(newly_created)
				*newly_created = false;
			error = "shared instance lease is already acquired";
			return false;
		}
		VulkanSharedInstanceEntry *acquired = nullptr;
		bool created = false;
		if(!target.Acquire(options, preflight, debug_messenger_created, error, failure_stage, resolver, acquired, created)) {
			registry = nullptr;
			entry = nullptr;
			if(newly_created)
				*newly_created = false;
			return false;
		}
		registry = &target;
		entry = acquired;
		if(newly_created)
			*newly_created = created;
		return true;
	}

	bool Reset()
	{
		if(IsEmpty())
			return true;
		ASSERT(IsAcquired());
		VulkanSharedInstanceRegistry *old_registry = registry;
		VulkanSharedInstanceEntry *old_entry = entry;
		VulkanSharedInstanceRegistry::ReleaseOutcome outcome = old_registry->ReleaseDetailed(old_entry);
		if(outcome.acquisition_released) {
			registry = nullptr;
			entry = nullptr;
		}
		else {
			ASSERT(false);
		}
		return outcome.acquisition_released && outcome.cleanup_ok;
	}
};

static_assert(!std::is_copy_constructible<VulkanSharedInstanceLease>::value, "shared instance lease must not be copyable");
static_assert(!std::is_copy_assignable<VulkanSharedInstanceLease>::value, "shared instance lease must not be copy assignable");
static_assert(std::is_move_constructible<VulkanSharedInstanceLease>::value, "shared instance lease must be movable");
static_assert(!std::is_move_assignable<VulkanSharedInstanceLease>::value, "shared instance lease move assignment must remain disabled");

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

	void RegisterDiagnostics()
	{
		if(diagnostic_id)
			return;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.device_next_id);
		g_runtime_device_stats.device_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.device_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.device_last_id.store(diagnostic_id, std::memory_order_relaxed);
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

	bool Open(const VulkanInstanceContext& instance, VkPhysicalDevice physical_device, VulkanDeviceInfo& device_info, VulkanBootstrapReport& report, String& error)
	{
		this->physical_device = physical_device;
		cleanup_ok = true;
		cleanup_result = 0;
		cleanup_error.Clear();
		RegisterDiagnostics();
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
		if(!ResolveInstanceProc(create_device, instance.dispatch->proc_filter, instance.dispatch->get_instance_proc_addr, instance.instance, "vkCreateDevice", error))
			return fail(error);
		VkResult vr = create_device(physical_device, &dci, nullptr, &device);
		if(vr != VK_SUCCESS) {
			error = String("vkCreateDevice failed: ") + AsString((int)vr);
			return fail(error);
		}

		if(!ResolveDeviceProc(destroy_device, instance.dispatch->proc_filter, instance.get_device_proc_addr, device, "vkDestroyDevice", error)) return fail(error);
		if(!ResolveDeviceProc(get_device_queue, instance.dispatch->proc_filter, instance.get_device_proc_addr, device, "vkGetDeviceQueue", error)) return fail(error);
		if(!ResolveDeviceProc(device_wait_idle, instance.dispatch->proc_filter, instance.get_device_proc_addr, device, "vkDeviceWaitIdle", error)) return fail(error);

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
	VulkanInstanceOwner *instance_owner = nullptr;
	PFN_vkCreateWin32SurfaceKHR create_win32_surface = nullptr;
	PFN_vkDestroySurfaceKHR destroy_surface = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR get_surface_support = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_surface_capabilities = nullptr;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR get_surface_formats = nullptr;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR get_surface_present_modes = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool surface_requested = false;
	bool surface_available = false;
	bool cleanup_ok = true;
	uint64_t diagnostic_id = 0;

	~VulkanSurfaceContext() { Close(); }

	bool IsCleared() const
	{
		return instance_owner == nullptr && create_win32_surface == nullptr && destroy_surface == nullptr && get_surface_support == nullptr && get_surface_capabilities == nullptr && get_surface_formats == nullptr && get_surface_present_modes == nullptr && surface == VK_NULL_HANDLE;
	}

	bool Close()
	{
		bool ok = true;
		if(surface && destroy_surface) {
			destroy_surface(instance_owner->instance.instance, surface, nullptr);
			surface = VK_NULL_HANDLE;
		}
		else if(surface) {
			ok = false;
			surface = VK_NULL_HANDLE;
		}
		create_win32_surface = nullptr;
		destroy_surface = nullptr;
		get_surface_support = nullptr;
		get_surface_capabilities = nullptr;
		get_surface_formats = nullptr;
		get_surface_present_modes = nullptr;
		surface_requested = false;
		surface_available = false;
		instance_owner = nullptr;
		cleanup_ok = cleanup_ok && ok && IsCleared();
		if(diagnostic_id) {
			g_runtime_device_stats.surface_live_count.fetch_sub(1, std::memory_order_relaxed);
			diagnostic_id = 0;
		}
		return cleanup_ok;
	}

	bool Open(VulkanInstanceOwner& owner, bool request_validation, const GpuNativeWindowDesc& native_window, VulkanSurfaceReport& report,
		String& error, VulkanInstanceOwnerOpenFailure& failure_stage, VulkanProcResolver resolver = nullptr)
	{
		Close();
		instance_owner = &owner;
		cleanup_ok = true;
		diagnostic_id = NextDiagnosticId(g_runtime_device_stats.surface_next_id);
		g_runtime_device_stats.surface_create_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.surface_live_count.fetch_add(1, std::memory_order_relaxed);
		g_runtime_device_stats.surface_last_id.store(diagnostic_id, std::memory_order_relaxed);
		surface_requested = true;
		auto fail = [&](const String& message) {
			error = message;
			Close();
			return false;
		};

		if(native_window.kind != GpuNativeWindowKind::Win32) {
			error = "surface requires Win32 native window";
			failure_stage = VulkanInstanceOwnerOpenFailure::None;
			return fail(error);
		}
		if(native_window.handle == 0) {
			error = "invalid native handle";
			failure_stage = VulkanInstanceOwnerOpenFailure::None;
			return fail(error);
		}

		report.validation_available = report.preflight.validation_available;
		report.debug_utils_available = report.preflight.debug_utils_available;
		surface_available = HasExtension(report.preflight.instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME) && HasExtension(report.preflight.instance_extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

		if(!ResolveInstanceProc(create_win32_surface, owner.dispatch.proc_filter, owner.dispatch.get_instance_proc_addr, owner.instance.instance, "vkCreateWin32SurfaceKHR", error)) return fail(error);
		if(!ResolveInstanceProc(destroy_surface, owner.dispatch.proc_filter, owner.dispatch.get_instance_proc_addr, owner.instance.instance, "vkDestroySurfaceKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_support, owner.dispatch.proc_filter, owner.dispatch.get_instance_proc_addr, owner.instance.instance, "vkGetPhysicalDeviceSurfaceSupportKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_capabilities, owner.dispatch.proc_filter, owner.dispatch.get_instance_proc_addr, owner.instance.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_formats, owner.dispatch.proc_filter, owner.dispatch.get_instance_proc_addr, owner.instance.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR", error)) return fail(error);
		if(!ResolveInstanceProc(get_surface_present_modes, owner.dispatch.proc_filter, owner.dispatch.get_instance_proc_addr, owner.instance.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR", error)) return fail(error);

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
		VkResult vr = create_win32_surface(owner.instance.instance, &surface_info, nullptr, &surface);
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
		if(!instance_owner || !instance_owner->instance.EnumeratePhysicalDevices(out, error))
			return false;
		if(out.IsEmpty())
			return true;

		for(auto& device : out) {
			VulkanDeviceInfo& info = device.info;
			for(int i = 0; i < info.queue_families.GetCount(); ++i) {
				VulkanQueueFamilyInfo& qinfo = info.queue_families[i];
				VkBool32 present = VK_FALSE;
				VkResult support_result = get_surface_support(device.handle, (uint32_t)i, surface, &present);
				if(support_result != VK_SUCCESS) {
					error = String("vkGetPhysicalDeviceSurfaceSupportKHR failed: ") + AsString((int)support_result);
					return false;
				}
				qinfo.present = present == VK_TRUE;
			}
			if(!HasExtension(info.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
				AppendMissing(info, "VK_KHR_swapchain");
			info.suitable = info.suitable && HasExtension(info.device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}
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

static void FinalizeBootstrapCleanup(VulkanBootstrapReport& report, const VulkanInstanceOwner& owner, const VulkanDeviceContext& device, bool cleanup_ok)
{
	report.cleanup_state_cleared = owner.IsCleared() && device.IsCleared();
	report.clean_shutdown = cleanup_ok && report.cleanup_state_cleared;
}

static bool CloseBootstrapContexts(VulkanInstanceOwner& owner, VulkanDeviceContext& device)
{
	bool ok = device.Close();
	ok = owner.Close() && ok;
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

struct VulkanSurfaceSessionGroup::Impl {
	VulkanSharedInstanceRegistry registry;
};

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

VulkanInstanceOptionsTestResult RunVulkanInstanceOptionsTest(bool validation, bool win32_surface,
	bool has_surface_extension, bool has_win32_surface_extension,
	bool has_validation_layer, bool has_debug_utils_extension)
{
	Vector<VulkanExtensionInfo> extensions;
	Vector<VulkanLayerInfo> layers;
	if(has_debug_utils_extension)
		extensions.Add().name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	if(has_surface_extension)
		extensions.Add().name = VK_KHR_SURFACE_EXTENSION_NAME;
	if(has_win32_surface_extension)
		extensions.Add().name = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	if(has_validation_layer)
		layers.Add().name = "VK_LAYER_KHRONOS_validation";

	VulkanInstanceOptions options;
	options.validation = validation;
	options.win32_surface = win32_surface;
	options.application_name = "VulkanInstanceOptionsTest";
	Vector<String> enabled_layers;
	Vector<String> enabled_extensions;
	String error;
	bool opened = ResolveVulkanInstanceOptions(options, extensions, layers, enabled_layers, enabled_extensions, error);

	VulkanInstanceOptionsTestResult result;
	result.opened = opened;
	result.error = error;
	for(const String& s : enabled_layers)
		result.enabled_layers.Add(s);
	for(const String& s : enabled_extensions)
		result.enabled_extensions.Add(s);
	return result;
}

bool TestVulkanInstanceCompatibility(bool validation_a, bool surface_a, bool validation_b, bool surface_b,
	const char *application_name_a, const char *application_name_b)
{
	VulkanInstanceOptions opts_a;
	opts_a.validation = validation_a;
	opts_a.win32_surface = surface_a;
	opts_a.application_name = application_name_a;
	VulkanInstanceOptions opts_b;
	opts_b.validation = validation_b;
	opts_b.win32_surface = surface_b;
	opts_b.application_name = application_name_b;
	return IsVulkanInstanceCompatible(GetVulkanInstanceCompatibility(opts_a), GetVulkanInstanceCompatibility(opts_b));
}

bool TestVulkanInstanceOwner(bool validation, VulkanProcResolver resolver, int& out_failure_stage,
	bool& out_debug_messenger_created, VulkanRuntimeDeviceDiagnostics& out_diag)
{
	out_diag = VulkanRuntimeDeviceDiagnostics();
	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanInstanceOwner owner;
	VulkanInstanceOptions options;
	options.validation = validation;
	options.application_name = "RenderVulkanTest";
	VulkanPreflightReport preflight;
	bool debug_messenger_created = true;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage = VulkanInstanceOwnerOpenFailure::None;
	bool ok = owner.Open(options, preflight, debug_messenger_created, error, failure_stage, resolver);
	out_failure_stage = (int)failure_stage;
	out_debug_messenger_created = debug_messenger_created;
	if(!ok) {
		owner.Close();
		out_diag = GetVulkanRuntimeDeviceDiagnostics();
		return false;
	}
	ok = owner.Close() && ok;
	ok = owner.Close() && ok;
	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	return ok;
}

bool TestVulkanInstanceOwnerCompatibility(bool validation, bool win32_surface)
{
	VulkanInstanceOwner owner;
	VulkanInstanceOptions options;
	options.validation = validation;
	options.win32_surface = win32_surface;
	options.application_name = "RenderVulkanTest";
	VulkanPreflightReport preflight;
	bool debug_messenger_created = false;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage = VulkanInstanceOwnerOpenFailure::None;
	if(!owner.Open(options, preflight, debug_messenger_created, error, failure_stage))
		return false;
	bool match = owner.GetCompatibility().validation == validation && owner.GetCompatibility().win32_surface == win32_surface;
	owner.Close();
	return match;
}

bool TestVulkanSurfaceOwner(bool validation, VulkanProcResolver resolver, int& out_failure_stage,
	VulkanRuntimeDeviceDiagnostics& out_diag)
{
	out_diag = VulkanRuntimeDeviceDiagnostics();
	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanSurfaceContext ctx;
	VulkanInstanceOwner owner;
	VulkanInstanceOptions instance_options;
	instance_options.validation = validation;
	instance_options.win32_surface = true;
	instance_options.application_name = "VulkanSurfaceProbe";
	GpuNativeWindowDesc window;
	window.kind = GpuNativeWindowKind::Win32;
	window.handle = (uint64_t)(uintptr_t)1;
	VulkanSurfaceReport report;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage = VulkanInstanceOwnerOpenFailure::None;
	bool debug = false;
	if(!owner.Open(instance_options, report.preflight, debug, error, failure_stage, resolver)) {
		out_failure_stage = (int)failure_stage;
		out_diag = GetVulkanRuntimeDeviceDiagnostics();
		return false;
	}
	bool ok = ctx.Open(owner, validation, window, report, error, failure_stage, resolver);
	out_failure_stage = (int)failure_stage;
	if(!ok) {
		ctx.Close();
		owner.Close();
		out_diag = GetVulkanRuntimeDeviceDiagnostics();
		return false;
	}
	ok = ctx.Close() && ok;
	ok = ctx.Close() && ok;
	ok = owner.Close() && ok;
	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	return ok;
}

bool TestVulkanSurfaceOwnerCompatibility(bool validation)
{
	HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	if(!hwnd)
		return false;

	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanSurfaceContext ctx;
	VulkanInstanceOwner owner;
	VulkanInstanceOptions instance_options;
	instance_options.validation = validation;
	instance_options.win32_surface = true;
	instance_options.application_name = "VulkanSurfaceProbe";
	GpuNativeWindowDesc window;
	window.kind = GpuNativeWindowKind::Win32;
	window.handle = (uint64_t)(uintptr_t)hwnd;
	VulkanSurfaceReport report;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage = VulkanInstanceOwnerOpenFailure::None;
	bool debug = false;
	bool ok = owner.Open(instance_options, report.preflight, debug, error, failure_stage) && ctx.Open(owner, validation, window, report, error, failure_stage);
	bool match = ok && owner.GetCompatibility().validation == validation && owner.GetCompatibility().win32_surface == true;
	ctx.Close();
	owner.Close();
	DestroyWindow(hwnd);
	return match;
}

static HWND CreateHiddenSurfaceTestWindow()
{
	return CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

bool TestVulkanSurfaceSessionLifecycle(bool validation, VulkanProcResolver resolver, VulkanSurfaceSessionAccountingResult& out_result)
{
	out_result = VulkanSurfaceSessionAccountingResult();
	ClearVulkanRuntimeDeviceDiagnostics();

	HWND hwnd = CreateHiddenSurfaceTestWindow();
	if(!hwnd)
		return false;

	VulkanSurfaceSession session;
	GpuNativeWindowDesc window;
	window.kind = GpuNativeWindowKind::Win32;
	window.handle = (uint64_t)(uintptr_t)hwnd;
	bool ok = session.Open(validation, window, resolver);
	out_result.open_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(!ok) {
		session.Close();
		DestroyWindow(hwnd);
		return false;
	}
	if(out_result.open_diag.device_create_count != 1 || out_result.open_diag.device_live_count != 1 || out_result.open_diag.device_id == 0) {
		session.Close();
		DestroyWindow(hwnd);
		return false;
	}

	session.Close();
	out_result.report = session.GetReport();
	out_result.close_diag = GetVulkanRuntimeDeviceDiagnostics();
	session.Close();
	out_result.repeat_report = session.GetReport();
	out_result.repeat_close_diag = GetVulkanRuntimeDeviceDiagnostics();
	DestroyWindow(hwnd);
	return true;
}

bool TestVulkanSurfaceSessionPostCreateFailure(bool validation, VulkanProcResolver resolver, VulkanSurfaceSessionAccountingResult& out_result)
{
	out_result = VulkanSurfaceSessionAccountingResult();
	ClearVulkanRuntimeDeviceDiagnostics();

	HWND hwnd = CreateHiddenSurfaceTestWindow();
	if(!hwnd)
		return false;

	VulkanSurfaceSession session;
	GpuNativeWindowDesc window;
	window.kind = GpuNativeWindowKind::Win32;
	window.handle = (uint64_t)(uintptr_t)hwnd;
	if(session.Open(validation, window, resolver)) {
		session.Close();
		DestroyWindow(hwnd);
		return false;
	}
	out_result.report = session.GetReport();
	out_result.error = session.GetError();
	out_result.close_diag = GetVulkanRuntimeDeviceDiagnostics();
	session.Close();
	out_result.repeat_report = session.GetReport();
	out_result.repeat_close_diag = GetVulkanRuntimeDeviceDiagnostics();
	DestroyWindow(hwnd);
	return true;
}

bool TestVulkanSurfaceSessionCleanupFailure(bool validation, VulkanProcResolver resolver, VulkanSurfaceSessionAccountingResult& out_result)
{
	out_result = VulkanSurfaceSessionAccountingResult();
	ClearVulkanRuntimeDeviceDiagnostics();

	HWND hwnd = CreateHiddenSurfaceTestWindow();
	if(!hwnd)
		return false;

	VulkanValidationTestInjection injection;
	injection.enabled = true;
	injection.point = VulkanValidationTestPoint::DuringDeviceCleanup;
	injection.force_device_cleanup_failure = true;
	injection.device_cleanup_result = VK_ERROR_DEVICE_LOST;
	SetVulkanValidationTestInjection(injection);

	VulkanSurfaceSession session;
	GpuNativeWindowDesc window;
	window.kind = GpuNativeWindowKind::Win32;
	window.handle = (uint64_t)(uintptr_t)hwnd;
	bool ok = session.Open(validation, window, resolver);
	out_result.open_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(!ok) {
		ClearVulkanValidationTestInjection();
		session.Close();
		DestroyWindow(hwnd);
		return false;
	}

	session.Close();
	out_result.report = session.GetReport();
	out_result.close_diag = GetVulkanRuntimeDeviceDiagnostics();
	session.Close();
	out_result.repeat_report = session.GetReport();
	out_result.repeat_close_diag = GetVulkanRuntimeDeviceDiagnostics();
	ClearVulkanValidationTestInjection();
	DestroyWindow(hwnd);
	return true;
}

bool TestVulkanSharedInstanceEntryLifecycle(VulkanProcResolver resolver, VulkanRuntimeDeviceDiagnostics& out_diag)
{
	out_diag = VulkanRuntimeDeviceDiagnostics();
	ClearVulkanRuntimeDeviceDiagnostics();

	VulkanSharedInstanceEntry entry;
	VulkanInstanceOptions opts;
	opts.validation = true;
	opts.application_name = "SharedEntryTest";
	VulkanPreflightReport preflight;
	bool debug_messenger_created = false;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage;

	if(!entry.Open(opts, preflight, debug_messenger_created, error, failure_stage, resolver))
		return false;
	if(entry.acquire_count != 1)
		return false;

	VulkanInstanceOptions opts2 = opts;
	opts2.application_name = "SharedEntryTest2";
	if(!entry.Acquire(opts2))
		return false;
	if(entry.acquire_count != 2)
		return false;

	if(!entry.Release())
		return false;
	if(entry.acquire_count != 1)
		return false;

	if(!entry.Release())
		return false;
	if(entry.acquire_count != 0)
		return false;
	if(!entry.IsUnused())
		return false;

	if(!entry.Close())
		return false;
	if(!entry.Close())
		return false;

	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	return true;
}

bool TestVulkanSharedInstanceEntrySafety(VulkanProcResolver resolver, VulkanRuntimeDeviceDiagnostics& out_diag)
{
	out_diag = VulkanRuntimeDeviceDiagnostics();
	ClearVulkanRuntimeDeviceDiagnostics();

	VulkanSharedInstanceEntry entry;
	VulkanInstanceOptions opts;
	opts.validation = true;
	opts.application_name = "SharedEntryTest";
	VulkanPreflightReport preflight;
	bool debug_messenger_created = false;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage;

	if(!entry.Open(opts, preflight, debug_messenger_created, error, failure_stage, resolver))
		return false;

	VulkanInstanceOptions opts2 = opts;
	if(!entry.Acquire(opts2))
		return false;
	if(entry.acquire_count != 2)
		return false;

	VulkanInstanceCompatibility compatibility = entry.compatibility;
	bool reopen_debug_messenger_created = true;
	VulkanInstanceOwnerOpenFailure reopen_failure_stage = VulkanInstanceOwnerOpenFailure::Instance;
	String reopen_error;
	if(entry.Open(opts, preflight, reopen_debug_messenger_created, reopen_error, reopen_failure_stage, resolver))
		return false;
	if(reopen_error != "shared instance entry is still acquired")
		return false;
	if(reopen_failure_stage != VulkanInstanceOwnerOpenFailure::None)
		return false;
	if(reopen_debug_messenger_created)
		return false;
	if(entry.acquire_count != 2)
		return false;
	if(!entry.opened || !entry.cleanup_ok)
		return false;
	if(entry.compatibility.validation != compatibility.validation || entry.compatibility.win32_surface != compatibility.win32_surface)
		return false;
	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(out_diag.runtime_create_count != 1 || out_diag.instance_create_count != 1)
		return false;
	if(out_diag.runtime_live_count != 1 || out_diag.instance_live_count != 1 || out_diag.debug_messenger_live_count != 1)
		return false;

	if(!entry.Release())
		return false;
	if(entry.acquire_count != 1)
		return false;

	reopen_debug_messenger_created = true;
	reopen_failure_stage = VulkanInstanceOwnerOpenFailure::Instance;
	reopen_error.Clear();
	if(entry.Open(opts, preflight, reopen_debug_messenger_created, reopen_error, reopen_failure_stage, resolver))
		return false;
	if(reopen_error != "shared instance entry is still acquired")
		return false;
	if(reopen_failure_stage != VulkanInstanceOwnerOpenFailure::None)
		return false;
	if(reopen_debug_messenger_created)
		return false;
	if(entry.acquire_count != 1)
		return false;
	if(!entry.opened || !entry.cleanup_ok)
		return false;
	if(entry.compatibility.validation != compatibility.validation || entry.compatibility.win32_surface != compatibility.win32_surface)
		return false;
	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(out_diag.runtime_create_count != 1 || out_diag.instance_create_count != 1)
		return false;
	if(out_diag.runtime_live_count != 1 || out_diag.instance_live_count != 1 || out_diag.debug_messenger_live_count != 1)
		return false;

	if(!entry.Release())
		return false;
	if(entry.acquire_count != 0)
		return false;
	if(entry.Release())
		return false;
	if(entry.acquire_count != 0)
		return false;

	if(!entry.Close())
		return false;
	if(!entry.Close())
		return false;

	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(out_diag.runtime_live_count != 0 || out_diag.instance_live_count != 0 || out_diag.debug_messenger_live_count != 0)
		return false;

	if(!entry.Open(opts, preflight, debug_messenger_created, error, failure_stage, resolver))
		return false;
	if(!entry.Release())
		return false;
	if(!entry.Close())
		return false;

	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(out_diag.runtime_live_count != 0 || out_diag.instance_live_count != 0 || out_diag.debug_messenger_live_count != 0)
		return false;

	VulkanSharedInstanceEntry cleanup_failure_entry;
	if(!cleanup_failure_entry.Open(opts, preflight, debug_messenger_created, error, failure_stage, resolver))
		return false;
	if(!cleanup_failure_entry.Release())
		return false;
	if(cleanup_failure_entry.acquire_count != 0)
		return false;
	if(cleanup_failure_entry.owner.instance.destroy_instance == nullptr)
		return false;
	cleanup_failure_entry.owner.cleanup_ok = false;
	VulkanRuntimeDeviceDiagnostics cleanup_counts_before_reopen = GetVulkanRuntimeDeviceDiagnostics();
	if(cleanup_counts_before_reopen.runtime_live_count != 1 || cleanup_counts_before_reopen.instance_live_count != 1 || cleanup_counts_before_reopen.debug_messenger_live_count != 1)
		return false;
	bool cleanup_reopen_debug_messenger_created = true;
	VulkanInstanceOwnerOpenFailure cleanup_reopen_failure_stage = VulkanInstanceOwnerOpenFailure::Instance;
	String cleanup_reopen_error;
	if(cleanup_failure_entry.Open(opts, preflight, cleanup_reopen_debug_messenger_created, cleanup_reopen_error, cleanup_reopen_failure_stage, resolver))
		return false;
	if(cleanup_reopen_error != "shared instance entry cleanup failed")
		return false;
	if(cleanup_reopen_failure_stage != VulkanInstanceOwnerOpenFailure::None)
		return false;
	if(cleanup_reopen_debug_messenger_created)
		return false;
	if(cleanup_failure_entry.cleanup_ok)
		return false;
	if(cleanup_failure_entry.opened)
		return false;
	if(cleanup_failure_entry.acquire_count != 0)
		return false;
	if(!cleanup_failure_entry.owner.IsCleared())
		return false;
	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(out_diag.runtime_create_count != cleanup_counts_before_reopen.runtime_create_count || out_diag.instance_create_count != cleanup_counts_before_reopen.instance_create_count)
		return false;
	if(out_diag.runtime_live_count != 0 || out_diag.instance_live_count != 0 || out_diag.debug_messenger_live_count != 0)
		return false;
	VulkanRuntimeDeviceDiagnostics cleanup_counts_after_failure = out_diag;
	if(cleanup_failure_entry.Open(opts, preflight, cleanup_reopen_debug_messenger_created, cleanup_reopen_error, cleanup_reopen_failure_stage, resolver))
		return false;
	if(cleanup_reopen_error != "shared instance entry cleanup failed")
		return false;
	if(cleanup_reopen_failure_stage != VulkanInstanceOwnerOpenFailure::None)
		return false;
	if(cleanup_reopen_debug_messenger_created)
		return false;
	if(cleanup_failure_entry.cleanup_ok)
		return false;
	if(cleanup_failure_entry.opened)
		return false;
	if(cleanup_failure_entry.acquire_count != 0)
		return false;
	if(!cleanup_failure_entry.owner.IsCleared())
		return false;
	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(out_diag.runtime_create_count != cleanup_counts_after_failure.runtime_create_count || out_diag.instance_create_count != cleanup_counts_after_failure.instance_create_count)
		return false;
	if(out_diag.runtime_live_count != cleanup_counts_after_failure.runtime_live_count || out_diag.instance_live_count != cleanup_counts_after_failure.instance_live_count || out_diag.debug_messenger_live_count != cleanup_counts_after_failure.debug_messenger_live_count)
		return false;

	return true;
}

bool TestVulkanSharedInstanceEntryIncompatible(bool base_validation, bool base_surface,
	bool test_validation, bool test_surface, VulkanRuntimeDeviceDiagnostics& out_diag)
{
	out_diag = VulkanRuntimeDeviceDiagnostics();
	ClearVulkanRuntimeDeviceDiagnostics();

	VulkanSharedInstanceEntry entry;
	VulkanInstanceOptions opts;
	opts.validation = base_validation;
	opts.win32_surface = base_surface;
	opts.application_name = "SharedEntryTest";
	VulkanPreflightReport preflight;
	bool debug_messenger_created = false;
	String error;
	VulkanInstanceOwnerOpenFailure failure_stage;

	if(!entry.Open(opts, preflight, debug_messenger_created, error, failure_stage))
		return false;

	int count_before = entry.acquire_count;

	VulkanInstanceOptions test_opts;
	test_opts.validation = test_validation;
	test_opts.win32_surface = test_surface;
	test_opts.application_name = "DifferentApp";

	if(entry.Acquire(test_opts)) {
		entry.Close();
		return false;
	}
	if(entry.acquire_count != count_before) {
		entry.Close();
		return false;
	}

	entry.Release();
	entry.Close();

	out_diag = GetVulkanRuntimeDeviceDiagnostics();
	return true;
}

static bool SameReusablePreflight(const VulkanPreflightReport& a, const VulkanPreflightReport& b)
{
	if(a.status != b.status || a.status_text != b.status_text || a.loader_available != b.loader_available || a.loader_version != b.loader_version || a.validation_requested != b.validation_requested || a.validation_available != b.validation_available || a.debug_utils_available != b.debug_utils_available || a.instance_created != b.instance_created || a.clean_shutdown != b.clean_shutdown || a.cleanup_state_cleared != b.cleanup_state_cleared || a.validation_warning_count != b.validation_warning_count || a.validation_error_count != b.validation_error_count || a.runtime_error != b.runtime_error || a.loader_error != b.loader_error || a.layer_error != b.layer_error || a.extension_error != b.extension_error || a.instance_error != b.instance_error || a.physical_device_error != b.physical_device_error || a.suitable_device_count != b.suitable_device_count || a.validation_messages != b.validation_messages)
		return false;
	if(a.instance_layers.GetCount() != b.instance_layers.GetCount() || a.instance_extensions.GetCount() != b.instance_extensions.GetCount() || a.devices.GetCount() != b.devices.GetCount())
		return false;
	for(int i = 0; i < a.instance_layers.GetCount(); ++i)
		if(a.instance_layers[i].name != b.instance_layers[i].name || a.instance_layers[i].description != b.instance_layers[i].description || a.instance_layers[i].spec_version != b.instance_layers[i].spec_version)
			return false;
	for(int i = 0; i < a.instance_extensions.GetCount(); ++i)
		if(a.instance_extensions[i].name != b.instance_extensions[i].name || a.instance_extensions[i].spec_version != b.instance_extensions[i].spec_version)
			return false;
	for(int i = 0; i < a.devices.GetCount(); ++i) {
		const VulkanDeviceInfo& x = a.devices[i];
		const VulkanDeviceInfo& y = b.devices[i];
		if(x.name != y.name || x.type != y.type || x.vendor_id != y.vendor_id || x.device_id != y.device_id || x.driver_version != y.driver_version || x.api_version != y.api_version || x.graphics_queue != y.graphics_queue || x.dynamic_rendering != y.dynamic_rendering || x.synchronization2 != y.synchronization2 || x.suitable != y.suitable || x.selection_reason != y.selection_reason || x.selected_queue_family_index != y.selected_queue_family_index || x.selected_queue_count != y.selected_queue_count || x.selected_queue_flags != y.selected_queue_flags || x.selected_queue_compute != y.selected_queue_compute || x.selected_queue_transfer != y.selected_queue_transfer || x.logical_device_created != y.logical_device_created || x.graphics_queue_acquired != y.graphics_queue_acquired || x.missing_requirements != y.missing_requirements || x.queue_families.GetCount() != y.queue_families.GetCount() || x.device_extensions.GetCount() != y.device_extensions.GetCount())
			return false;
		for(int j = 0; j < x.queue_families.GetCount(); ++j) {
			const VulkanQueueFamilyInfo& q = x.queue_families[j];
			const VulkanQueueFamilyInfo& r = y.queue_families[j];
			if(q.index != r.index || q.flags != r.flags || q.count != r.count || q.graphics != r.graphics || q.present != r.present || q.compute != r.compute || q.transfer != r.transfer || q.sparse_binding != r.sparse_binding)
				return false;
		}
		for(int j = 0; j < x.device_extensions.GetCount(); ++j)
			if(x.device_extensions[j].name != y.device_extensions[j].name || x.device_extensions[j].spec_version != y.device_extensions[j].spec_version)
				return false;
	}
	return true;
}

static bool SameRuntimeDiagnostics(const VulkanRuntimeDeviceDiagnostics& a, const VulkanRuntimeDeviceDiagnostics& b)
{
	return a.runtime_create_count == b.runtime_create_count && a.runtime_live_count == b.runtime_live_count && a.runtime_id == b.runtime_id && a.instance_create_count == b.instance_create_count && a.instance_live_count == b.instance_live_count && a.debug_messenger_create_count == b.debug_messenger_create_count && a.debug_messenger_live_count == b.debug_messenger_live_count && a.physical_device_discovery_count == b.physical_device_discovery_count && a.device_create_count == b.device_create_count && a.device_live_count == b.device_live_count && a.device_id == b.device_id && a.surface_create_count == b.surface_create_count && a.surface_live_count == b.surface_live_count && a.surface_id == b.surface_id;
}

bool TestVulkanSharedInstanceRegistryReuse(VulkanProcResolver resolver, VulkanSharedInstanceRegistryAcquireResult& first, VulkanSharedInstanceRegistryAcquireResult& second)
{
	first = VulkanSharedInstanceRegistryAcquireResult();
	second = VulkanSharedInstanceRegistryAcquireResult();
	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanSharedInstanceRegistry registry;
	VulkanInstanceOptions opts;
	opts.validation = true;
	opts.application_name = "RegistryFirst";
	VulkanInstanceOwnerOpenFailure first_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *first_entry = nullptr;
	bool ok = registry.Acquire(opts, first.preflight, first.debug_messenger_created, first.error, first_stage, resolver, first_entry, first.newly_created);
	first.entry = first_entry;
	first.failure_stage = (int)first_stage;
	first.acquire_count = first_entry ? first_entry->acquire_count : 0;
	first.diag = GetVulkanRuntimeDeviceDiagnostics();
	if(!ok || !first.entry)
		return false;
	opts.application_name = "RegistrySecond";
	VulkanInstanceOwnerOpenFailure second_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *second_entry = nullptr;
	ok = registry.Acquire(opts, second.preflight, second.debug_messenger_created, second.error, second_stage, resolver, second_entry, second.newly_created);
	second.entry = second_entry;
	second.failure_stage = (int)second_stage;
	second.acquire_count = second_entry ? second_entry->acquire_count : 0;
	second.diag = GetVulkanRuntimeDeviceDiagnostics();
	first.first_discovery_count = first.diag.physical_device_discovery_count;
	second.second_discovery_count = second.diag.physical_device_discovery_count;
	first.reusable_preflight_equal = SameReusablePreflight(first.preflight, second.preflight);
	if(!ok)
		return false;
	if(first.entry != second.entry)
		return false;
	if(!registry.Release((VulkanSharedInstanceEntry *)second.entry))
		return false;
	return registry.Release((VulkanSharedInstanceEntry *)first.entry);
}

bool TestVulkanSharedInstanceRegistryStability(VulkanProcResolver resolver, VulkanSharedInstanceRegistryAcquireResult& first, VulkanSharedInstanceRegistryAcquireResult& incompatible, VulkanSharedInstanceRegistryReleaseResult& release)
{
	first = VulkanSharedInstanceRegistryAcquireResult();
	incompatible = VulkanSharedInstanceRegistryAcquireResult();
	release = VulkanSharedInstanceRegistryReleaseResult();
	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanSharedInstanceRegistry registry;
	VulkanInstanceOptions opts;
	opts.validation = true;
	opts.win32_surface = false;
	opts.application_name = "RegistryStable";
	VulkanInstanceOwnerOpenFailure first_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *first_entry = nullptr;
	bool ok = registry.Acquire(opts, first.preflight, first.debug_messenger_created, first.error, first_stage, resolver, first_entry, first.newly_created);
	first.entry = first_entry;
	first.failure_stage = (int)first_stage;
	if(!ok || !first_entry || first_entry->acquire_count != 1 || !first_entry->opened || !first_entry->cleanup_ok) return false;
	VulkanSharedInstanceEntry *saved_first_entry = first_entry;
	first.original_acquire_count = saved_first_entry->acquire_count;
	VulkanInstanceCompatibility original_key = saved_first_entry->compatibility;
	first.compatibility_preserved = IsVulkanInstanceCompatible(saved_first_entry->compatibility, original_key);
	auto stable = [&]() {
		VulkanRuntimeDeviceDiagnostics current = GetVulkanRuntimeDeviceDiagnostics();
		return first.entry == saved_first_entry && saved_first_entry->acquire_count == 1 && saved_first_entry->opened && saved_first_entry->cleanup_ok && IsVulkanInstanceCompatible(saved_first_entry->compatibility, original_key) && current.runtime_live_count > 0 && current.instance_live_count > 0;
	};
	if(!stable()) return false;
	VulkanInstanceOptions validation_mismatch = opts;
	validation_mismatch.validation = false;
	VulkanInstanceOwnerOpenFailure incompatible_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *incompatible_entry = nullptr;
	if(!registry.Acquire(validation_mismatch, incompatible.preflight, incompatible.debug_messenger_created, incompatible.error, incompatible_stage, resolver, incompatible_entry, incompatible.newly_created) || !incompatible_entry || incompatible_entry == saved_first_entry)
		return false;
	incompatible.entry = incompatible_entry;
	incompatible.failure_stage = (int)incompatible_stage;
	first.validation_mismatch_entry_count = registry.GetEntryCount();
	first.identity_after_validation = stable();
	first.state_preserved_after_validation = first.identity_after_validation;
	if(!first.identity_after_validation)
		return false;
	if(!registry.Release((VulkanSharedInstanceEntry *)incompatible.entry))
		return false;
	first.after_first_removal_entry_count = registry.GetEntryCount();
	first.identity_after_first_removal = stable();
	first.state_preserved_after_first_removal = first.identity_after_first_removal;
	if(!first.identity_after_first_removal)
		return false;
	VulkanInstanceOptions surface_mismatch = opts;
	surface_mismatch.win32_surface = true;
	incompatible_entry = nullptr;
	if(!registry.Acquire(surface_mismatch, incompatible.preflight, incompatible.debug_messenger_created, incompatible.error, incompatible_stage, resolver, incompatible_entry, incompatible.newly_created) || !incompatible_entry || incompatible_entry == saved_first_entry)
		return false;
	first.surface_mismatch_entry_count = registry.GetEntryCount();
	first.identity_after_surface = stable();
	first.state_preserved_after_surface = first.identity_after_surface;
	if(!first.identity_after_surface || !registry.Release(incompatible_entry))
		return false;
	first.after_second_removal_entry_count = registry.GetEntryCount();
	first.identity_after_second_removal = stable();
	first.state_preserved_after_second_removal = first.identity_after_second_removal;
	first.stable_address_preserved = first.identity_after_second_removal;
	if(!first.identity_after_second_removal)
		return false;
	release.released = registry.Release(saved_first_entry);
	release.registry_entry_count = registry.GetEntryCount();
	release.diag = GetVulkanRuntimeDeviceDiagnostics();
	return release.released;
}

bool TestVulkanSharedInstanceRegistryFailures(VulkanProcResolver resolver, VulkanSharedInstanceRegistryAcquireResult& dispatch_failure, VulkanSharedInstanceRegistryAcquireResult& instance_failure, VulkanSharedInstanceRegistryAcquireResult& cleanup_failure)
{
	dispatch_failure = VulkanSharedInstanceRegistryAcquireResult();
	instance_failure = VulkanSharedInstanceRegistryAcquireResult();
	cleanup_failure = VulkanSharedInstanceRegistryAcquireResult();
	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanSharedInstanceRegistry registry;
	struct MissingProcReset {
		~MissingProcReset() { g_registry_test_missing_proc = nullptr; }
	} reset_missing_proc;
	VulkanInstanceOptions opts;
	opts.validation = false;
	opts.application_name = "RegistryFail";
	VulkanInstanceOwnerOpenFailure dispatch_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *dispatch_entry = nullptr;
	g_registry_test_missing_proc = "vkEnumerateInstanceLayerProperties";
	bool ok = registry.Acquire(opts, dispatch_failure.preflight, dispatch_failure.debug_messenger_created, dispatch_failure.error, dispatch_stage, resolver, dispatch_entry, dispatch_failure.newly_created);
	g_registry_test_missing_proc = nullptr;
	dispatch_failure.entry = dispatch_entry;
	dispatch_failure.failure_stage = (int)dispatch_stage;
	dispatch_failure.diag = GetVulkanRuntimeDeviceDiagnostics();
	dispatch_failure.registry_entry_count = registry.GetEntryCount();
	if(ok || dispatch_failure.entry || dispatch_failure.failure_stage != (int)VulkanInstanceOwnerOpenFailure::Dispatch || dispatch_failure.newly_created || dispatch_failure.registry_entry_count != 0 || dispatch_failure.diag.instance_create_count != 0 || dispatch_failure.diag.runtime_live_count != 0 || dispatch_failure.diag.instance_live_count != 0)
		return false;
	VulkanSharedInstanceEntry *recovery_entry = nullptr;
	VulkanPreflightReport recovery_preflight;
	bool recovery_debug = false, recovery_new = false;
	VulkanInstanceOwnerOpenFailure recovery_stage = VulkanInstanceOwnerOpenFailure::None;
	String recovery_error;
	if(!registry.Acquire(opts, recovery_preflight, recovery_debug, recovery_error, recovery_stage, resolver, recovery_entry, recovery_new) || !recovery_entry || !recovery_new || !registry.Release(recovery_entry))
		return false;
	dispatch_failure.recovery_succeeded = true;
	ClearVulkanRuntimeDeviceDiagnostics();

	VulkanInstanceOwnerOpenFailure instance_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *instance_entry = nullptr;
	opts.validation = true;
	g_registry_test_missing_proc = "vkGetDeviceProcAddr";
	bool instance_ok = registry.Acquire(opts, instance_failure.preflight, instance_failure.debug_messenger_created, instance_failure.error, instance_stage, resolver, instance_entry, instance_failure.newly_created);
	g_registry_test_missing_proc = nullptr;
	instance_failure.entry = instance_entry;
	instance_failure.failure_stage = (int)instance_stage;
	instance_failure.diag = GetVulkanRuntimeDeviceDiagnostics();
	instance_failure.registry_entry_count = registry.GetEntryCount();
	if(instance_ok || instance_failure.entry || instance_failure.failure_stage != (int)VulkanInstanceOwnerOpenFailure::Instance || instance_failure.error != "vkGetDeviceProcAddr" || instance_failure.newly_created || instance_failure.registry_entry_count != 0 || instance_failure.diag.instance_create_count != 1 || instance_failure.diag.runtime_live_count != 0 || instance_failure.diag.instance_live_count != 0 || instance_failure.diag.debug_messenger_live_count != 0 || instance_failure.diag.device_create_count != 0 || instance_failure.diag.device_live_count != 0)
		return false;
	if(!registry.Acquire(opts, recovery_preflight, recovery_debug, recovery_error, recovery_stage, resolver, recovery_entry, recovery_new) || !recovery_entry || !recovery_new || !registry.Release(recovery_entry))
		return false;
	instance_failure.recovery_succeeded = true;

	VulkanInstanceOwnerOpenFailure cleanup_stage = VulkanInstanceOwnerOpenFailure::None;
	VulkanSharedInstanceEntry *cleanup_entry = nullptr;
	opts.validation = true;
	if(!registry.Acquire(opts, cleanup_failure.preflight, cleanup_failure.debug_messenger_created, cleanup_failure.error, cleanup_stage, resolver, cleanup_entry, cleanup_failure.newly_created) || !cleanup_entry)
		return false;
	cleanup_failure.entry = cleanup_entry;
	cleanup_failure.failure_stage = (int)cleanup_stage;
	VulkanSharedInstanceEntry *retained = cleanup_entry;
	retained->owner.cleanup_ok = false;
	if(registry.Release(retained) || registry.GetEntryCount() != 1 || retained->acquire_count != 0 || retained->opened || retained->cleanup_ok || !retained->owner.IsCleared())
		return false;
	cleanup_failure.registry_entry_count = registry.GetEntryCount();
	cleanup_failure.acquire_count = retained->acquire_count;
	cleanup_failure.opened = retained->opened;
	cleanup_failure.cleanup_ok = retained->cleanup_ok;
	cleanup_failure.owner_cleared = retained->owner.IsCleared();
	cleanup_failure.diag = GetVulkanRuntimeDeviceDiagnostics();
	cleanup_failure.retained_runtime_create_count = cleanup_failure.diag.runtime_create_count;
	cleanup_failure.retained_instance_create_count = cleanup_failure.diag.instance_create_count;
	cleanup_failure.retained_debug_messenger_create_count = cleanup_failure.diag.debug_messenger_create_count;
	VulkanSharedInstanceEntry *refused = nullptr;
	VulkanPreflightReport refused_preflight;
	bool refused_new = false;
	VulkanInstanceOwnerOpenFailure refused_stage = VulkanInstanceOwnerOpenFailure::None;
	String refused_error;
	VulkanInstanceCompatibility retained_key = retained->compatibility;
	int retained_acquire_count = retained->acquire_count;
	bool retained_opened = retained->opened;
	bool retained_cleanup_ok = retained->cleanup_ok;
	bool retained_owner_cleared = retained->owner.IsCleared();
	cleanup_failure.pre_refusal_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(registry.Acquire(opts, refused_preflight, recovery_debug, refused_error, refused_stage, resolver, refused, refused_new) || refused || refused_new || refused_stage != VulkanInstanceOwnerOpenFailure::None || refused_error != "shared instance entry cleanup failed" || registry.GetEntryCount() != 1)
		return false;
	cleanup_failure.post_refusal_diag = GetVulkanRuntimeDeviceDiagnostics();
	cleanup_failure.refusal_diagnostics_unchanged = SameRuntimeDiagnostics(cleanup_failure.pre_refusal_diag, cleanup_failure.post_refusal_diag);
	cleanup_failure.retained_identity_preserved = retained == cleanup_entry;
	cleanup_failure.retained_state_preserved = retained->acquire_count == retained_acquire_count && retained->opened == retained_opened && retained->cleanup_ok == retained_cleanup_ok && retained->owner.IsCleared() == retained_owner_cleared;
	cleanup_failure.retained_compatibility_preserved = IsVulkanInstanceCompatible(retained->compatibility, retained_key);
	if(!cleanup_failure.refusal_diagnostics_unchanged || !cleanup_failure.retained_identity_preserved || !cleanup_failure.retained_state_preserved || !cleanup_failure.retained_compatibility_preserved)
		return false;
	cleanup_failure.replacement_refused = true;
	VulkanInstanceOptions incompatible_opts = opts;
	incompatible_opts.win32_surface = true;
	VulkanSharedInstanceEntry *incompatible_entry = nullptr;
	if(!registry.Acquire(incompatible_opts, recovery_preflight, recovery_debug, recovery_error, recovery_stage, resolver, incompatible_entry, recovery_new) || !incompatible_entry || !registry.Release(incompatible_entry) || registry.GetEntryCount() != 1 || retained != cleanup_entry || retained->acquire_count != 0 || retained->opened || retained->cleanup_ok || !retained->owner.IsCleared() || !IsVulkanInstanceCompatible(retained->compatibility, retained_key))
		return false;
	cleanup_failure.incompatible_succeeded = true;
	cleanup_failure.retained_registry_entry_count = registry.GetEntryCount();
	return cleanup_failure.diag.runtime_live_count == 0 && cleanup_failure.diag.instance_live_count == 0 && cleanup_failure.diag.debug_messenger_live_count == 0;
}

bool TestVulkanSharedInstanceRegistryInvalidRelease(VulkanProcResolver resolver, VulkanSharedInstanceRegistryReleaseResult& result)
{
	result = VulkanSharedInstanceRegistryReleaseResult();
	ClearVulkanRuntimeDeviceDiagnostics();
	VulkanSharedInstanceRegistry registry_a;
	VulkanSharedInstanceRegistry registry_b;
	VulkanInstanceOptions opts;
	opts.validation = true;
	VulkanSharedInstanceEntry *a = nullptr;
	VulkanSharedInstanceEntry *b = nullptr;
	VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
	bool created = false;
	VulkanPreflightReport preflight;
	String error;
	bool debug = false;
	if(!registry_a.Acquire(opts, preflight, debug, error, stage, resolver, a, created) || !a)
		return false;
	if(!registry_b.Acquire(opts, preflight, debug, error, stage, resolver, b, created) || !b)
		return false;
	VulkanSharedInstanceEntry foreign;
	int a_count = registry_a.GetEntryCount(), b_count = registry_b.GetEntryCount();
	int foreign_count = foreign.acquire_count;
	VulkanRuntimeDeviceDiagnostics before = GetVulkanRuntimeDeviceDiagnostics();
	result.null_release_rejected = !registry_a.Release(nullptr);
	result.foreign_release_rejected = !registry_a.Release(&foreign);
	result.cross_release_rejected = !registry_b.Release(a) && !registry_a.Release(b);
	VulkanRuntimeDeviceDiagnostics after = GetVulkanRuntimeDeviceDiagnostics();
	if(!result.null_release_rejected || !result.foreign_release_rejected || !result.cross_release_rejected || registry_a.GetEntryCount() != a_count || registry_b.GetEntryCount() != b_count || foreign.acquire_count != foreign_count || !SameRuntimeDiagnostics(before, after))
		return false;
	if(!registry_a.Release(a) || !registry_b.Release(b))
		return false;
	VulkanSharedInstanceEntry *removed = nullptr;
	if(!registry_a.Acquire(opts, preflight, debug, error, stage, resolver, removed, created) || !removed)
		return false;
	void *removed_identity = removed;
	if(!registry_a.Release(removed))
		return false;
	VulkanRuntimeDeviceDiagnostics removed_before = GetVulkanRuntimeDeviceDiagnostics();
	if(registry_a.Release((VulkanSharedInstanceEntry *)removed_identity))
		return false;
	VulkanRuntimeDeviceDiagnostics removed_after = GetVulkanRuntimeDeviceDiagnostics();
	if(!SameRuntimeDiagnostics(removed_before, removed_after))
		return false;
	result.removed_release_rejected = true;
	result.registry_a_count = registry_a.GetEntryCount();
	result.registry_b_count = registry_b.GetEntryCount();
	result.registry_entry_count = result.registry_a_count + result.registry_b_count;
	result.diag = GetVulkanRuntimeDeviceDiagnostics();
	return result.registry_entry_count == 0;
}

bool TestVulkanSharedInstanceLease(VulkanProcResolver resolver, VulkanSharedInstanceLeaseTestResult& result)
{
	result = VulkanSharedInstanceLeaseTestResult();
	VulkanInstanceOptions opts;
	opts.validation = true;
	opts.application_name = "LeaseTest";
	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease lease;
		VulkanPreflightReport preflight;
		bool debug = false, created = false;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		if(!lease.Acquire(registry, opts, preflight, debug, error, stage, resolver, &created) || !lease.IsAcquired() || registry.GetEntryCount() != 1 || ((VulkanSharedInstanceEntry *)lease.entry)->acquire_count != 1 || !created)
			return false;
		result.automatic_release = true;
	}
	VulkanRuntimeDeviceDiagnostics after_auto = GetVulkanRuntimeDeviceDiagnostics();
	if(after_auto.runtime_live_count != 0 || after_auto.instance_live_count != 0 || after_auto.debug_messenger_live_count != 0 || after_auto.device_live_count != 0)
		return false;

	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease first, second;
		VulkanPreflightReport p1, p2;
		bool d1 = false, d2 = false, n1 = false, n2 = false;
		VulkanInstanceOwnerOpenFailure s1 = VulkanInstanceOwnerOpenFailure::None, s2 = VulkanInstanceOwnerOpenFailure::None;
		String e1, e2;
		VulkanInstanceOptions second_opts = opts;
		second_opts.application_name = "LeaseSecond";
		if(!first.Acquire(registry, opts, p1, d1, e1, s1, resolver, &n1) || !second.Acquire(registry, second_opts, p2, d2, e2, s2, resolver, &n2) || first.entry != second.entry || !n1 || n2 || ((VulkanSharedInstanceEntry *)first.entry)->acquire_count != 2 || registry.GetEntryCount() != 1)
			return false;
		VulkanRuntimeDeviceDiagnostics before_reset = GetVulkanRuntimeDeviceDiagnostics();
		if(!first.Reset() || first.IsAcquired() || ((VulkanSharedInstanceEntry *)second.entry)->acquire_count != 1 || registry.GetEntryCount() != 1 || !SameRuntimeDiagnostics(before_reset, GetVulkanRuntimeDeviceDiagnostics()))
			return false;
		result.non_final_release = true;
		if(!second.Reset() || registry.GetEntryCount() != 0)
			return false;
		result.two_lease_reuse = true;
		result.final_release = true;
	}

	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease lease;
		VulkanPreflightReport p;
		bool debug = false, created = false;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		if(!lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created) || !lease.Reset()) return false;
		VulkanRuntimeDeviceDiagnostics before = GetVulkanRuntimeDeviceDiagnostics();
		if(!lease.Reset() || lease.IsAcquired() || registry.GetEntryCount() != 0 || !SameRuntimeDiagnostics(before, GetVulkanRuntimeDeviceDiagnostics())) return false;
		result.reset_idempotent = true;
	}

	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease source;
		VulkanPreflightReport p;
		bool debug = false, created = false;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		if(!source.Acquire(registry, opts, p, debug, error, stage, resolver, &created)) return false;
		void *identity = source.entry;
		VulkanRuntimeDeviceDiagnostics before = GetVulkanRuntimeDeviceDiagnostics();
		VulkanSharedInstanceLease destination(static_cast<VulkanSharedInstanceLease&&>(source));
		result.source_empty_after_move = source.IsEmpty();
		result.destination_registry_preserved = destination.registry == &registry;
		result.destination_entry_preserved = destination.entry == identity;
		result.move_count_preserved = ((VulkanSharedInstanceEntry *)destination.entry)->acquire_count == 1;
		result.move_registry_count_preserved = registry.GetEntryCount() == 1;
		result.move_diagnostics_unchanged = SameRuntimeDiagnostics(before, GetVulkanRuntimeDeviceDiagnostics());
		if(!result.source_empty_after_move || !destination.IsAcquired() || !result.destination_registry_preserved || !result.destination_entry_preserved || !result.move_count_preserved || !result.move_registry_count_preserved || !result.move_diagnostics_unchanged || !destination.Reset() || registry.GetEntryCount() != 0) return false;
		result.move_transfer = true;
	}

	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease lease;
		VulkanPreflightReport p;
		bool debug = false, created = false;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		if(!lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created)) return false;
		void *identity = lease.entry;
		int count = ((VulkanSharedInstanceEntry *)identity)->acquire_count;
		VulkanSharedInstanceRegistry *lease_registry = lease.registry;
		VulkanRuntimeDeviceDiagnostics before = GetVulkanRuntimeDeviceDiagnostics();
		VulkanPreflightReport refused_p;
		bool refused_debug = true, refused_new = true;
		VulkanInstanceOwnerOpenFailure refused_stage = VulkanInstanceOwnerOpenFailure::Dispatch;
		String refused_error;
		if(lease.Acquire(registry, opts, refused_p, refused_debug, refused_error, refused_stage, resolver, &refused_new) || lease.registry != lease_registry || lease.entry != identity || !lease.IsAcquired() || refused_new || refused_debug || refused_stage != VulkanInstanceOwnerOpenFailure::None || refused_error != "shared instance lease is already acquired" || ((VulkanSharedInstanceEntry *)identity)->acquire_count != count || registry.GetEntryCount() != 1 || !SameRuntimeDiagnostics(before, GetVulkanRuntimeDeviceDiagnostics()) || refused_p.status != VulkanProbeStatus::RuntimeUnavailable) return false;
		result.occupied_identity_preserved = lease.registry == lease_registry && lease.entry == identity;
		result.occupied_acquire_count_preserved = ((VulkanSharedInstanceEntry *)identity)->acquire_count == count;
		result.occupied_registry_preserved = registry.GetEntryCount() == 1;
		result.occupied_diagnostics_unchanged = SameRuntimeDiagnostics(before, GetVulkanRuntimeDeviceDiagnostics());
		result.occupied_outputs_reset = refused_p.status == VulkanProbeStatus::RuntimeUnavailable && !refused_debug;
		if(!lease.Reset()) return false;
		result.occupied_refused = true;
	}

	struct MissingProcReset { ~MissingProcReset() { g_registry_test_missing_proc = nullptr; } } reset_missing_proc;
	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease lease;
		VulkanPreflightReport p;
		bool debug = false, created = true;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		g_registry_test_missing_proc = "vkEnumerateInstanceLayerProperties";
		VulkanRuntimeDeviceDiagnostics failed_diag;
		if(lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created) || !lease.IsEmpty() || lease.registry != nullptr || lease.entry != nullptr || created || stage != VulkanInstanceOwnerOpenFailure::Dispatch || registry.GetEntryCount() != 0 || (failed_diag = GetVulkanRuntimeDeviceDiagnostics()).runtime_live_count != 0 || failed_diag.instance_live_count != 0 || failed_diag.debug_messenger_live_count != 0 || failed_diag.surface_live_count != 0 || failed_diag.device_live_count != 0) return false;
		g_registry_test_missing_proc = nullptr;
		if(!lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created) || !lease.Reset()) return false;
		result.dispatch_failure_empty = true;
		result.dispatch_lease_empty = true;
		result.dispatch_outputs_complete = true;
	}
	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceLease lease;
		VulkanPreflightReport p;
		bool debug = false, created = true;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		g_registry_test_missing_proc = "vkGetDeviceProcAddr";
		opts.validation = true;
		VulkanRuntimeDeviceDiagnostics failed_diag;
		if(lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created) || !lease.IsEmpty() || lease.registry != nullptr || lease.entry != nullptr || created || stage != VulkanInstanceOwnerOpenFailure::Instance || error != "vkGetDeviceProcAddr" || registry.GetEntryCount() != 0 || (failed_diag = GetVulkanRuntimeDeviceDiagnostics()).runtime_live_count != 0 || failed_diag.instance_live_count != 0 || failed_diag.debug_messenger_live_count != 0 || failed_diag.surface_live_count != 0 || failed_diag.device_live_count != 0) return false;
		g_registry_test_missing_proc = nullptr;
		if(!lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created) || !lease.Reset()) return false;
		result.instance_failure_empty = true;
		result.instance_lease_empty = true;
		result.instance_outputs_complete = true;
		result.recovery = true;
	}

	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSharedInstanceRegistry registry;
		VulkanSharedInstanceEntry *retained = nullptr;
		VulkanRuntimeDeviceDiagnostics before_destructor;
		{
			VulkanSharedInstanceLease lease;
			VulkanPreflightReport p;
			bool debug = false, created = false;
			VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
			String error;
			if(!lease.Acquire(registry, opts, p, debug, error, stage, resolver, &created) || !lease.entry) return false;
			retained = lease.entry;
			retained->owner.cleanup_ok = false;
			if(lease.Reset() || !lease.IsEmpty() || registry.GetEntryCount() != 1 || retained->acquire_count != 0 || retained->opened || retained->cleanup_ok || !retained->owner.IsCleared()) return false;
			result.cleanup_failure_empty = true;
			result.retained_acquire_count = retained->acquire_count;
			result.retained_opened = retained->opened;
			result.retained_cleanup_ok = retained->cleanup_ok;
			result.retained_owner_cleared = retained->owner.IsCleared();
			before_destructor = GetVulkanRuntimeDeviceDiagnostics();
		}
		result.destructor_pre_diag = before_destructor;
		result.destructor_post_diag = GetVulkanRuntimeDeviceDiagnostics();
		result.retained_identity_after_destructor = retained && registry.GetEntryCount() == 1 && retained->acquire_count == 0 && !retained->opened && !retained->cleanup_ok && retained->owner.IsCleared();
		result.destructor_diagnostics_unchanged = SameRuntimeDiagnostics(result.destructor_pre_diag, result.destructor_post_diag);
		if(!result.retained_identity_after_destructor || !result.destructor_diagnostics_unchanged) return false;
		result.no_double_release = true;
		VulkanSharedInstanceLease fresh;
		VulkanPreflightReport refused_p;
		bool refused_debug = true, refused_new = true;
		VulkanInstanceOwnerOpenFailure refused_stage = VulkanInstanceOwnerOpenFailure::Dispatch;
		String refused_error;
		result.refusal_pre_diag = GetVulkanRuntimeDeviceDiagnostics();
		if(fresh.Acquire(registry, opts, refused_p, refused_debug, refused_error, refused_stage, resolver, &refused_new) || !fresh.IsEmpty() || refused_new || refused_stage != VulkanInstanceOwnerOpenFailure::None || refused_error != "shared instance entry cleanup failed" || registry.GetEntryCount() != 1) return false;
		result.refusal_post_diag = GetVulkanRuntimeDeviceDiagnostics();
		result.refusal_diagnostics_unchanged = SameRuntimeDiagnostics(result.refusal_pre_diag, result.refusal_post_diag);
		result.same_key_refused = result.refusal_diagnostics_unchanged;
		result.refusal_registry_entry_count = registry.GetEntryCount();
		VulkanInstanceOptions incompatible_opts = opts;
		incompatible_opts.win32_surface = true;
		VulkanSharedInstanceLease incompatible_lease;
		if(!incompatible_lease.Acquire(registry, incompatible_opts, refused_p, refused_debug, refused_error, refused_stage, resolver, &refused_new) || !incompatible_lease.Reset() || registry.GetEntryCount() != 1 || retained->acquire_count != 0 || retained->opened || retained->cleanup_ok || !retained->owner.IsCleared()) return false;
		result.incompatible_lease_succeeded = true;
		result.registry_entry_count = registry.GetEntryCount();
	}

	{
		VulkanSharedInstanceRegistry registry;
		VulkanPreflightReport p;
		bool debug = false, created = false;
		VulkanInstanceOwnerOpenFailure stage = VulkanInstanceOwnerOpenFailure::None;
		String error;
		VulkanSharedInstanceEntry *entry = nullptr;
		if(!registry.Acquire(opts, p, debug, error, stage, resolver, entry, created) || !registry.Acquire(opts, p, debug, error, stage, resolver, entry, created)) return false;
		VulkanSharedInstanceRegistry::ReleaseOutcome non_final = registry.ReleaseDetailed(entry);
		VulkanSharedInstanceRegistry::ReleaseOutcome final = registry.ReleaseDetailed(entry);
		VulkanSharedInstanceRegistry::ReleaseOutcome invalid = registry.ReleaseDetailed(nullptr);
		VulkanSharedInstanceEntry *failed_entry = nullptr;
		if(!registry.Acquire(opts, p, debug, error, stage, resolver, failed_entry, created) || !failed_entry) return false;
		failed_entry->owner.cleanup_ok = false;
		VulkanSharedInstanceRegistry::ReleaseOutcome cleanup_failed = registry.ReleaseDetailed(failed_entry);
		result.non_final_outcome = non_final.acquisition_released && non_final.entry_retained && !non_final.entry_removed && non_final.cleanup_ok;
		result.final_outcome = final.acquisition_released && !final.entry_retained && final.entry_removed && final.cleanup_ok;
		result.null_outcome_invalid = !invalid.acquisition_released && !invalid.entry_retained && !invalid.entry_removed && !invalid.cleanup_ok;
		result.cleanup_failed_outcome = cleanup_failed.acquisition_released && cleanup_failed.entry_retained && !cleanup_failed.entry_removed && !cleanup_failed.cleanup_ok;
		if(!result.non_final_outcome || !result.final_outcome || !result.null_outcome_invalid || !result.cleanup_failed_outcome || registry.GetEntryCount() != 1) return false;
		VulkanSharedInstanceRegistry foreign_registry;
		VulkanSharedInstanceEntry foreign;
		VulkanRuntimeDeviceDiagnostics foreign_before = GetVulkanRuntimeDeviceDiagnostics();
		VulkanSharedInstanceRegistry::ReleaseOutcome foreign_outcome = foreign_registry.ReleaseDetailed(&foreign);
		result.foreign_outcome_invalid = !foreign_outcome.acquisition_released && !foreign_outcome.entry_retained && !foreign_outcome.entry_removed && !foreign_outcome.cleanup_ok && SameRuntimeDiagnostics(foreign_before, GetVulkanRuntimeDeviceDiagnostics()) && foreign.acquire_count == 0;
		VulkanSharedInstanceRegistry registry_a, registry_b;
		VulkanSharedInstanceEntry *a = nullptr, *b = nullptr;
		if(!registry_a.Acquire(opts, p, debug, error, stage, resolver, a, created) || !registry_b.Acquire(opts, p, debug, error, stage, resolver, b, created)) return false;
		int a_count = registry_a.GetEntryCount(), b_count = registry_b.GetEntryCount();
		VulkanRuntimeDeviceDiagnostics cross_before = GetVulkanRuntimeDeviceDiagnostics();
		VulkanSharedInstanceRegistry::ReleaseOutcome cross_a = registry_b.ReleaseDetailed(a);
		VulkanSharedInstanceRegistry::ReleaseOutcome cross_b = registry_a.ReleaseDetailed(b);
		result.cross_outcome_invalid = !cross_a.acquisition_released && !cross_a.entry_retained && !cross_a.entry_removed && !cross_a.cleanup_ok && !cross_b.acquisition_released && !cross_b.entry_retained && !cross_b.entry_removed && !cross_b.cleanup_ok && registry_a.GetEntryCount() == a_count && registry_b.GetEntryCount() == b_count && SameRuntimeDiagnostics(cross_before, GetVulkanRuntimeDeviceDiagnostics());
		if(!result.foreign_outcome_invalid || !result.cross_outcome_invalid || !registry_a.Release(a) || !registry_b.Release(b)) return false;
		result.detailed_outcomes = true;
	}
	result.diag = GetVulkanRuntimeDeviceDiagnostics();
	return result.automatic_release && result.two_lease_reuse && result.non_final_release && result.final_release && result.reset_idempotent && result.move_transfer && result.occupied_refused && result.dispatch_failure_empty && result.instance_failure_empty && result.recovery && result.cleanup_failure_empty && result.no_double_release && result.detailed_outcomes && result.diag.runtime_live_count == 0 && result.diag.instance_live_count == 0 && result.diag.debug_messenger_live_count == 0 && result.diag.surface_live_count == 0 && result.diag.device_live_count == 0;
}

bool TestVulkanGroupedSurfaceSessions(VulkanProcResolver resolver, VulkanGroupedSurfaceSessionTestResult& result)
{
	result = VulkanGroupedSurfaceSessionTestResult();
	struct ValidationReset { ~ValidationReset() { ClearVulkanValidationTestInjection(); } } validation_reset;
	HWND first_hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	HWND second_hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	if(!first_hwnd || !second_hwnd) {
		if(first_hwnd) DestroyWindow(first_hwnd);
		if(second_hwnd) DestroyWindow(second_hwnd);
		return false;
	}
	struct WindowGuard {
		HWND first;
		HWND second;
		~WindowGuard() { if(first) DestroyWindow(first); if(second) DestroyWindow(second); }
	} windows{first_hwnd, second_hwnd};
	GpuNativeWindowDesc first_window;
	first_window.kind = GpuNativeWindowKind::Win32;
	first_window.handle = (uint64_t)(uintptr_t)first_hwnd;
	GpuNativeWindowDesc second_window = first_window;
	second_window.handle = (uint64_t)(uintptr_t)second_hwnd;
	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSurfaceSessionGroup group;
		VulkanSurfaceSession first(group), second(group);
		bool first_ok = first.Open(true, first_window, resolver);
		bool second_ok = second.Open(true, second_window, resolver);
		if(!first_ok || !second_ok || !first.IsReady() || !second.IsReady() || !second.GetReport().shared_instance_reused) return false;
		result.compatible_diag = GetVulkanRuntimeDeviceDiagnostics();
		result.compatible_registry_entries = group.impl->registry.GetEntryCount();
		if(result.compatible_diag.runtime_create_count != 1 || result.compatible_diag.runtime_live_count != 1 || result.compatible_diag.instance_create_count != 1 || result.compatible_diag.instance_live_count != 1 || result.compatible_diag.surface_create_count != 2 || result.compatible_diag.surface_live_count != 2 || result.compatible_diag.device_create_count != 2 || result.compatible_diag.device_live_count != 2) return false;
		result.compatible_shared = true;
		String second_error = second.GetError();
		second.Close();
		result.non_final_diag = GetVulkanRuntimeDeviceDiagnostics();
		result.non_final_registry_entries = group.impl->registry.GetEntryCount();
		if(!first.IsReady() || second.GetError() != second_error || result.non_final_diag.runtime_live_count != 1 || result.non_final_diag.instance_live_count != 1 || result.non_final_diag.surface_live_count != 1 || result.non_final_diag.device_live_count != 1) return false;
		result.non_final_close = true;
		first.Close();
		result.final_diag = GetVulkanRuntimeDeviceDiagnostics();
		result.final_registry_entries = group.impl->registry.GetEntryCount();
		if(result.final_diag.runtime_live_count != 0 || result.final_diag.instance_live_count != 0 || result.final_diag.surface_live_count != 0 || result.final_diag.device_live_count != 0) return false;
		result.final_close = true;
	}
	{
		VulkanSurfaceSessionGroup group;
		VulkanSurfaceSession first(group), second(group);
		if(!first.Open(true, first_window, resolver) || !second.Open(true, second_window, resolver)) return false;
		second.Close();
		if(!first.IsReady()) return false;
		first.Close();
		result.reverse_close = true;
	}
	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSurfaceSessionGroup group;
		VulkanSurfaceSession no_validation(group), validation(group);
		if(!no_validation.Open(false, first_window, resolver) || !validation.Open(true, second_window, resolver)) return false;
		VulkanRuntimeDeviceDiagnostics incompatible_diag = GetVulkanRuntimeDeviceDiagnostics();
		if(incompatible_diag.runtime_create_count != 2 || incompatible_diag.runtime_live_count != 2 || incompatible_diag.instance_create_count != 2 || incompatible_diag.instance_live_count != 2) return false;
		result.incompatible_registry_entries = group.impl->registry.GetEntryCount();
		no_validation.Close();
		if(!validation.IsReady()) return false;
		validation.Close();
		result.incompatible_entries = true;
	}
	struct MissingProcReset { ~MissingProcReset() { g_registry_test_missing_proc = nullptr; } } reset_missing_proc;
	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSurfaceSessionGroup group;
		VulkanSurfaceSession first(group), second(group);
		if(!first.Open(true, first_window, resolver)) return false;
		g_registry_test_missing_proc = "vkCreateWin32SurfaceKHR";
		bool failed_open = second.Open(true, second_window, resolver);
		if(failed_open || second.IsReady() || !second.GetReport().shared_instance_released || !first.IsReady()) return false;
		result.failure_registry_entries = group.impl->registry.GetEntryCount();
		g_registry_test_missing_proc = nullptr;
		bool recovery_open = second.Open(true, second_window, resolver);
		if(!recovery_open || !second.IsReady()) return false;
		second.Close();
		first.Close();
		result.post_lease_failure = true;
		result.recovery = true;
	}
	{
		VulkanValidationTestInjection injection;
		injection.enabled = true;
		injection.point = VulkanValidationTestPoint::DuringDeviceCleanup;
		injection.force_device_cleanup_failure = true;
		SetVulkanValidationTestInjection(injection);
		VulkanSurfaceSessionGroup group;
		VulkanSurfaceSession session(group);
		if(!session.Open(true, first_window, resolver)) return false;
		session.Close();
		ClearVulkanValidationTestInjection();
		const VulkanSurfaceReport& report = session.GetReport();
		VulkanRuntimeDeviceDiagnostics cleanup_diag = GetVulkanRuntimeDeviceDiagnostics();
		if(report.device_cleanup_ok || !report.surface_cleanup_ok || !report.shared_instance_released || !report.cleanup_state_cleared || report.clean_shutdown || group.impl->registry.GetEntryCount() != 0 || cleanup_diag.runtime_live_count != 0 || cleanup_diag.instance_live_count != 0 || cleanup_diag.surface_live_count != 0 || cleanup_diag.device_live_count != 0) return false;
		result.device_cleanup_failure_non_short_circuit = true;
	}
	result.diag = GetVulkanRuntimeDeviceDiagnostics();
	return result.compatible_shared && result.non_final_close && result.final_close && result.reverse_close && result.incompatible_entries && result.post_lease_failure && result.recovery && result.diag.runtime_live_count == 0 && result.diag.instance_live_count == 0 && result.diag.debug_messenger_live_count == 0 && result.diag.surface_live_count == 0 && result.diag.device_live_count == 0;
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

	VulkanInstanceOwner owner;
	VulkanDeviceContext device;
	String error;

	VulkanInstanceOptions instance_options;
	instance_options.validation = request_validation;
	instance_options.application_name = "VulkanBootstrap";
	VulkanInstanceOwnerOpenFailure open_failure = VulkanInstanceOwnerOpenFailure::None;
	if(!owner.Open(instance_options, report.preflight, report.debug_messenger_created, error, open_failure, resolver)) {
		if(open_failure == VulkanInstanceOwnerOpenFailure::Dispatch) {
			report.status = MapDispatchError(error);
			if(report.status == VulkanProbeStatus::RuntimeUnavailable)
				report.runtime_error = error;
			else
				report.loader_error = error;
		}
		else {
			report.status = MapInstanceError(error);
			report.instance_error = error;
			report.validation_available = report.preflight.validation_available;
			report.debug_utils_available = report.preflight.debug_utils_available;
		}
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, owner.instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, owner, device, CloseBootstrapContexts(owner, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	uint32_t loader_version = VK_API_VERSION_1_0;
	if(!QueryLoaderVersion(owner.dispatch.enumerate_instance_version, loader_version, error)) {
		report.status = VulkanProbeStatus::LoaderTooOld;
		report.loader_error = error;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		FinalizeBootstrapCleanup(report, owner, device, CloseBootstrapContexts(owner, device));
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
		FinalizeBootstrapCleanup(report, owner, device, CloseBootstrapContexts(owner, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}

	report.preflight.instance_created = true;
	report.validation_available = report.preflight.validation_available;
	report.debug_utils_available = report.preflight.debug_utils_available;

	Vector<VulkanDiscoveredDevice> discovered;
	if(!owner.instance.EnumeratePhysicalDevices(discovered, error)) {
		report.status = MapDeviceError(error);
		report.physical_device_error = error;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, owner.instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, owner, device, CloseBootstrapContexts(owner, device));
		report.preflight.clean_shutdown = report.clean_shutdown;
		report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
		return false;
	}
	if(discovered.IsEmpty()) {
		report.status = VulkanProbeStatus::NoPhysicalDevices;
		report.status_text = StatusText(report.status);
		report.preflight.status = report.status;
		report.preflight.status_text = report.status_text;
		CopyValidationCapture(report, owner.instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, owner, device, CloseBootstrapContexts(owner, device));
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
		CopyValidationCapture(report, owner.instance.capture);
		report.preflight.validation_warning_count = report.validation_warning_count;
		report.preflight.validation_error_count = report.validation_error_count;
		CopyMessages(report.preflight.validation_messages, report.validation_messages);
		FinalizeBootstrapCleanup(report, owner, device, CloseBootstrapContexts(owner, device));
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

	report.validation_warning_count = owner.instance.capture.warnings;
	report.validation_error_count = owner.instance.capture.errors;
	CopyMessages(report.validation_messages, owner.instance.capture.messages);
	report.preflight.validation_warning_count = report.validation_warning_count;
	report.preflight.validation_error_count = report.validation_error_count;
	CopyMessages(report.preflight.validation_messages, report.validation_messages);
	report.status = report.validation_error_count > 0 ? VulkanProbeStatus::ValidationErrorsReported : VulkanProbeStatus::Ok;
	report.status_text = StatusText(report.status);
	report.preflight.status = report.status;
	report.preflight.status_text = report.status_text;

	if(request_validation)
		InjectValidationIfRequested(owner.instance.capture, VulkanValidationTestPoint::BeforeDeviceCreation);
	if(create_device) {
		if(!device.Open(owner.instance, selected->handle, report.selected_device, report, error)) {
			report.status = MapDeviceError(error);
			report.device_error = error;
			report.device_cleanup_ok = device.Close();
			report.device_cleanup_result = device.cleanup_result;
			report.device_cleanup_error = device.cleanup_error;
			CopyValidationCapture(report, owner.instance.capture);
			CopyMessages(report.preflight.validation_messages, report.validation_messages);
			report.preflight.validation_warning_count = report.validation_warning_count;
			report.preflight.validation_error_count = report.validation_error_count;
			report.preflight.debug_utils_available = owner.instance.debug_utils_available;
			report.preflight.validation_available = report.validation_available;
			bool owner_cleanup_ok = owner.Close();
			report.instance_cleanup_ok = owner.instance.cleanup_ok;
			report.dispatch_cleanup_ok = owner.dispatch.cleanup_ok;
			FinalizeBootstrapStatus(report, create_device);
			FinalizeBootstrapCleanup(report, owner, device, owner_cleanup_ok && report.device_cleanup_ok);
			report.preflight.clean_shutdown = report.clean_shutdown;
			report.preflight.cleanup_state_cleared = report.cleanup_state_cleared;
			return false;
		}
		report.logical_device_created = true;
		report.graphics_queue_acquired = true;
		report.selected_device.logical_device_created = true;
		report.selected_device.graphics_queue_acquired = true;
		if(request_validation) {
			InjectValidationIfRequested(owner.instance.capture, VulkanValidationTestPoint::AfterDeviceCreation);
			InjectValidationIfRequested(owner.instance.capture, VulkanValidationTestPoint::DuringDeviceCleanup);
		}
		report.device_cleanup_ok = device.Close();
		report.device_cleanup_result = device.cleanup_result;
		report.device_cleanup_error = device.cleanup_error;
	}

	CopyValidationCapture(report, owner.instance.capture);
	CopyMessages(report.preflight.validation_messages, report.validation_messages);
	report.preflight.validation_warning_count = report.validation_warning_count;
	report.preflight.validation_error_count = report.validation_error_count;
	report.preflight.debug_utils_available = owner.instance.debug_utils_available;
	report.preflight.validation_available = report.validation_available;
	bool owner_cleanup_ok = owner.Close();
	report.instance_cleanup_ok = owner.instance.cleanup_ok;
	report.dispatch_cleanup_ok = owner.dispatch.cleanup_ok;
	FinalizeBootstrapStatus(report, create_device);
	FinalizeBootstrapCleanup(report, owner, device, owner_cleanup_ok && (!create_device || report.device_cleanup_ok));
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

static void FinalizeSurfaceCleanup(VulkanSurfaceReport& report, const VulkanSurfaceContext& ctx, const VulkanDeviceContext& device, bool lease_empty, bool cleanup_ok)
{
	report.cleanup_state_cleared = ctx.IsCleared() && device.IsCleared() && lease_empty;
	report.clean_shutdown = cleanup_ok && report.cleanup_state_cleared && report.surface_cleanup_ok && report.device_cleanup_ok && report.instance_cleanup_ok && report.dispatch_cleanup_ok && report.shared_instance_cleanup_ok;
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
	shared_instance_acquired = src.shared_instance_acquired;
	shared_instance_reused = src.shared_instance_reused;
	shared_instance_released = src.shared_instance_released;
	shared_instance_cleanup_ok = src.shared_instance_cleanup_ok;
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
	std::unique_ptr<VulkanSurfaceSessionGroup> owned_group;
	VulkanSurfaceSessionGroup *group = nullptr;
	VulkanSharedInstanceLease lease;
	VulkanSurfaceContext ctx;
	VulkanDeviceContext device;
	VulkanSurfaceReport report;
	String error;
	bool open = false;
	bool ready = false;
};

VulkanSurfaceSessionGroup::VulkanSurfaceSessionGroup()
	: impl(new Impl)
{
}

VulkanSurfaceSessionGroup::~VulkanSurfaceSessionGroup() = default;

VulkanSurfaceSession::VulkanSurfaceSession()
    : impl(new Impl)
{
	impl->owned_group.reset(new VulkanSurfaceSessionGroup);
	impl->group = impl->owned_group.get();
}

VulkanSurfaceSession::VulkanSurfaceSession(VulkanSurfaceSessionGroup& group)
	: impl(new Impl)
{
	impl->group = &group;
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
	VulkanInstanceOwner *owner = impl.lease.GetOwner();
	impl.report.instance_cleanup_ok = owner ? owner->cleanup_ok : impl.report.instance_cleanup_ok;
	impl.report.surface_cleanup_ok = impl.ctx.cleanup_ok;
	impl.report.device_cleanup_ok = impl.device.cleanup_ok;
	impl.report.dispatch_cleanup_ok = owner ? owner->dispatch.cleanup_ok : impl.report.dispatch_cleanup_ok;
	impl.report.shared_instance_released = impl.lease.IsEmpty();
	impl.report.native_window = GpuNativeWindowDesc();
	FinalizeSurfaceCleanup(impl.report, impl.ctx, impl.device, impl.lease.IsEmpty(), cleanup_ok);
	impl.ready = false;
	impl.open = false;
}

static void CleanupSurfaceSession(VulkanSurfaceSession::Impl& impl)
{
	if(VulkanInstanceOwner *owner = impl.lease.GetOwner()) {
		impl.report.instance_cleanup_ok = owner->cleanup_ok;
		impl.report.dispatch_cleanup_ok = owner->dispatch.cleanup_ok;
	}
	bool had_lease = impl.lease.IsAcquired();
	bool device_ok = impl.device.cleanup_ok && (!impl.device.device || impl.device.Close());
	bool surface_ok = impl.ctx.cleanup_ok && impl.ctx.Close();
	bool shared_ok = had_lease ? impl.lease.Reset() : (impl.report.shared_instance_released && impl.report.shared_instance_cleanup_ok);
	if(had_lease) {
		impl.report.shared_instance_released = impl.lease.IsEmpty();
		impl.report.shared_instance_cleanup_ok = shared_ok;
	}
	FinalizeSurfaceSession(impl, device_ok && surface_ok && (!had_lease || shared_ok));
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

	auto fail = [&](const String& message) {
		impl->error = message;
		Close();
		return false;
	};
	auto fail_device_setup = [&](const String& message) {
		impl->report.status = VulkanProbeStatus::DeviceCreationFailed;
		impl->report.device_error = message;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		return fail(message);
	};

	VulkanInstanceOwnerOpenFailure failure_stage = VulkanInstanceOwnerOpenFailure::None;
	if(native_window.kind != GpuNativeWindowKind::Win32 || native_window.handle == 0 || !IsWindow((HWND)(uintptr_t)native_window.handle))
		return fail("invalid native handle");
	VulkanInstanceOptions instance_options;
	instance_options.validation = request_validation;
	instance_options.win32_surface = true;
	instance_options.application_name = "VulkanSurfaceProbe";
	bool debug_messenger_created = false;
	bool newly_created = false;
	if(!impl->lease.Acquire(impl->group->impl->registry, instance_options, impl->report.preflight, debug_messenger_created, impl->error, failure_stage, resolver, &newly_created)) {
		if(failure_stage == VulkanInstanceOwnerOpenFailure::Dispatch) {
			impl->report.status = MapDispatchError(impl->error);
			if(impl->report.status == VulkanProbeStatus::RuntimeUnavailable)
				impl->report.runtime_error = impl->error;
			else
				impl->report.loader_error = impl->error;
		}
		else if(failure_stage == VulkanInstanceOwnerOpenFailure::Instance) {
			if(impl->error == "VK_LAYER_KHRONOS_validation not present")
				impl->report.status = VulkanProbeStatus::ValidationUnavailable;
			else if(impl->error == "VK_EXT_debug_utils not present")
				impl->report.status = VulkanProbeStatus::DebugUtilsUnavailable;
			else
				impl->report.status = VulkanProbeStatus::SurfaceUnsupported;
			impl->report.instance_error = impl->error;
		}
		else {
			if(impl->error.StartsWith("vkCreateWin32SurfaceKHR failed"))
				impl->report.status = VulkanProbeStatus::SurfaceCreationFailed;
			else
				impl->report.status = VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
			impl->report.surface_error = impl->error;
		}
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CleanupSurfaceSession(*impl);
		return false;
	}
	impl->report.shared_instance_acquired = true;
	impl->report.shared_instance_reused = !newly_created;
	impl->report.validation_available = impl->report.preflight.validation_available;
	impl->report.debug_utils_available = impl->report.preflight.debug_utils_available;
	VulkanInstanceOwner *owner = impl->lease.GetOwner();
	if(!owner || !impl->ctx.Open(*owner, request_validation, native_window, impl->report, impl->error, failure_stage, resolver)) {
		impl->report.status = impl->error.StartsWith("vkCreateWin32SurfaceKHR failed") ? VulkanProbeStatus::SurfaceCreationFailed : VulkanProbeStatus::RequiredLoaderFunctionUnavailable;
		impl->report.surface_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CleanupSurfaceSession(*impl);
		return false;
	}

	uint32_t loader_version = VK_API_VERSION_1_0;
	if(!QueryLoaderVersion(owner->dispatch.enumerate_instance_version, loader_version, impl->error)) {
		impl->report.status = VulkanProbeStatus::LoaderTooOld;
		impl->report.loader_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CleanupSurfaceSession(*impl);
		return false;
	}
	impl->report.preflight.loader_available = true;
	impl->report.preflight.loader_version = loader_version;
	if(loader_version < VK_API_VERSION_1_3) {
		impl->report.status = VulkanProbeStatus::LoaderTooOld;
		impl->report.loader_error = "loader api version older than Vulkan 1.3";
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CleanupSurfaceSession(*impl);
		return false;
	}

	Vector<VulkanDiscoveredDevice> discovered;
	if(!impl->ctx.EnumeratePhysicalDevices(discovered, impl->report, impl->error)) {
		impl->report.status = VulkanProbeStatus::PhysicalDeviceEnumerationFailed;
		impl->report.physical_device_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CleanupSurfaceSession(*impl);
		return false;
	}
	if(discovered.IsEmpty()) {
		impl->report.status = VulkanProbeStatus::NoPhysicalDevices;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CleanupSurfaceSession(*impl);
		return false;
	}

	for(auto& found : discovered) {
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
		CleanupSurfaceSession(*impl);
		return false;
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
		CopySurfaceValidationCapture(impl->report, owner->instance.capture);
		CleanupSurfaceSession(*impl);
		return false;
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

	PFN_vkCreateDevice create_device = nullptr;
	if(!ResolveInstanceProc(create_device, owner->dispatch.proc_filter, owner->dispatch.get_instance_proc_addr, owner->instance.instance, "vkCreateDevice", impl->error))
		return fail(impl->error);
	VkResult vr = create_device(choice.device->handle, &dci, nullptr, &impl->device.device);
	if(vr != VK_SUCCESS) {
		impl->error = String("vkCreateDevice failed: ") + AsString((int)vr);
		impl->report.status = VulkanProbeStatus::DeviceCreationFailed;
		impl->report.device_error = impl->error;
		impl->report.status_text = StatusText(impl->report.status);
		impl->report.preflight.status = impl->report.status;
		impl->report.preflight.status_text = impl->report.status_text;
		CopySurfaceValidationCapture(impl->report, owner->instance.capture);
		CleanupSurfaceSession(*impl);
		return false;
	}
	impl->device.RegisterDiagnostics();

	if(!ResolveDeviceProc(impl->device.destroy_device, owner->dispatch.proc_filter, owner->instance.get_device_proc_addr, impl->device.device, "vkDestroyDevice", impl->error)) return fail_device_setup(impl->error);
	if(!ResolveDeviceProc(impl->device.get_device_queue, owner->dispatch.proc_filter, owner->instance.get_device_proc_addr, impl->device.device, "vkGetDeviceQueue", impl->error)) return fail_device_setup(impl->error);
	if(!ResolveDeviceProc(impl->device.device_wait_idle, owner->dispatch.proc_filter, owner->instance.get_device_proc_addr, impl->device.device, "vkDeviceWaitIdle", impl->error)) return fail_device_setup(impl->error);
	impl->device.get_device_queue(impl->device.device, (uint32_t)choice.graphics_family, 0, &impl->device.graphics_queue);
	if(impl->device.graphics_queue == VK_NULL_HANDLE) {
		impl->error = "vkGetDeviceQueue returned VK_NULL_HANDLE";
		impl->report.status = VulkanProbeStatus::DeviceCreationFailed;
		impl->report.device_error = impl->error;
		CopySurfaceValidationCapture(impl->report, owner->instance.capture);
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
		CopySurfaceValidationCapture(impl->report, owner->instance.capture);
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
	if(request_validation)
		InjectValidationIfRequested(owner->instance.capture, VulkanValidationTestPoint::AfterDeviceCreation);
	if(request_validation)
		InjectValidationIfRequested(owner->instance.capture, VulkanValidationTestPoint::DuringDeviceCleanup);

	CopySurfaceValidationCapture(impl->report, owner->instance.capture);
	impl->report.preflight.debug_utils_available = owner->instance.debug_utils_available;
	impl->report.debug_utils_available = owner->instance.debug_utils_available;
	if(impl->report.status == VulkanProbeStatus::Ok && impl->report.validation_error_count > 0) {
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
	CleanupSurfaceSession(*impl);
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
	out << "Shared instance acquired: " << BoolText(report.shared_instance_acquired) << '\n';
	out << "Shared instance reused: " << BoolText(report.shared_instance_reused) << '\n';
	out << "Shared instance released: " << BoolText(report.shared_instance_released) << '\n';
	out << "Shared instance cleanup ok: " << BoolText(report.shared_instance_cleanup_ok) << '\n';
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
