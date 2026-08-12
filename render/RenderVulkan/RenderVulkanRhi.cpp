#include "RenderVulkanRhi.h"

#include <cstring>
#include <limits>

namespace Upp {

namespace {

static VkFormat ToVkFormat(GpuFormat format)
{
	switch(format) {
	case GpuFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
	case GpuFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
	case GpuFormat::R16F: return VK_FORMAT_R16_SFLOAT;
	case GpuFormat::D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
	case GpuFormat::Unknown: return VK_FORMAT_UNDEFINED;
	}
	return VK_FORMAT_UNDEFINED;
}

static int BytesPerPixel(GpuFormat format)
{
	switch(format) {
	case GpuFormat::RGBA8:
	case GpuFormat::BGRA8:
	case GpuFormat::D24S8:
		return 4;
	case GpuFormat::R16F:
		return 2;
	case GpuFormat::Unknown:
		return 0;
	}
	return 0;
}

static VkBufferUsageFlags ToVkBufferUsage(int usage)
{
	VkBufferUsageFlags out = 0;
	if(usage & GpuBufferUsage_Vertex) out |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if(usage & GpuBufferUsage_Index) out |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if(usage & GpuBufferUsage_Uniform) out |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if(usage & GpuBufferUsage_Storage) out |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if(usage & GpuBufferUsage_TransferSrc) out |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if(usage & GpuBufferUsage_TransferDst) out |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if(out == 0)
		out = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	return out;
}

static VkImageUsageFlags ToVkImageUsage(int usage)
{
	VkImageUsageFlags out = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if(usage & GpuTextureUsage_ColorAttachment) out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if(usage & GpuTextureUsage_Sampled) out |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if(usage & GpuTextureUsage_TransferSrc) out |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if(usage & GpuTextureUsage_TransferDst) out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	return out;
}

static VkImageLayout FinalTextureLayout(int usage)
{
	if(usage & GpuTextureUsage_Sampled)
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if(usage & GpuTextureUsage_ColorAttachment)
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	return VK_IMAGE_LAYOUT_GENERAL;
}

static VkPipelineStageFlags StageForLayout(VkImageLayout layout)
{
	switch(layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_GENERAL: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	default: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}
}

static VkAccessFlags AccessForLayout(VkImageLayout layout)
{
	switch(layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED: return 0;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_GENERAL: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	default: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	}
}

static String VkFailure(const char *operation, VkResult result)
{
	return String(operation) + " failed: " + AsString((int)result);
}

}

struct VulkanGpuDevice::Impl {
	struct RawBuffer : Moveable<RawBuffer> {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize allocation_size = 0;
	};

	struct BufferState : Moveable<BufferState> {
		GpuBufferDesc desc;
		RawBuffer raw;
	};

	struct TextureState : Moveable<TextureState> {
		GpuTextureDesc desc;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	VulkanSurfaceSession *session = nullptr;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphics_queue = VK_NULL_HANDLE;
	uint32_t graphics_queue_family_index = 0;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	VulkanProcResolver proc_filter = nullptr;
	bool ready = false;
	String error;
	GpuDeviceId device_id;
	GpuAdapterInfo adapter_info;
	int next_buffer_id = 1;
	int next_texture_id = 1;
	VectorMap<int, BufferState> buffers;
	VectorMap<int, TextureState> textures;

	PFN_vkCreateBuffer create_buffer = nullptr;
	PFN_vkDestroyBuffer destroy_buffer = nullptr;
	PFN_vkGetBufferMemoryRequirements get_buffer_memory_requirements = nullptr;
	PFN_vkAllocateMemory allocate_memory = nullptr;
	PFN_vkFreeMemory free_memory = nullptr;
	PFN_vkBindBufferMemory bind_buffer_memory = nullptr;
	PFN_vkMapMemory map_memory = nullptr;
	PFN_vkUnmapMemory unmap_memory = nullptr;
	PFN_vkFlushMappedMemoryRanges flush_mapped_memory_ranges = nullptr;
	PFN_vkCreateImage create_image = nullptr;
	PFN_vkDestroyImage destroy_image = nullptr;
	PFN_vkGetImageMemoryRequirements get_image_memory_requirements = nullptr;
	PFN_vkBindImageMemory bind_image_memory = nullptr;
	PFN_vkCreateCommandPool create_command_pool = nullptr;
	PFN_vkDestroyCommandPool destroy_command_pool = nullptr;
	PFN_vkAllocateCommandBuffers allocate_command_buffers = nullptr;
	PFN_vkBeginCommandBuffer begin_command_buffer = nullptr;
	PFN_vkEndCommandBuffer end_command_buffer = nullptr;
	PFN_vkCmdPipelineBarrier cmd_pipeline_barrier = nullptr;
	PFN_vkCmdCopyBufferToImage cmd_copy_buffer_to_image = nullptr;
	PFN_vkQueueSubmit queue_submit = nullptr;
	PFN_vkQueueWaitIdle queue_wait_idle = nullptr;

	template <class T>
	bool Resolve(T& out, const char *name)
	{
		if(proc_filter && !proc_filter(nullptr, name)) {
			error = String("missing Vulkan procedure: ") + name;
			return false;
		}
		out = reinterpret_cast<T>(get_device_proc_addr(device, name));
		if(!out) {
			error = String("missing Vulkan procedure: ") + name;
			return false;
		}
		return true;
	}

	bool ResolveResourceDispatch()
	{
		return Resolve(create_buffer, "vkCreateBuffer") &&
		       Resolve(destroy_buffer, "vkDestroyBuffer") &&
		       Resolve(get_buffer_memory_requirements, "vkGetBufferMemoryRequirements") &&
		       Resolve(allocate_memory, "vkAllocateMemory") &&
		       Resolve(free_memory, "vkFreeMemory") &&
		       Resolve(bind_buffer_memory, "vkBindBufferMemory") &&
		       Resolve(map_memory, "vkMapMemory") &&
		       Resolve(unmap_memory, "vkUnmapMemory") &&
		       Resolve(flush_mapped_memory_ranges, "vkFlushMappedMemoryRanges") &&
		       Resolve(create_image, "vkCreateImage") &&
		       Resolve(destroy_image, "vkDestroyImage") &&
		       Resolve(get_image_memory_requirements, "vkGetImageMemoryRequirements") &&
		       Resolve(bind_image_memory, "vkBindImageMemory") &&
		       Resolve(create_command_pool, "vkCreateCommandPool") &&
		       Resolve(destroy_command_pool, "vkDestroyCommandPool") &&
		       Resolve(allocate_command_buffers, "vkAllocateCommandBuffers") &&
		       Resolve(begin_command_buffer, "vkBeginCommandBuffer") &&
		       Resolve(end_command_buffer, "vkEndCommandBuffer") &&
		       Resolve(cmd_pipeline_barrier, "vkCmdPipelineBarrier") &&
		       Resolve(cmd_copy_buffer_to_image, "vkCmdCopyBufferToImage") &&
		       Resolve(queue_submit, "vkQueueSubmit") &&
		       Resolve(queue_wait_idle, "vkQueueWaitIdle");
	}

	bool CheckReady()
	{
		if(!ready || !session || !session->IsReady()) {
			error = "VulkanGpuDevice requires a live ready VulkanSurfaceSession";
			return false;
		}
		error.Clear();
		return true;
	}

	GpuResult Unsupported(const char *operation)
	{
		error = String(operation) + " is deferred beyond TASK-008A1-S17B";
		return GpuResult::Unsupported;
	}

	bool CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage, RawBuffer& out)
	{
		out = RawBuffer();
		VkBufferCreateInfo bci {};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = usage;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult vr = create_buffer(device, &bci, nullptr, &out.buffer);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkCreateBuffer", vr);
			return false;
		}

		VkMemoryRequirements requirements {};
		get_buffer_memory_requirements(device, out.buffer, &requirements);
		for(uint32_t memory_type = 0; memory_type < 32; ++memory_type) {
			if((requirements.memoryTypeBits & (1u << memory_type)) == 0)
				continue;
			VkMemoryAllocateInfo mai {};
			mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			mai.allocationSize = requirements.size;
			mai.memoryTypeIndex = memory_type;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			vr = allocate_memory(device, &mai, nullptr, &memory);
			if(vr != VK_SUCCESS)
				continue;
			void *mapped = nullptr;
			VkResult map_result = map_memory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
			if(map_result == VK_SUCCESS && mapped) {
				unmap_memory(device, memory);
				vr = bind_buffer_memory(device, out.buffer, memory, 0);
				if(vr == VK_SUCCESS) {
					out.memory = memory;
					out.allocation_size = requirements.size;
					return true;
				}
			}
			free_memory(device, memory, nullptr);
		}
		destroy_buffer(device, out.buffer, nullptr);
		out.buffer = VK_NULL_HANDLE;
		error = "no compatible host-visible Vulkan memory type for buffer";
		return false;
	}

	void DestroyRawBuffer(RawBuffer& raw)
	{
		if(raw.buffer)
			destroy_buffer(device, raw.buffer, nullptr);
		if(raw.memory)
			free_memory(device, raw.memory, nullptr);
		raw = RawBuffer();
	}

	bool WriteRawBuffer(const RawBuffer& raw, VkDeviceSize offset, const void *data, VkDeviceSize size)
	{
		void *mapped = nullptr;
		VkResult vr = map_memory(device, raw.memory, 0, VK_WHOLE_SIZE, 0, &mapped);
		if(vr != VK_SUCCESS || !mapped) {
			error = VkFailure("vkMapMemory", vr);
			return false;
		}
		std::memcpy(static_cast<byte *>(mapped) + offset, data, (size_t)size);
		VkMappedMemoryRange range {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = raw.memory;
		range.offset = 0;
		range.size = VK_WHOLE_SIZE;
		vr = flush_mapped_memory_ranges(device, 1, &range);
		unmap_memory(device, raw.memory);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkFlushMappedMemoryRanges", vr);
			return false;
		}
		return true;
	}

	bool AllocateImageMemory(VkImage image, VkDeviceMemory& out_memory)
	{
		out_memory = VK_NULL_HANDLE;
		VkMemoryRequirements requirements {};
		get_image_memory_requirements(device, image, &requirements);
		for(uint32_t memory_type = 0; memory_type < 32; ++memory_type) {
			if((requirements.memoryTypeBits & (1u << memory_type)) == 0)
				continue;
			VkMemoryAllocateInfo mai {};
			mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			mai.allocationSize = requirements.size;
			mai.memoryTypeIndex = memory_type;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkResult vr = allocate_memory(device, &mai, nullptr, &memory);
			if(vr != VK_SUCCESS)
				continue;
			vr = bind_image_memory(device, image, memory, 0);
			if(vr == VK_SUCCESS) {
				out_memory = memory;
				return true;
			}
			free_memory(device, memory, nullptr);
		}
		error = "no compatible Vulkan memory type for texture";
		return false;
	}

	bool UploadTexture(TextureState& texture, const GpuTextureWriteDesc& desc, const void *data, int64 data_size)
	{
		const int bytes_per_pixel = BytesPerPixel(texture.desc.format);
		if(bytes_per_pixel <= 0 || texture.desc.format == GpuFormat::D24S8) {
			error = "VulkanGpuDevice texture upload format is unsupported in S17B";
			return false;
		}
		const int64 tight_row = (int64)desc.size.cx * bytes_per_pixel;
		if(desc.row_pitch < tight_row) {
			error = "texture row pitch is smaller than the tight row size";
			return false;
		}
		if(desc.size.cy > 1 && desc.row_pitch > (std::numeric_limits<int64>::max() - tight_row) / (desc.size.cy - 1)) {
			error = "texture upload layout overflows int64";
			return false;
		}
		const int64 required_size = desc.row_pitch * (desc.size.cy - 1) + tight_row;
		if(data_size < required_size) {
			error = "texture upload data is smaller than the declared layout";
			return false;
		}
		if(tight_row > std::numeric_limits<int>::max() / desc.size.cy) {
			error = "texture upload staging size is too large";
			return false;
		}
		const int tight_size = (int)(tight_row * desc.size.cy);
		Vector<byte> packed;
		packed.SetCount(tight_size);
		const byte *source = static_cast<const byte *>(data);
		for(int y = 0; y < desc.size.cy; ++y)
			std::memcpy(packed.Begin() + (int)(tight_row * y), source + desc.row_pitch * y, (size_t)tight_row);

		RawBuffer staging;
		if(!CreateRawBuffer((VkDeviceSize)tight_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
			return false;
		if(!WriteRawBuffer(staging, 0, packed.Begin(), (VkDeviceSize)tight_size)) {
			DestroyRawBuffer(staging);
			return false;
		}

		VkCommandPool command_pool = VK_NULL_HANDLE;
		VkCommandBuffer command_buffer = VK_NULL_HANDLE;
		VkCommandPoolCreateInfo cpci {};
		cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cpci.queueFamilyIndex = graphics_queue_family_index;
		VkResult vr = create_command_pool(device, &cpci, nullptr, &command_pool);
		if(vr != VK_SUCCESS) {
			DestroyRawBuffer(staging);
			error = VkFailure("vkCreateCommandPool", vr);
			return false;
		}

		auto cleanup = [&]() {
			if(command_pool)
				destroy_command_pool(device, command_pool, nullptr);
			DestroyRawBuffer(staging);
		};

		VkCommandBufferAllocateInfo cbai {};
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.commandPool = command_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbai.commandBufferCount = 1;
		vr = allocate_command_buffers(device, &cbai, &command_buffer);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkAllocateCommandBuffers", vr);
			cleanup();
			return false;
		}

		VkCommandBufferBeginInfo begin_info {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vr = begin_command_buffer(command_buffer, &begin_info);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkBeginCommandBuffer", vr);
			cleanup();
			return false;
		}

		VkImageMemoryBarrier before {};
		before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		before.srcAccessMask = AccessForLayout(texture.layout);
		before.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		before.oldLayout = texture.layout;
		before.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		before.image = texture.image;
		before.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		before.subresourceRange.baseMipLevel = 0;
		before.subresourceRange.levelCount = 1;
		before.subresourceRange.baseArrayLayer = 0;
		before.subresourceRange.layerCount = 1;
		cmd_pipeline_barrier(command_buffer, StageForLayout(texture.layout), VK_PIPELINE_STAGE_TRANSFER_BIT,
		                     0, 0, nullptr, 0, nullptr, 1, &before);

		VkBufferImageCopy copy {};
		copy.bufferOffset = 0;
		copy.bufferRowLength = 0;
		copy.bufferImageHeight = 0;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset = { desc.origin.x, desc.origin.y, 0 };
		copy.imageExtent = { (uint32_t)desc.size.cx, (uint32_t)desc.size.cy, 1 };
		cmd_copy_buffer_to_image(command_buffer, staging.buffer, texture.image,
		                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

		const VkImageLayout final_layout = FinalTextureLayout(texture.desc.usage);
		VkImageMemoryBarrier after {};
		after.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		after.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		after.dstAccessMask = AccessForLayout(final_layout);
		after.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		after.newLayout = final_layout;
		after.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		after.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		after.image = texture.image;
		after.subresourceRange = before.subresourceRange;
		cmd_pipeline_barrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, StageForLayout(final_layout),
		                     0, 0, nullptr, 0, nullptr, 1, &after);

		vr = end_command_buffer(command_buffer);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkEndCommandBuffer", vr);
			cleanup();
			return false;
		}

		VkSubmitInfo submit {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &command_buffer;
		vr = queue_submit(graphics_queue, 1, &submit, VK_NULL_HANDLE);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkQueueSubmit", vr);
			cleanup();
			return false;
		}
		vr = queue_wait_idle(graphics_queue);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkQueueWaitIdle", vr);
			cleanup();
			return false;
		}
		texture.layout = final_layout;
		cleanup();
		return true;
	}

	void DestroyAllResources()
	{
		if(!ready || !session || !session->IsReady()) {
			buffers.Clear();
			textures.Clear();
			return;
		}
		queue_wait_idle(graphics_queue);
		while(textures.GetCount()) {
			TextureState& state = textures[textures.GetCount() - 1];
			if(state.image)
				destroy_image(device, state.image, nullptr);
			if(state.memory)
				free_memory(device, state.memory, nullptr);
			textures.Remove(textures.GetCount() - 1);
		}
		while(buffers.GetCount()) {
			BufferState& state = buffers[buffers.GetCount() - 1];
			DestroyRawBuffer(state.raw);
			buffers.Remove(buffers.GetCount() - 1);
		}
	}
};

VulkanGpuDevice::VulkanGpuDevice(VulkanSurfaceSession& session)
	: impl(new Impl)
{
	impl->session = &session;
	impl->device_id.value = 1;
	impl->adapter_info.adapter_id.value = 1;
	impl->adapter_info.device_id = impl->device_id;
	impl->adapter_info.backend_kind = GpuBackendKind::Vulkan;
	impl->adapter_info.capability_flags = GpuCapability_Buffers | GpuCapability_Textures;
	impl->adapter_info.name = session.GetReport().selected_device.name;
	if(!session.IsReady()) {
		impl->error = "VulkanGpuDevice requires a ready VulkanSurfaceSession";
		return;
	}
	VulkanSurfaceSession::FrameInterop interop;
	if(!session.GetFrameInterop(interop)) {
		impl->error = "VulkanGpuDevice could not acquire session device interop";
		return;
	}
	impl->device = interop.device;
	impl->graphics_queue = interop.graphics_queue;
	impl->graphics_queue_family_index = interop.graphics_queue_family_index;
	impl->get_device_proc_addr = interop.get_device_proc_addr;
	impl->proc_filter = interop.proc_filter;
	if(!impl->device || !impl->graphics_queue || !impl->get_device_proc_addr) {
		impl->error = "VulkanGpuDevice session interop is incomplete";
		return;
	}
	impl->ready = impl->ResolveResourceDispatch();
}

VulkanGpuDevice::~VulkanGpuDevice()
{
	if(impl)
		impl->DestroyAllResources();
}

bool VulkanGpuDevice::IsReady() const
{
	return impl && impl->ready && impl->session && impl->session->IsReady();
}

const String& VulkanGpuDevice::GetError() const
{
	return impl->error;
}

int VulkanGpuDevice::GetLiveBufferCount() const
{
	return impl ? impl->buffers.GetCount() : 0;
}

int VulkanGpuDevice::GetLiveTextureCount() const
{
	return impl ? impl->textures.GetCount() : 0;
}

GpuDeviceId VulkanGpuDevice::GetDeviceId() const
{
	return impl->device_id;
}

GpuBackendKind VulkanGpuDevice::GetBackendKind() const
{
	return GpuBackendKind::Vulkan;
}

GpuAdapterInfo VulkanGpuDevice::GetAdapterInfo() const
{
	return impl->adapter_info;
}

GpuResult VulkanGpuDevice::CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out)
{
	out = GpuBufferId();
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	if(desc.size <= 0)
		return impl->error = "CreateBuffer requires positive size", GpuResult::InvalidArgument;
	Impl::RawBuffer raw;
	if(!impl->CreateRawBuffer((VkDeviceSize)desc.size, ToVkBufferUsage(desc.usage), raw))
		return GpuResult::InvalidState;
	GpuBufferId id;
	id.value = impl->next_buffer_id++;
	Impl::BufferState& state = impl->buffers.Add(id.value, Impl::BufferState());
	state.desc = desc;
	state.raw = pick(raw);
	out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size)
{
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	int index = impl->buffers.Find(id.value);
	if(!id.IsValid() || index < 0) {
		impl->error = "WriteBuffer received an unknown buffer";
		return GpuResult::InvalidHandle;
	}
	if(offset < 0 || size <= 0 || !data) {
		impl->error = "WriteBuffer received an invalid write range";
		return GpuResult::InvalidArgument;
	}
	const int64 buffer_size = impl->buffers[index].desc.size;
	if(offset > buffer_size || size > buffer_size - offset) {
		impl->error = "WriteBuffer range exceeds the buffer";
		return GpuResult::InvalidArgument;
	}
	if(!impl->WriteRawBuffer(impl->buffers[index].raw, (VkDeviceSize)offset, data, (VkDeviceSize)size))
		return GpuResult::InvalidState;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroyBuffer(GpuBufferId id)
{
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	int index = impl->buffers.Find(id.value);
	if(!id.IsValid() || index < 0) {
		impl->error = "DestroyBuffer received an unknown buffer";
		return GpuResult::InvalidHandle;
	}
	impl->DestroyRawBuffer(impl->buffers[index].raw);
	impl->buffers.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out)
{
	out = GpuTextureId();
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	if(desc.size.cx <= 0 || desc.size.cy <= 0 || desc.format == GpuFormat::Unknown) {
		impl->error = "CreateTexture requires positive size and a known format";
		return GpuResult::InvalidArgument;
	}
	const VkFormat format = ToVkFormat(desc.format);
	if(format == VK_FORMAT_UNDEFINED) {
		impl->error = "CreateTexture format is unsupported";
		return GpuResult::Unsupported;
	}
	VkImageCreateInfo ici {};
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = format;
	ici.extent = { (uint32_t)desc.size.cx, (uint32_t)desc.size.cy, 1 };
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = ToVkImageUsage(desc.usage);
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage image = VK_NULL_HANDLE;
	VkResult vr = impl->create_image(impl->device, &ici, nullptr, &image);
	if(vr != VK_SUCCESS) {
		impl->error = VkFailure("vkCreateImage", vr);
		return GpuResult::InvalidState;
	}
	VkDeviceMemory memory = VK_NULL_HANDLE;
	if(!impl->AllocateImageMemory(image, memory)) {
		impl->destroy_image(impl->device, image, nullptr);
		return GpuResult::InvalidState;
	}
	GpuTextureId id;
	id.value = impl->next_texture_id++;
	Impl::TextureState& state = impl->textures.Add(id.value, Impl::TextureState());
	state.desc = desc;
	state.image = image;
	state.memory = memory;
	state.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size)
{
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	int index = impl->textures.Find(id.value);
	if(!id.IsValid() || index < 0) {
		impl->error = "WriteTexture received an unknown texture";
		return GpuResult::InvalidHandle;
	}
	Impl::TextureState& state = impl->textures[index];
	if(!data || data_size <= 0 || desc.origin.x < 0 || desc.origin.y < 0 ||
	   desc.size.cx <= 0 || desc.size.cy <= 0 || desc.row_pitch <= 0) {
		impl->error = "WriteTexture received an invalid write layout";
		return GpuResult::InvalidArgument;
	}
	if(desc.size.cx > state.desc.size.cx || desc.size.cy > state.desc.size.cy ||
	   desc.origin.x > state.desc.size.cx - desc.size.cx ||
	   desc.origin.y > state.desc.size.cy - desc.size.cy) {
		impl->error = "WriteTexture region exceeds the texture";
		return GpuResult::InvalidArgument;
	}
	if(state.desc.format == GpuFormat::D24S8) {
		impl->error = "WriteTexture depth/stencil upload is deferred beyond S17B";
		return GpuResult::Unsupported;
	}
	const int bytes_per_pixel = BytesPerPixel(state.desc.format);
	const int64 tight_row = (int64)desc.size.cx * bytes_per_pixel;
	if(desc.row_pitch < tight_row) {
		impl->error = "WriteTexture row pitch is too small";
		return GpuResult::InvalidArgument;
	}
	if(desc.size.cy > 1 && desc.row_pitch > (std::numeric_limits<int64>::max() - tight_row) / (desc.size.cy - 1)) {
		impl->error = "WriteTexture layout overflows int64";
		return GpuResult::InvalidArgument;
	}
	const int64 required_size = desc.row_pitch * (desc.size.cy - 1) + tight_row;
	if(data_size < required_size) {
		impl->error = "WriteTexture data span is too small";
		return GpuResult::InvalidArgument;
	}
	if(!impl->UploadTexture(state, desc, data, data_size))
		return GpuResult::InvalidState;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroyTexture(GpuTextureId id)
{
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	int index = impl->textures.Find(id.value);
	if(!id.IsValid() || index < 0) {
		impl->error = "DestroyTexture received an unknown texture";
		return GpuResult::InvalidHandle;
	}
	VkResult vr = impl->queue_wait_idle(impl->graphics_queue);
	if(vr != VK_SUCCESS) {
		impl->error = VkFailure("vkQueueWaitIdle", vr);
		return GpuResult::InvalidState;
	}
	Impl::TextureState& state = impl->textures[index];
	if(state.image)
		impl->destroy_image(impl->device, state.image, nullptr);
	if(state.memory)
		impl->free_memory(impl->device, state.memory, nullptr);
	impl->textures.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreateSurface(const GpuSurfaceDesc&, GpuSurfaceId& out)
{
	out = GpuSurfaceId();
	return impl->Unsupported("CreateSurface");
}

GpuResult VulkanGpuDevice::DestroySurface(GpuSurfaceId)
{
	return impl->Unsupported("DestroySurface");
}

GpuResult VulkanGpuDevice::CreateSwapchain(const GpuSwapchainDesc&, GpuSwapchainId& out)
{
	out = GpuSwapchainId();
	return impl->Unsupported("CreateSwapchain");
}

GpuResult VulkanGpuDevice::DestroySwapchain(GpuSwapchainId)
{
	return impl->Unsupported("DestroySwapchain");
}

GpuResult VulkanGpuDevice::ResizeSwapchain(GpuSwapchainId, Size)
{
	return impl->Unsupported("ResizeSwapchain");
}

GpuResult VulkanGpuDevice::BeginFrame(GpuSwapchainId, GpuFrameInfo& out)
{
	out.frame = GpuFrameId();
	out.swapchain = GpuSwapchainId();
	out.color_target = GpuTextureId();
	out.size = Size(0, 0);
	out.color_format = GpuFormat::Unknown;
	return impl->Unsupported("BeginFrame");
}

GpuResult VulkanGpuDevice::Present(GpuFrameId)
{
	return impl->Unsupported("Present");
}

GpuResult VulkanGpuDevice::CreatePipeline(const GpuPipelineDesc&, GpuPipelineId& out)
{
	out = GpuPipelineId();
	return impl->Unsupported("CreatePipeline");
}

GpuResult VulkanGpuDevice::DestroyPipeline(GpuPipelineId)
{
	return impl->Unsupported("DestroyPipeline");
}

GpuResult VulkanGpuDevice::BeginCommands(GpuCommandListId& out)
{
	out = GpuCommandListId();
	return impl->Unsupported("BeginCommands");
}

GpuResult VulkanGpuDevice::BeginRenderPass(GpuCommandListId, const GpuRenderPassDesc&)
{
	return impl->Unsupported("BeginRenderPass");
}

GpuResult VulkanGpuDevice::SetPipeline(GpuCommandListId, GpuPipelineId)
{
	return impl->Unsupported("SetPipeline");
}

GpuResult VulkanGpuDevice::SetVertexBuffer(GpuCommandListId, GpuBufferId)
{
	return impl->Unsupported("SetVertexBuffer");
}

GpuResult VulkanGpuDevice::Draw(GpuCommandListId, int, int)
{
	return impl->Unsupported("Draw");
}

GpuResult VulkanGpuDevice::EndRenderPass(GpuCommandListId)
{
	return impl->Unsupported("EndRenderPass");
}

GpuResult VulkanGpuDevice::EndCommands(GpuCommandListId)
{
	return impl->Unsupported("EndCommands");
}

GpuResult VulkanGpuDevice::Submit(GpuCommandListId)
{
	return impl->Unsupported("Submit");
}

}
