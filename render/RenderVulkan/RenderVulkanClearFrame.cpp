#include "RenderVulkanSurfaceSession.h"

namespace Upp {

namespace {

template <class T, class Interop>
static bool ResolveClearProc(T& out, const Interop& interop, const char *name, String& error)
{
	if(interop.proc_filter && !interop.proc_filter(nullptr, name)) {
		error = name;
		return false;
	}
	out = reinterpret_cast<T>(interop.get_device_proc_addr(interop.device, name));
	if(!out) {
		error = name;
		return false;
	}
	return true;
}

struct VulkanClearResources {
	PFN_vkDestroyImageView destroy_image_view = nullptr;
	PFN_vkDestroySemaphore destroy_semaphore = nullptr;
	PFN_vkDestroyFence destroy_fence = nullptr;
	PFN_vkDestroyCommandPool destroy_command_pool = nullptr;
	Vector<VkImageView> image_views;
	VkSemaphore render_finished = VK_NULL_HANDLE;
	VkFence submitted = VK_NULL_HANDLE;
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;

	void Destroy(VkDevice device)
	{
		if(command_pool && destroy_command_pool)
			destroy_command_pool(device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
		command_buffer = VK_NULL_HANDLE;
		if(submitted && destroy_fence)
			destroy_fence(device, submitted, nullptr);
		submitted = VK_NULL_HANDLE;
		if(render_finished && destroy_semaphore)
			destroy_semaphore(device, render_finished, nullptr);
		render_finished = VK_NULL_HANDLE;
		if(destroy_image_view)
			for(VkImageView view : image_views)
				if(view)
					destroy_image_view(device, view, nullptr);
		image_views.Clear();
	}
};

} // namespace

bool VulkanSurfaceSession::PresentClearFrame(float red, float green, float blue, float alpha)
{
	FrameInterop interop;
	if(!GetFrameInterop(interop) || interop.device == VK_NULL_HANDLE || interop.swapchain == VK_NULL_HANDLE || interop.images.IsEmpty()) {
		frame_report.error = "Vulkan swapchain is not ready for clear presentation";
		return false;
	}

	PFN_vkCreateImageView create_image_view = nullptr;
	PFN_vkDestroyImageView destroy_image_view = nullptr;
	PFN_vkCreateSemaphore create_semaphore = nullptr;
	PFN_vkDestroySemaphore destroy_semaphore = nullptr;
	PFN_vkCreateFence create_fence = nullptr;
	PFN_vkDestroyFence destroy_fence = nullptr;
	PFN_vkWaitForFences wait_for_fences = nullptr;
	PFN_vkCreateCommandPool create_command_pool = nullptr;
	PFN_vkDestroyCommandPool destroy_command_pool = nullptr;
	PFN_vkAllocateCommandBuffers allocate_command_buffers = nullptr;
	PFN_vkBeginCommandBuffer begin_command_buffer = nullptr;
	PFN_vkEndCommandBuffer end_command_buffer = nullptr;
	PFN_vkCmdPipelineBarrier2 cmd_pipeline_barrier_2 = nullptr;
	PFN_vkCmdBeginRendering cmd_begin_rendering = nullptr;
	PFN_vkCmdEndRendering cmd_end_rendering = nullptr;
	PFN_vkQueueSubmit2 queue_submit_2 = nullptr;
	PFN_vkQueuePresentKHR queue_present = nullptr;
	String error;
	if(!ResolveClearProc(create_image_view, interop, "vkCreateImageView", error) ||
	   !ResolveClearProc(destroy_image_view, interop, "vkDestroyImageView", error) ||
	   !ResolveClearProc(create_semaphore, interop, "vkCreateSemaphore", error) ||
	   !ResolveClearProc(destroy_semaphore, interop, "vkDestroySemaphore", error) ||
	   !ResolveClearProc(create_fence, interop, "vkCreateFence", error) ||
	   !ResolveClearProc(destroy_fence, interop, "vkDestroyFence", error) ||
	   !ResolveClearProc(wait_for_fences, interop, "vkWaitForFences", error) ||
	   !ResolveClearProc(create_command_pool, interop, "vkCreateCommandPool", error) ||
	   !ResolveClearProc(destroy_command_pool, interop, "vkDestroyCommandPool", error) ||
	   !ResolveClearProc(allocate_command_buffers, interop, "vkAllocateCommandBuffers", error) ||
	   !ResolveClearProc(begin_command_buffer, interop, "vkBeginCommandBuffer", error) ||
	   !ResolveClearProc(end_command_buffer, interop, "vkEndCommandBuffer", error) ||
	   !ResolveClearProc(cmd_pipeline_barrier_2, interop, "vkCmdPipelineBarrier2", error) ||
	   !ResolveClearProc(cmd_begin_rendering, interop, "vkCmdBeginRendering", error) ||
	   !ResolveClearProc(cmd_end_rendering, interop, "vkCmdEndRendering", error) ||
	   !ResolveClearProc(queue_submit_2, interop, "vkQueueSubmit2", error) ||
	   !ResolveClearProc(queue_present, interop, "vkQueuePresentKHR", error)) {
		frame_report.error = error;
		return false;
	}

	VulkanClearResources resources;
	resources.destroy_image_view = destroy_image_view;
	resources.destroy_semaphore = destroy_semaphore;
	resources.destroy_fence = destroy_fence;
	resources.destroy_command_pool = destroy_command_pool;
	resources.image_views.SetCount(interop.images.GetCount(), VK_NULL_HANDLE);

	VkResult vr = VK_SUCCESS;
	for(int i = 0; i < interop.images.GetCount() && vr == VK_SUCCESS; ++i) {
		VkImageViewCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = interop.images[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = GetReport().swapchain_format;
		view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		vr = create_image_view(interop.device, &view_info, nullptr, &resources.image_views[i]);
	}

	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if(vr == VK_SUCCESS)
		vr = create_semaphore(interop.device, &semaphore_info, nullptr, &resources.render_finished);

	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if(vr == VK_SUCCESS)
		vr = create_fence(interop.device, &fence_info, nullptr, &resources.submitted);

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	pool_info.queueFamilyIndex = interop.graphics_queue_family_index;
	if(vr == VK_SUCCESS)
		vr = create_command_pool(interop.device, &pool_info, nullptr, &resources.command_pool);

	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = resources.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	if(vr == VK_SUCCESS)
		vr = allocate_command_buffers(interop.device, &alloc_info, &resources.command_buffer);

	if(vr != VK_SUCCESS) {
		resources.Destroy(interop.device);
		frame_report.error = String("Vulkan clear-frame resource creation failed: ") + AsString((int)vr);
		return false;
	}

	if(!AcquireFrame()) {
		resources.Destroy(interop.device);
		return false;
	}

	int image_index = frame_report.image_index;
	if(image_index < 0 || image_index >= resources.image_views.GetCount()) {
		String primary_error = "Vulkan clear frame has an invalid acquired image index";
		resources.Destroy(interop.device);
		DestroySwapchain();
		frame_report.error = primary_error;
		return false;
	}

	if(clear_swapchain_id != interop.swapchain_id || clear_image_initialized.GetCount() != interop.images.GetCount()) {
		clear_swapchain_id = interop.swapchain_id;
		clear_image_initialized.SetCount(interop.images.GetCount(), 0);
	}

	frame_report.clear_requested = true;
	frame_report.cleared = false;
	frame_report.frame_submitted = false;
	frame_report.present_requested = false;
	frame_report.presented = false;
	frame_report.clear_red = red;
	frame_report.clear_green = green;
	frame_report.clear_blue = blue;
	frame_report.clear_alpha = alpha;
	frame_report.error.Clear();

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vr = begin_command_buffer(resources.command_buffer, &begin_info);
	if(vr == VK_SUCCESS) {
		VkImageMemoryBarrier2 to_color{};
		to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		to_color.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		to_color.srcAccessMask = 0;
		to_color.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		to_color.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		to_color.oldLayout = clear_image_initialized[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
		to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_color.image = interop.images[image_index];
		to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_color.subresourceRange.baseMipLevel = 0;
		to_color.subresourceRange.levelCount = 1;
		to_color.subresourceRange.baseArrayLayer = 0;
		to_color.subresourceRange.layerCount = 1;
		VkDependencyInfo to_color_dependency{};
		to_color_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		to_color_dependency.imageMemoryBarrierCount = 1;
		to_color_dependency.pImageMemoryBarriers = &to_color;
		cmd_pipeline_barrier_2(resources.command_buffer, &to_color_dependency);

		VkClearValue clear_value{};
		clear_value.color.float32[0] = red;
		clear_value.color.float32[1] = green;
		clear_value.color.float32[2] = blue;
		clear_value.color.float32[3] = alpha;
		VkRenderingAttachmentInfo attachment{};
		attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachment.imageView = resources.image_views[image_index];
		attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.clearValue = clear_value;
		VkRenderingInfo rendering{};
		rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		rendering.renderArea.offset.x = 0;
		rendering.renderArea.offset.y = 0;
		rendering.renderArea.extent.width = (uint32_t)GetReport().swapchain_extent.cx;
		rendering.renderArea.extent.height = (uint32_t)GetReport().swapchain_extent.cy;
		rendering.layerCount = 1;
		rendering.colorAttachmentCount = 1;
		rendering.pColorAttachments = &attachment;
		cmd_begin_rendering(resources.command_buffer, &rendering);
		cmd_end_rendering(resources.command_buffer);

		VkImageMemoryBarrier2 to_present{};
		to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		to_present.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		to_present.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		to_present.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		to_present.dstAccessMask = 0;
		to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_present.image = interop.images[image_index];
		to_present.subresourceRange = to_color.subresourceRange;
		VkDependencyInfo to_present_dependency{};
		to_present_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		to_present_dependency.imageMemoryBarrierCount = 1;
		to_present_dependency.pImageMemoryBarriers = &to_present;
		cmd_pipeline_barrier_2(resources.command_buffer, &to_present_dependency);
		vr = end_command_buffer(resources.command_buffer);
	}

	if(vr != VK_SUCCESS) {
		String primary_error = String("Vulkan clear-frame command recording failed: ") + AsString((int)vr);
		resources.Destroy(interop.device);
		DestroySwapchain();
		frame_report.error = primary_error;
		return false;
	}

	VkCommandBufferSubmitInfo command_info{};
	command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	command_info.commandBuffer = resources.command_buffer;
	VkSemaphoreSubmitInfo signal_info{};
	signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal_info.semaphore = resources.render_finished;
	signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &command_info;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &signal_info;
	vr = queue_submit_2(interop.graphics_queue, 1, &submit_info, resources.submitted);
	if(vr != VK_SUCCESS) {
		String primary_error = String("vkQueueSubmit2 clear failed: ") + AsString((int)vr);
		String idle_error;
		WaitFrameIdle(idle_error);
		resources.Destroy(interop.device);
		DestroySwapchain();
		frame_report.error = primary_error;
		return false;
	}
	frame_report.frame_submitted = true;
	clear_image_initialized[image_index] = 1;

	frame_report.present_requested = true;
	VkSwapchainKHR swapchain = interop.swapchain;
	uint32_t present_index = (uint32_t)image_index;
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &resources.render_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &present_index;
	vr = queue_present(interop.present_queue, &present_info);

	String idle_error;
	bool idle_ok = WaitFrameIdle(idle_error);
	VkResult submitted_wait = wait_for_fences(interop.device, 1, &resources.submitted, VK_TRUE, UINT64_MAX);
	resources.Destroy(interop.device);
	bool frame_cleanup_ok = DestroyFrameState();
	frame_report.image_acquired = false;
	frame_report.image_index = -1;
	frame_report.cleared = idle_ok && submitted_wait == VK_SUCCESS;
	if(frame_report.cleared)
		frame_report.clear_count++;

	if(!idle_ok || submitted_wait != VK_SUCCESS || !frame_cleanup_ok) {
		if(!idle_ok)
			frame_report.error = idle_error;
		else if(submitted_wait != VK_SUCCESS)
			frame_report.error = String("vkWaitForFences after clear failed: ") + AsString((int)submitted_wait);
		SyncFrameValidation();
		return false;
	}

	if(vr == VK_SUCCESS || vr == VK_SUBOPTIMAL_KHR) {
		frame_report.presented = true;
		frame_report.present_count++;
		frame_report.suboptimal = frame_report.suboptimal || vr == VK_SUBOPTIMAL_KHR;
		frame_report.error.Clear();
		SyncFrameValidation();
		return true;
	}
	if(vr == VK_ERROR_OUT_OF_DATE_KHR) {
		frame_report.out_of_date = true;
		frame_report.error = "Vulkan swapchain is out of date";
		SyncFrameValidation();
		return false;
	}
	frame_report.error = String("vkQueuePresentKHR clear failed: ") + AsString((int)vr);
	SyncFrameValidation();
	return false;
}

}
