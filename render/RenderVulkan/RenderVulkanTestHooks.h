#pragma once

#include <RenderVulkan/RenderVulkan.h>

namespace VulkanTestHooks {

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

}
