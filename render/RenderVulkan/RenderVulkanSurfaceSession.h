#pragma once

#include <RenderVulkan/RenderVulkan.h>
#include <memory>

namespace Upp {

namespace VulkanTestHooks {
struct VulkanGroupedSurfaceSessionTestResult;
struct VulkanFrameTestResult;
bool TestVulkanGroupedSurfaceSessions(VulkanProcResolver resolver, VulkanGroupedSurfaceSessionTestResult& result);
bool TestVulkanFramePresentation(VulkanProcResolver resolver, VulkanFrameTestResult& result);
}

struct VulkanFrameReport {
	bool sync_requested = false;
	bool sync_created = false;
	bool state_cleared = true;
	bool cleanup_ok = true;
	bool image_acquired = false;
	bool frame_submitted = false;
	bool present_requested = false;
	bool presented = false;
	bool suboptimal = false;
	bool out_of_date = false;
	int image_index = -1;
	uint64_t acquire_count = 0;
	uint64_t present_count = 0;
	uint64_t swapchain_id = 0;
	String error;
};

class VulkanSurfaceSessionGroup {
public:
	struct Impl;

	VulkanSurfaceSessionGroup();
	~VulkanSurfaceSessionGroup();
	VulkanSurfaceSessionGroup(const VulkanSurfaceSessionGroup&) = delete;
	VulkanSurfaceSessionGroup& operator=(const VulkanSurfaceSessionGroup&) = delete;
	VulkanSurfaceSessionGroup(VulkanSurfaceSessionGroup&&) = delete;
	VulkanSurfaceSessionGroup& operator=(VulkanSurfaceSessionGroup&&) = delete;

	private:
	std::unique_ptr<Impl> impl;
	friend class VulkanSurfaceSession;
	friend bool VulkanTestHooks::TestVulkanGroupedSurfaceSessions(VulkanProcResolver, VulkanTestHooks::VulkanGroupedSurfaceSessionTestResult&);
};

class VulkanSurfaceSession {
public:
	struct Impl;

	VulkanSurfaceSession();
	explicit VulkanSurfaceSession(VulkanSurfaceSessionGroup& group);
	~VulkanSurfaceSession();

	bool Open(bool request_validation, const GpuNativeWindowDesc& native_window, VulkanProcResolver resolver = nullptr);
	void Close();

	bool IsOpen() const;
	bool IsReady() const;
	const VulkanSurfaceReport& GetReport() const;
	const String& GetError() const;
	bool CreateSwapchain(Size requested_size);
	bool DestroySwapchain();
	bool HasSwapchain() const;

	bool AcquireFrame();
	bool PresentFrame();
	bool HasAcquiredFrame() const;
	const VulkanFrameReport& GetFrameReport() const;

private:
	struct FrameInterop {
		VkDevice device = VK_NULL_HANDLE;
		VkQueue graphics_queue = VK_NULL_HANDLE;
		VkQueue present_queue = VK_NULL_HANDLE;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		Vector<VkImage> images;
		uint32_t graphics_queue_family_index = 0;
		uint64_t swapchain_id = 0;
		PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
		VulkanProcResolver proc_filter = nullptr;
	};

	std::unique_ptr<Impl> impl;
	void *frame_impl = nullptr;
	VulkanFrameReport frame_report;

	bool GetFrameInterop(FrameInterop& out) const;
	bool WaitFrameIdle(String& error);
	void SyncFrameValidation();
	bool DestroyFrameState();

	friend bool VulkanTestHooks::TestVulkanFramePresentation(VulkanProcResolver, VulkanTestHooks::VulkanFrameTestResult&);
};

}
