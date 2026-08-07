#include "RenderVulkanSurfaceSession.h"
#include "RenderVulkanTestHooks.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Upp {

namespace {

struct VulkanFrameState {
	PFN_vkCreateSemaphore create_semaphore = nullptr;
	PFN_vkDestroySemaphore destroy_semaphore = nullptr;
	PFN_vkCreateFence create_fence = nullptr;
	PFN_vkDestroyFence destroy_fence = nullptr;
	PFN_vkWaitForFences wait_for_fences = nullptr;
	PFN_vkResetFences reset_fences = nullptr;
	PFN_vkCreateCommandPool create_command_pool = nullptr;
	PFN_vkDestroyCommandPool destroy_command_pool = nullptr;
	PFN_vkAllocateCommandBuffers allocate_command_buffers = nullptr;
	PFN_vkResetCommandBuffer reset_command_buffer = nullptr;
	PFN_vkBeginCommandBuffer begin_command_buffer = nullptr;
	PFN_vkEndCommandBuffer end_command_buffer = nullptr;
	PFN_vkCmdPipelineBarrier2 cmd_pipeline_barrier_2 = nullptr;
	PFN_vkQueueSubmit2 queue_submit_2 = nullptr;
	PFN_vkAcquireNextImageKHR acquire_next_image = nullptr;
	PFN_vkQueuePresentKHR queue_present = nullptr;
	Vector<VkSemaphore> render_finished;
	VkFence in_flight = VK_NULL_HANDLE;
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	Vector<uint8_t> image_initialized;
	uint64_t swapchain_id = 0;
	bool acquired = false;
	uint32_t image_index = UINT32_MAX;
};

struct VulkanFrameTestInjection {
	bool acquire_out_of_date = false;
	bool acquire_suboptimal = false;
	bool submit_failure = false;
	bool present_out_of_date = false;
	bool present_suboptimal = false;
};

static VulkanFrameTestInjection g_frame_test_injection;
static VulkanProcResolver g_frame_test_base_resolver = nullptr;
static const char *g_frame_test_missing_proc = nullptr;

static FARPROC WINAPI FrameTestResolver(HMODULE module, LPCSTR name)
{
	if(g_frame_test_missing_proc && String(name) == g_frame_test_missing_proc)
		return nullptr;
	return g_frame_test_base_resolver ? g_frame_test_base_resolver(module, name) : nullptr;
}

template <class T, class Interop>
static bool ResolveFrameProc(T& out, const Interop& interop, const char *name, String& error)
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

static bool IsFrameStateCleared(const VulkanFrameState& state)
{
	if(state.in_flight || state.command_pool || state.command_buffer)
		return false;
	for(VkSemaphore semaphore : state.render_finished)
		if(semaphore)
			return false;
	return true;
}

static void DestroyUnpublishedFrameState(VkDevice device, VulkanFrameState& state)
{
	if(state.in_flight && state.destroy_fence)
		state.destroy_fence(device, state.in_flight, nullptr);
	state.in_flight = VK_NULL_HANDLE;
	if(state.destroy_semaphore)
		for(VkSemaphore semaphore : state.render_finished)
			if(semaphore)
				state.destroy_semaphore(device, semaphore, nullptr);
	state.render_finished.Clear();
	if(state.command_pool && state.destroy_command_pool)
		state.destroy_command_pool(device, state.command_pool, nullptr);
	state.command_pool = VK_NULL_HANDLE;
	state.command_buffer = VK_NULL_HANDLE;
	state.image_initialized.Clear();
}

static HWND CreateHiddenFrameTestWindow()
{
	return CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

} // namespace

bool VulkanSurfaceSession::HasAcquiredFrame() const
{
	const VulkanFrameState *state = reinterpret_cast<const VulkanFrameState *>(frame_impl);
	return state && state->acquired;
}

const VulkanFrameReport& VulkanSurfaceSession::GetFrameReport() const
{
	return frame_report;
}

bool VulkanSurfaceSession::DestroyFrameState()
{
	VulkanFrameState *state = reinterpret_cast<VulkanFrameState *>(frame_impl);
	if(!state) {
		frame_report.sync_created = false;
		frame_report.state_cleared = true;
		frame_report.image_acquired = false;
		frame_report.image_index = -1;
		return frame_report.cleanup_ok;
	}

	String idle_error;
	bool idle_ok = WaitFrameIdle(idle_error);
	FrameInterop interop;
	bool have_device = GetFrameInterop(interop) && interop.device != VK_NULL_HANDLE;
	bool ok = idle_ok && have_device;
	VkDevice device = have_device ? interop.device : VK_NULL_HANDLE;

	if(state->in_flight && state->destroy_fence && device)
		state->destroy_fence(device, state->in_flight, nullptr);
	else if(state->in_flight)
		ok = false;
	state->in_flight = VK_NULL_HANDLE;

	for(VkSemaphore semaphore : state->render_finished) {
		if(semaphore && state->destroy_semaphore && device)
			state->destroy_semaphore(device, semaphore, nullptr);
		else if(semaphore)
			ok = false;
	}
	state->render_finished.Clear();

	if(state->command_pool && state->destroy_command_pool && device)
		state->destroy_command_pool(device, state->command_pool, nullptr);
	else if(state->command_pool)
		ok = false;
	state->command_pool = VK_NULL_HANDLE;
	state->command_buffer = VK_NULL_HANDLE;
	state->image_initialized.Clear();
	state->acquired = false;
	state->image_index = UINT32_MAX;

	ok = ok && IsFrameStateCleared(*state);
	delete state;
	frame_impl = nullptr;
	frame_report.sync_created = false;
	frame_report.state_cleared = true;
	frame_report.image_acquired = false;
	frame_report.image_index = -1;
	frame_report.cleanup_ok = frame_report.cleanup_ok && ok;
	if(!idle_ok && frame_report.error.IsEmpty())
		frame_report.error = idle_error;
	SyncFrameValidation();
	return frame_report.cleanup_ok;
}

bool VulkanSurfaceSession::AcquireFrame()
{
	FrameInterop interop;
	if(!GetFrameInterop(interop) || interop.device == VK_NULL_HANDLE || interop.swapchain == VK_NULL_HANDLE || interop.images.IsEmpty()) {
		frame_report.error = "Vulkan swapchain is not ready for frame acquisition";
		return false;
	}

	if(frame_report.swapchain_id != interop.swapchain_id) {
		if(frame_impl)
			DestroyFrameState();
		frame_report.image_acquired = false;
		frame_report.frame_submitted = false;
		frame_report.present_requested = false;
		frame_report.presented = false;
		frame_report.suboptimal = false;
		frame_report.out_of_date = false;
		frame_report.image_index = -1;
		frame_report.swapchain_id = interop.swapchain_id;
		frame_report.error.Clear();
	}
	if(frame_report.out_of_date) {
		frame_report.error = "Vulkan swapchain recreation is required";
		return false;
	}

	VulkanFrameState *state = reinterpret_cast<VulkanFrameState *>(frame_impl);
	if(state && state->acquired) {
		frame_report.error = "Vulkan frame is already acquired";
		return false;
	}

	frame_report.sync_requested = true;
	if(!state) {
		String error;
		std::unique_ptr<VulkanFrameState> created(new VulkanFrameState);
		created->swapchain_id = interop.swapchain_id;
		if(!ResolveFrameProc(created->create_semaphore, interop, "vkCreateSemaphore", error) ||
		   !ResolveFrameProc(created->destroy_semaphore, interop, "vkDestroySemaphore", error) ||
		   !ResolveFrameProc(created->create_fence, interop, "vkCreateFence", error) ||
		   !ResolveFrameProc(created->destroy_fence, interop, "vkDestroyFence", error) ||
		   !ResolveFrameProc(created->wait_for_fences, interop, "vkWaitForFences", error) ||
		   !ResolveFrameProc(created->reset_fences, interop, "vkResetFences", error) ||
		   !ResolveFrameProc(created->create_command_pool, interop, "vkCreateCommandPool", error) ||
		   !ResolveFrameProc(created->destroy_command_pool, interop, "vkDestroyCommandPool", error) ||
		   !ResolveFrameProc(created->allocate_command_buffers, interop, "vkAllocateCommandBuffers", error) ||
		   !ResolveFrameProc(created->reset_command_buffer, interop, "vkResetCommandBuffer", error) ||
		   !ResolveFrameProc(created->begin_command_buffer, interop, "vkBeginCommandBuffer", error) ||
		   !ResolveFrameProc(created->end_command_buffer, interop, "vkEndCommandBuffer", error) ||
		   !ResolveFrameProc(created->cmd_pipeline_barrier_2, interop, "vkCmdPipelineBarrier2", error) ||
		   !ResolveFrameProc(created->queue_submit_2, interop, "vkQueueSubmit2", error) ||
		   !ResolveFrameProc(created->acquire_next_image, interop, "vkAcquireNextImageKHR", error) ||
		   !ResolveFrameProc(created->queue_present, interop, "vkQueuePresentKHR", error)) {
			frame_report.error = error;
			frame_report.state_cleared = true;
			return false;
		}

		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkResult vr = VK_SUCCESS;
		created->render_finished.SetCount(interop.images.GetCount(), VK_NULL_HANDLE);
		for(int i = 0; i < created->render_finished.GetCount() && vr == VK_SUCCESS; ++i)
			vr = created->create_semaphore(interop.device, &semaphore_info, nullptr, &created->render_finished[i]);

		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if(vr == VK_SUCCESS)
			vr = created->create_fence(interop.device, &fence_info, nullptr, &created->in_flight);

		VkCommandPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = interop.graphics_queue_family_index;
		if(vr == VK_SUCCESS)
			vr = created->create_command_pool(interop.device, &pool_info, nullptr, &created->command_pool);

		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = created->command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		if(vr == VK_SUCCESS)
			vr = created->allocate_command_buffers(interop.device, &alloc_info, &created->command_buffer);

		if(vr != VK_SUCCESS) {
			DestroyUnpublishedFrameState(interop.device, *created);
			frame_report.sync_created = false;
			frame_report.error = String("Vulkan frame synchronization creation failed: ") + AsString((int)vr);
			frame_report.state_cleared = true;
			return false;
		}

		created->image_initialized.SetCount(interop.images.GetCount(), 0);
		state = created.release();
		frame_impl = state;
		frame_report.sync_created = true;
		frame_report.state_cleared = false;
	}

	if(state->swapchain_id != interop.swapchain_id || state->render_finished.GetCount() != interop.images.GetCount()) {
		frame_report.error = "Vulkan frame state does not match the active swapchain";
		DestroyFrameState();
		return false;
	}

	VkResult vr = state->wait_for_fences(interop.device, 1, &state->in_flight, VK_TRUE, UINT64_MAX);
	if(vr != VK_SUCCESS) {
		frame_report.error = String("vkWaitForFences failed: ") + AsString((int)vr);
		DestroyFrameState();
		return false;
	}
	vr = state->reset_fences(interop.device, 1, &state->in_flight);
	if(vr != VK_SUCCESS) {
		frame_report.error = String("vkResetFences before acquire failed: ") + AsString((int)vr);
		DestroyFrameState();
		return false;
	}

	frame_report.frame_submitted = false;
	frame_report.present_requested = false;
	frame_report.presented = false;
	frame_report.suboptimal = false;
	frame_report.image_index = -1;
	frame_report.error.Clear();
	uint32_t image_index = 0;
	if(g_frame_test_injection.acquire_out_of_date)
		vr = VK_ERROR_OUT_OF_DATE_KHR;
	else {
		vr = state->acquire_next_image(interop.device, interop.swapchain, UINT64_MAX, VK_NULL_HANDLE, state->in_flight, &image_index);
		if(g_frame_test_injection.acquire_suboptimal && vr == VK_SUCCESS)
			vr = VK_SUBOPTIMAL_KHR;
	}

	if(vr == VK_ERROR_OUT_OF_DATE_KHR) {
		frame_report.out_of_date = true;
		frame_report.error = "Vulkan swapchain is out of date";
		SyncFrameValidation();
		return false;
	}
	if(vr != VK_SUCCESS && vr != VK_SUBOPTIMAL_KHR) {
		frame_report.error = String("vkAcquireNextImageKHR failed: ") + AsString((int)vr);
		DestroyFrameState();
		return false;
	}
	VkResult acquire_wait = state->wait_for_fences(interop.device, 1, &state->in_flight, VK_TRUE, UINT64_MAX);
	if(acquire_wait != VK_SUCCESS) {
		frame_report.error = String("vkWaitForFences after vkAcquireNextImageKHR failed: ") + AsString((int)acquire_wait);
		DestroyFrameState();
		return false;
	}
	if(image_index >= (uint32_t)interop.images.GetCount()) {
		frame_report.error = "vkAcquireNextImageKHR returned an invalid image index";
		DestroyFrameState();
		return false;
	}

	state->acquired = true;
	state->image_index = image_index;
	frame_report.image_acquired = true;
	frame_report.image_index = (int)image_index;
	frame_report.acquire_count++;
	frame_report.suboptimal = vr == VK_SUBOPTIMAL_KHR;
	SyncFrameValidation();
	return true;
}

bool VulkanSurfaceSession::PresentFrame()
{
	VulkanFrameState *state = reinterpret_cast<VulkanFrameState *>(frame_impl);
	if(!state || !state->acquired) {
		frame_report.error = "No Vulkan frame is acquired";
		return false;
	}

	FrameInterop interop;
	if(!GetFrameInterop(interop) || interop.swapchain == VK_NULL_HANDLE || interop.swapchain_id != state->swapchain_id || state->image_index >= (uint32_t)interop.images.GetCount()) {
		frame_report.error = "Vulkan frame state does not match the active swapchain";
		DestroyFrameState();
		return false;
	}

	VkResult vr = state->reset_command_buffer(state->command_buffer, 0);
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if(vr == VK_SUCCESS)
		vr = state->begin_command_buffer(state->command_buffer, &begin_info);

	if(vr == VK_SUCCESS && !state->image_initialized[state->image_index]) {
		VkImageMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		barrier.srcAccessMask = 0;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.dstAccessMask = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = interop.images[state->image_index];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		VkDependencyInfo dependency{};
		dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency.imageMemoryBarrierCount = 1;
		dependency.pImageMemoryBarriers = &barrier;
		state->cmd_pipeline_barrier_2(state->command_buffer, &dependency);
	}
	if(vr == VK_SUCCESS)
		vr = state->end_command_buffer(state->command_buffer);
	if(vr != VK_SUCCESS) {
		frame_report.error = String("Vulkan frame command recording failed: ") + AsString((int)vr);
		DestroyFrameState();
		return false;
	}

	vr = state->reset_fences(interop.device, 1, &state->in_flight);
	if(vr != VK_SUCCESS) {
		frame_report.error = String("vkResetFences failed: ") + AsString((int)vr);
		DestroyFrameState();
		return false;
	}

	VkCommandBufferSubmitInfo command_info{};
	command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	command_info.commandBuffer = state->command_buffer;
	VkSemaphoreSubmitInfo signal_info{};
	signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal_info.semaphore = state->render_finished[state->image_index];
	signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkSubmitInfo2 submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &command_info;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &signal_info;

	if(g_frame_test_injection.submit_failure)
		vr = VK_ERROR_DEVICE_LOST;
	else
		vr = state->queue_submit_2(interop.graphics_queue, 1, &submit_info, state->in_flight);
	if(vr != VK_SUCCESS) {
		frame_report.error = String("vkQueueSubmit2 failed: ") + AsString((int)vr);
		state->acquired = false;
		frame_report.image_acquired = false;
		DestroyFrameState();
		return false;
	}

	frame_report.frame_submitted = true;
	state->image_initialized[state->image_index] = 1;
	frame_report.present_requested = true;
	VkSwapchainKHR swapchain = interop.swapchain;
	uint32_t image_index = state->image_index;
	VkSemaphore render_finished = state->render_finished[image_index];
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &image_index;

	if(g_frame_test_injection.present_out_of_date)
		vr = VK_ERROR_OUT_OF_DATE_KHR;
	else {
		vr = state->queue_present(interop.present_queue, &present_info);
		if(g_frame_test_injection.present_suboptimal && vr == VK_SUCCESS)
			vr = VK_SUBOPTIMAL_KHR;
	}

	state->acquired = false;
	state->image_index = UINT32_MAX;
	frame_report.image_acquired = false;
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
		DestroyFrameState();
		return false;
	}
	frame_report.error = String("vkQueuePresentKHR failed: ") + AsString((int)vr);
	DestroyFrameState();
	return false;
}

namespace VulkanTestHooks {

bool TestVulkanFramePresentation(VulkanProcResolver resolver, VulkanFrameTestResult& result)
{
	result = VulkanFrameTestResult();
	g_frame_test_base_resolver = resolver;
	g_frame_test_missing_proc = nullptr;
	g_frame_test_injection = VulkanFrameTestInjection();
	struct FrameTestReset {
		~FrameTestReset()
		{
			g_frame_test_base_resolver = nullptr;
			g_frame_test_missing_proc = nullptr;
			g_frame_test_injection = VulkanFrameTestInjection();
		}
	} frame_test_reset;
	ClearVulkanRuntimeDeviceDiagnostics();

	HWND first_hwnd = CreateHiddenFrameTestWindow();
	HWND second_hwnd = CreateHiddenFrameTestWindow();
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

	VulkanSurfaceSession session;
	if(!session.Open(true, first_window, &FrameTestResolver) || !session.IsReady()) return false;
	if(session.AcquireFrame() || session.GetFrameReport().error != "Vulkan swapchain is not ready for frame acquisition") return false;
	result.no_swapchain_refused = true;
	if(!session.CreateSwapchain(Size(64, 64))) return false;
	if(!session.AcquireFrame() || !session.HasAcquiredFrame()) return false;
	int first_index = session.GetFrameReport().image_index;
	if(first_index < 0 || first_index >= session.GetReport().swapchain_image_count) return false;
	if(session.AcquireFrame() || session.GetFrameReport().error != "Vulkan frame is already acquired" || !session.HasAcquiredFrame()) return false;
	result.duplicate_acquire_refused = true;
	if(!session.PresentFrame() || session.HasAcquiredFrame() || !session.GetFrameReport().presented || session.GetFrameReport().present_count != 1) return false;
	result.acquire_present = true;
	int repeats = max(4, session.GetReport().swapchain_image_count * 2);
	for(int i = 0; i < repeats; ++i)
		if(!session.AcquireFrame() || !session.PresentFrame()) return false;
	if(session.GetFrameReport().present_count != (uint64_t)(repeats + 1)) return false;
	result.repeat_present = true;

	const char *missing[] = {
		"vkCreateSemaphore", "vkDestroySemaphore", "vkCreateFence", "vkDestroyFence", "vkWaitForFences", "vkResetFences",
		"vkCreateCommandPool", "vkDestroyCommandPool", "vkAllocateCommandBuffers", "vkResetCommandBuffer", "vkBeginCommandBuffer",
		"vkEndCommandBuffer", "vkCmdPipelineBarrier2", "vkQueueSubmit2", "vkAcquireNextImageKHR", "vkQueuePresentKHR"
	};
	for(const char *name : missing) {
		if(!session.DestroyFrameState()) return false;
		g_frame_test_missing_proc = name;
		if(session.AcquireFrame() || session.GetFrameReport().error != name || !session.GetFrameReport().state_cleared || session.HasAcquiredFrame()) return false;
		g_frame_test_missing_proc = nullptr;
		if(!session.AcquireFrame() || !session.PresentFrame()) return false;
	}
	result.missing_procedure_recovered = true;

	g_frame_test_injection.acquire_suboptimal = true;
	if(!session.AcquireFrame() || !session.GetFrameReport().suboptimal || !session.PresentFrame() || !session.GetFrameReport().presented || !session.GetFrameReport().suboptimal) return false;
	g_frame_test_injection = VulkanFrameTestInjection();
	result.acquire_suboptimal = true;

	if(!session.DestroyFrameState()) return false;
	g_frame_test_injection.acquire_out_of_date = true;
	if(session.AcquireFrame() || !session.GetFrameReport().out_of_date || session.HasAcquiredFrame()) return false;
	g_frame_test_injection = VulkanFrameTestInjection();
	result.acquire_out_of_date = true;
	if(!session.DestroySwapchain() || !session.CreateSwapchain(Size(64, 64))) return false;

	if(!session.AcquireFrame()) return false;
	g_frame_test_injection.present_suboptimal = true;
	if(!session.PresentFrame() || !session.GetFrameReport().suboptimal || !session.GetFrameReport().presented) return false;
	g_frame_test_injection = VulkanFrameTestInjection();
	result.present_suboptimal = true;

	if(!session.AcquireFrame()) return false;
	g_frame_test_injection.present_out_of_date = true;
	if(session.PresentFrame() || !session.GetFrameReport().out_of_date || !session.GetFrameReport().state_cleared || session.HasAcquiredFrame()) return false;
	g_frame_test_injection = VulkanFrameTestInjection();
	result.present_out_of_date = true;
	if(!session.DestroySwapchain() || !session.CreateSwapchain(Size(64, 64))) return false;

	if(!session.AcquireFrame()) return false;
	g_frame_test_injection.submit_failure = true;
	if(session.PresentFrame() || !session.GetFrameReport().state_cleared || session.HasAcquiredFrame() || session.GetFrameReport().error.Find("vkQueueSubmit2 failed") < 0) return false;
	g_frame_test_injection = VulkanFrameTestInjection();
	if(!session.AcquireFrame() || !session.PresentFrame()) return false;
	result.submit_failure_recovered = true;

	if(!session.AcquireFrame() || !session.HasAcquiredFrame()) return false;
	if(!session.DestroySwapchain() || session.HasSwapchain() || session.HasAcquiredFrame() || !session.GetFrameReport().state_cleared) return false;
	result.destroy_with_acquired_cleanup = true;
	if(!session.CreateSwapchain(Size(64, 64))) return false;
	if(session.GetReport().validation_warning_count != 0 || session.GetReport().validation_error_count != 0) return false;
	result.validation_clean = true;
	session.Close();
	if(session.GetReport().validation_warning_count != 0 || session.GetReport().validation_error_count != 0 || !session.GetFrameReport().state_cleared || !session.GetFrameReport().cleanup_ok || !session.GetReport().clean_shutdown) return false;

	{
		VulkanSurfaceSession close_session;
		if(!close_session.Open(true, first_window, &FrameTestResolver) || !close_session.CreateSwapchain(Size(64, 64)) || !close_session.AcquireFrame()) return false;
		close_session.Close();
		if(close_session.IsOpen() || close_session.IsReady() || close_session.HasSwapchain() || close_session.HasAcquiredFrame() || !close_session.GetFrameReport().state_cleared || !close_session.GetFrameReport().cleanup_ok || !close_session.GetReport().clean_shutdown || close_session.GetReport().validation_warning_count != 0 || close_session.GetReport().validation_error_count != 0) return false;
	}
	result.close_with_acquired_cleanup = true;

	{
		VulkanSurfaceSession destructor_session;
		if(!destructor_session.Open(true, first_window, &FrameTestResolver) || !destructor_session.CreateSwapchain(Size(64, 64)) || !destructor_session.AcquireFrame()) return false;
	}
	VulkanRuntimeDeviceDiagnostics destructor_diag = GetVulkanRuntimeDeviceDiagnostics();
	if(destructor_diag.runtime_live_count != 0 || destructor_diag.instance_live_count != 0 || destructor_diag.surface_live_count != 0 || destructor_diag.device_live_count != 0 || destructor_diag.swapchain_live_count != 0) return false;
	result.destructor_cleanup = true;

	ClearVulkanRuntimeDeviceDiagnostics();
	{
		VulkanSurfaceSessionGroup group;
		VulkanSurfaceSession first(group), second(group);
		if(!first.Open(true, first_window, &FrameTestResolver) || !second.Open(true, second_window, &FrameTestResolver)) return false;
		if(!first.CreateSwapchain(Size(64, 64)) || !second.CreateSwapchain(Size(64, 64))) return false;
		if(!first.AcquireFrame() || !second.AcquireFrame()) return false;
		int second_index = second.GetFrameReport().image_index;
		if(!first.PresentFrame() || !second.HasAcquiredFrame() || second.GetFrameReport().image_index != second_index) return false;
		if(!second.PresentFrame()) return false;
		first.Close();
		if(!second.IsReady() || !second.HasSwapchain()) return false;
		if(!second.AcquireFrame() || !second.PresentFrame()) return false;
		if(first.GetReport().validation_warning_count != 0 || first.GetReport().validation_error_count != 0 || second.GetReport().validation_warning_count != 0 || second.GetReport().validation_error_count != 0) return false;
		second.Close();
		if(second.GetReport().validation_warning_count != 0 || second.GetReport().validation_error_count != 0 || !second.GetFrameReport().state_cleared || !second.GetFrameReport().cleanup_ok || !second.GetReport().clean_shutdown) return false;
	}
	result.grouped_isolation = true;
	result.final_diag = GetVulkanRuntimeDeviceDiagnostics();
	g_frame_test_base_resolver = nullptr;
	g_frame_test_missing_proc = nullptr;
	g_frame_test_injection = VulkanFrameTestInjection();
	return result.no_swapchain_refused && result.acquire_present && result.repeat_present && result.duplicate_acquire_refused && result.missing_procedure_recovered && result.acquire_suboptimal && result.acquire_out_of_date && result.present_suboptimal && result.present_out_of_date && result.submit_failure_recovered && result.destroy_with_acquired_cleanup && result.close_with_acquired_cleanup && result.destructor_cleanup && result.grouped_isolation && result.validation_clean && result.final_diag.runtime_live_count == 0 && result.final_diag.instance_live_count == 0 && result.final_diag.debug_messenger_live_count == 0 && result.final_diag.surface_live_count == 0 && result.final_diag.device_live_count == 0 && result.final_diag.swapchain_live_count == 0;
}

} // namespace VulkanTestHooks

}
