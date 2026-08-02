#pragma once

#include <RenderVulkan/RenderVulkan.h>
#include <memory>

namespace Upp {

namespace VulkanTestHooks {
struct VulkanGroupedSurfaceSessionTestResult;
bool TestVulkanGroupedSurfaceSessions(VulkanProcResolver resolver, VulkanGroupedSurfaceSessionTestResult& result);
}

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

	private:
	std::unique_ptr<Impl> impl;
};

}
