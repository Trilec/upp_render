param()

$path = 'E:\apps\github\upp_render\render\RenderVulkan\RenderVulkan.cpp'
$text = [System.IO.File]::ReadAllText($path)
$pattern = '(?ms)^bool VulkanSurfaceSession::Open\(bool request_validation, const GpuNativeWindowDesc& native_window, VulkanProcResolver resolver\).*?^VulkanSurfaceReport VulkanSurfaceProbe::Run'
$replacement = @'
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

VulkanSurfaceReport VulkanSurfaceProbe::Run
'@
$new = [System.Text.RegularExpressions.Regex]::Replace($text, $pattern, $replacement)
[System.IO.File]::WriteAllText($path, $new)
