#pragma once

#include "RenderVulkan.h"

#if defined(PLATFORM_WIN32)
#include <vulkan/vulkan_win32.h>
#elif defined(PLATFORM_OSX)
#include <vulkan/vulkan_metal.h>
#include <dlfcn.h>
#else
#error Vulkan native presentation currently requires Win32 or macOS.
#endif

namespace Upp::VulkanPlatform {

#ifdef PLATFORM_WIN32
using Module = HMODULE;
using CreateSurfaceProc = PFN_vkCreateWin32SurfaceKHR;
constexpr auto SurfaceExtension = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
constexpr auto CreateSurfaceName = "vkCreateWin32SurfaceKHR";
constexpr auto LoaderError = "LoadLibraryW(vulkan-1.dll) failed";

inline Module Load() { return LoadLibraryW(L"vulkan-1.dll"); }
inline bool Unload(Module module) { return FreeLibrary(module) != 0; }
inline FARPROC Symbol(Module module, const char *name) { return GetProcAddress(module, name); }

inline bool IsNativeWindow(const GpuNativeWindowDesc& desc)
{
	return desc.kind == GpuNativeWindowKind::Win32 && IsWindow(reinterpret_cast<HWND>(desc.handle));
}

inline VkResult CreateSurface(CreateSurfaceProc create, VkInstance instance,
                              const GpuNativeWindowDesc& desc, VkSurfaceKHR& surface)
{
	VkWin32SurfaceCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hwnd = reinterpret_cast<HWND>(desc.handle);
	info.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(info.hwnd, GWLP_HINSTANCE));
	return create(instance, &info, nullptr, &surface);
}
#else
using Module = void *;
using CreateSurfaceProc = PFN_vkCreateMetalSurfaceEXT;
constexpr auto SurfaceExtension = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
constexpr auto CreateSurfaceName = "vkCreateMetalSurfaceEXT";
constexpr auto LoaderError = "could not load libvulkan.1.dylib";

inline Module Load()
{
	if(Module bundled = dlopen("@executable_path/../Frameworks/libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL))
		return bundled;
	return dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
}
inline bool Unload(Module module) { return dlclose(module) == 0; }
inline void *Symbol(Module module, const char *name) { return dlsym(module, name); }

inline bool IsNativeWindow(const GpuNativeWindowDesc& desc)
{
	return desc.kind == GpuNativeWindowKind::MetalLayer && desc.handle != 0;
}

inline VkResult CreateSurface(CreateSurfaceProc create, VkInstance instance,
                              const GpuNativeWindowDesc& desc, VkSurfaceKHR& surface)
{
	VkMetalSurfaceCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	info.pLayer = reinterpret_cast<const CAMetalLayer *>(desc.handle);
	return create(instance, &info, nullptr, &surface);
}
#endif

}
