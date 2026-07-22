#pragma once

#include <RenderVulkan/RenderVulkan.h>
#include <memory>

namespace Upp {

class VulkanSurfaceSession {
public:
	struct Impl;

	VulkanSurfaceSession();
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
