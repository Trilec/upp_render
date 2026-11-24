#include "RenderVulkanRhi.h"

#include <cstring>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>
#include <tuple>

namespace Upp {

namespace {

static VkFormat ToVkFormat(GpuFormat format)
{
	switch(format) {
	case GpuFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
	case GpuFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
	case GpuFormat::RGBA8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
	case GpuFormat::BGRA8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;
	case GpuFormat::R16F: return VK_FORMAT_R16_SFLOAT;
	case GpuFormat::D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
	case GpuFormat::D32F: return VK_FORMAT_D32_SFLOAT;
	case GpuFormat::RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;
	case GpuFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
	case GpuFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case GpuFormat::R32F: return VK_FORMAT_R32_SFLOAT;
	case GpuFormat::Unknown: return VK_FORMAT_UNDEFINED;
	}
	return VK_FORMAT_UNDEFINED;
}

static GpuFormat FromVkFormat(VkFormat format)
{
	switch(format) {
	case VK_FORMAT_R8G8B8A8_UNORM: return GpuFormat::RGBA8;
	case VK_FORMAT_B8G8R8A8_UNORM: return GpuFormat::BGRA8;
	case VK_FORMAT_R8G8B8A8_SRGB: return GpuFormat::RGBA8Srgb;
	case VK_FORMAT_B8G8R8A8_SRGB: return GpuFormat::BGRA8Srgb;
	case VK_FORMAT_R16_SFLOAT: return GpuFormat::R16F;
	case VK_FORMAT_D24_UNORM_S8_UINT: return GpuFormat::D24S8;
	case VK_FORMAT_D32_SFLOAT: return GpuFormat::D32F;
	case VK_FORMAT_R16G16B16A16_UNORM: return GpuFormat::RGBA16;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return GpuFormat::RGBA16F;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return GpuFormat::RGBA32F;
	case VK_FORMAT_R32_SFLOAT: return GpuFormat::R32F;
	default: return GpuFormat::Unknown;
	}
}

static bool IsDepthFormat(GpuFormat format)
{
	return format == GpuFormat::D24S8 || format == GpuFormat::D32F;
}

static int VertexStride(GpuVertexLayout layout)
{
	switch(layout) {
	case GpuVertexLayout::Position2Color4F: return 24;
	case GpuVertexLayout::Position2Uv2Color4F: return 32;
	case GpuVertexLayout::Position3Normal3Uv2Color4F: return 48;
	case GpuVertexLayout::Position3Normal3Color4F: return 40;
	case GpuVertexLayout::Position3Covariance6Corner2Color4F: return 60;
	default: return 0;
	}
}

static bool ValidDepthState(const GpuPipelineDesc& desc)
{
	if(desc.depth_format != GpuFormat::Unknown && desc.depth_format != GpuFormat::D32F) return false;
	if((desc.depth_test || desc.depth_write) && desc.depth_format == GpuFormat::Unknown) return false;
	if(desc.depth_write && !desc.depth_test) return false;
	if((int)desc.depth_compare < 0 || (int)desc.depth_compare > (int)GpuCompareOp::Always) return false;
	if((int)desc.cull_mode < 0 || (int)desc.cull_mode > (int)GpuCullMode::Back) return false;
	if(desc.front_face != GpuFrontFace::Clockwise && desc.front_face != GpuFrontFace::CounterClockwise) return false;
	return desc.parameter_bytes >= 0 && desc.parameter_bytes <= 128 && desc.parameter_bytes % 4 == 0;
}

static bool IsSwapchainColorFormat(GpuFormat format)
{
	return format == GpuFormat::RGBA8 || format == GpuFormat::BGRA8 ||
	       format == GpuFormat::RGBA8Srgb || format == GpuFormat::BGRA8Srgb;
}

static int BytesPerPixel(GpuFormat format)
{
	return GpuFormatBytesPerPixel(format);
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
	VkImageUsageFlags out = (usage & GpuTextureUsage_DepthAttachment) ? 0 : VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if(usage & GpuTextureUsage_DepthAttachment) out |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if(usage & GpuTextureUsage_ColorAttachment) out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if(usage & GpuTextureUsage_Sampled) out |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if(usage & GpuTextureUsage_TransferSrc) out |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if(usage & GpuTextureUsage_TransferDst) out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	return out;
}

static VkImageLayout FinalTextureLayout(int usage)
{
	if(usage & GpuTextureUsage_DepthAttachment)
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
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
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return 0;
	case VK_IMAGE_LAYOUT_GENERAL: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	default: return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
	}
}

static VkPipelineStageFlags2 Stage2ForLayout(VkImageLayout layout)
{
	switch(layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED: return VK_PIPELINE_STAGE_2_NONE;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	case VK_IMAGE_LAYOUT_GENERAL: return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	default: return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
}

static VkAccessFlags2 Access2ForLayout(VkImageLayout layout)
{
	switch(layout) {
	case VK_IMAGE_LAYOUT_UNDEFINED: return 0;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_2_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_2_SHADER_READ_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return 0;
	case VK_IMAGE_LAYOUT_GENERAL: return VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	default: return VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	}
}

static VkAttachmentLoadOp ToVkLoadOp(GpuLoadOp op)
{
	switch(op) {
	case GpuLoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
	case GpuLoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case GpuLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp ToVkStoreOp(GpuStoreOp op)
{
	return op == GpuStoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static VkPrimitiveTopology ToVkTopology(GpuPrimitiveTopology topology)
{
	return topology == GpuPrimitiveTopology::LineList ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
		VkMemoryPropertyFlags memory_flags = 0;
	};

	struct IndexRange : Moveable<IndexRange> {
		int first = 0, count = 0, width = 0;
		uint32 maximum = 0;
	};
	struct BufferState : Moveable<BufferState> {
		GpuBufferDesc desc;
		RawBuffer raw;
		// Host-authored index data only; cached maxima make retained mesh draws O(1).
		Vector<byte> index_bytes;
		Vector<IndexRange> index_ranges; // At most 64 queried ranges per buffer.
	};

	struct ImmutableAllocation {
		VkDevice device = VK_NULL_HANDLE;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize bytes = 0;
		PFN_vkDestroyImage destroy_image = nullptr;
		PFN_vkFreeMemory free_memory = nullptr;
		~ImmutableAllocation()
		{
			if(image) destroy_image(device, image, nullptr);
			if(memory) free_memory(device, memory, nullptr);
		}
	};
	using ImmutableKey = std::tuple<uintptr_t, uint64, int, int, int>;
	static std::map<ImmutableKey, std::weak_ptr<ImmutableAllocation>>& ImmutableCache()
	{
		static std::map<ImmutableKey, std::weak_ptr<ImmutableAllocation>> cache;
		return cache;
	}
	static std::mutex& ImmutableMutex() { static std::mutex mutex; return mutex; }

	struct TextureState : Moveable<TextureState> {
		GpuTextureDesc desc;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize allocation_size = 0;
		std::shared_ptr<ImmutableAllocation> immutable;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		bool initialized = false;
		bool borrowed_swapchain = false;
		bool renderable = false;
		GpuSwapchainId owner_swapchain;
		GpuFrameId owner_frame;
		int image_index = -1;
	};

	struct ShaderState : Moveable<ShaderState> {
		GpuShaderDesc desc;
		VkShaderModule module = VK_NULL_HANDLE;
	};

	struct PipelineState : Moveable<PipelineState> {
		GpuPipelineDesc desc;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};

	struct CommandState : Moveable<CommandState> {
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer buffer = VK_NULL_HANDLE;
		Vector<VkImageView> target_views;
		VectorMap<int, VkImageLayout> texture_layouts;
		VectorMap<int, bool> texture_initialized;
		GpuTextureId target;
		GpuPipelineId pipeline;
		GpuBufferId vertex_buffer;
		GpuBufferId index_buffer;
		GpuIndexType index_type = GpuIndexType::UInt32;
		GpuTextureId depth_target;
		GpuFormat depth_format = GpuFormat::Unknown;
		bool depth_store_result = false;
		bool parameters_set = false;
		bool submitted = false;
		GpuFrameId frame;
		GpuFormat color_format = GpuFormat::Unknown;
		VkImageLayout final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		bool store_result = false;
		bool pass_active = false;
		bool ended = false;
	};

	struct SurfaceState : Moveable<SurfaceState> {
		GpuSurfaceDesc desc;
		int live_swapchains = 0;
	};

	struct SwapchainState : Moveable<SwapchainState> {
		GpuSwapchainDesc desc;
		GpuTextureId backbuffer;
		GpuFrameId active_frame;
		uint64_t session_swapchain_id = 0;
		Vector<uint8_t> content_initialized;
	};

	struct FrameState : Moveable<FrameState> {
		GpuSwapchainId swapchain;
		GpuTextureId color_target;
		uint32_t image_index = UINT32_MAX;
		bool active = false;
		bool presented = false;
		bool rendered = false;
	};

	VulkanSurfaceSession *session = nullptr;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
	VkQueue graphics_queue = VK_NULL_HANDLE;
	uint32_t graphics_queue_family_index = 0;
	PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
	PFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;
	VulkanProcResolver proc_filter = nullptr;
	bool ready = false;
	String error;
	GpuDeviceId device_id;
	GpuAdapterInfo adapter_info;
	int next_buffer_id = 1;
	int next_texture_id = 1;
	int next_shader_id = 1;
	int next_pipeline_id = 1;
	int next_command_id = 1;
	int next_surface_id = 1;
	int next_swapchain_id = 1;
	int next_frame_id = 1;
	VectorMap<int, BufferState> buffers;
	VectorMap<int, TextureState> textures;
	VectorMap<int, ShaderState> shaders;
	VectorMap<int, PipelineState> pipelines;
	VectorMap<int, CommandState> commands;
	VectorMap<int, SurfaceState> surfaces;
	VectorMap<int, SwapchainState> swapchains;
	VectorMap<int, FrameState> frames;

	std::map<std::pair<int, int>, GpuTextureCapabilities> texture_capabilities;
	PFN_vkGetPhysicalDeviceMemoryProperties get_physical_device_memory_properties = nullptr;
	VkPhysicalDeviceMemoryProperties memory_properties {};

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
	PFN_vkCmdPipelineBarrier2 cmd_pipeline_barrier_2 = nullptr;
	PFN_vkCmdCopyBufferToImage cmd_copy_buffer_to_image = nullptr;
	PFN_vkCreateImageView create_image_view = nullptr;
	PFN_vkDestroyImageView destroy_image_view = nullptr;
	PFN_vkCreateShaderModule create_shader_module = nullptr;
	PFN_vkDestroyShaderModule destroy_shader_module = nullptr;
	PFN_vkCreatePipelineLayout create_pipeline_layout = nullptr;
	PFN_vkDestroyPipelineLayout destroy_pipeline_layout = nullptr;
	PFN_vkCreateGraphicsPipelines create_graphics_pipelines = nullptr;
	PFN_vkDestroyPipeline destroy_pipeline = nullptr;
	PFN_vkCmdBeginRendering cmd_begin_rendering = nullptr;
	PFN_vkCmdEndRendering cmd_end_rendering = nullptr;
	PFN_vkCmdBindPipeline cmd_bind_pipeline = nullptr;
	PFN_vkCmdBindVertexBuffers cmd_bind_vertex_buffers = nullptr;
	PFN_vkCmdDraw cmd_draw = nullptr;
	PFN_vkCmdBindIndexBuffer cmd_bind_index_buffer = nullptr;
	PFN_vkCmdDrawIndexed cmd_draw_indexed = nullptr;
	PFN_vkCmdPushConstants cmd_push_constants = nullptr;
	PFN_vkCmdSetViewport cmd_set_viewport = nullptr;
	PFN_vkCmdSetScissor cmd_set_scissor = nullptr;
	PFN_vkQueueSubmit queue_submit = nullptr;
	PFN_vkQueueWaitIdle queue_wait_idle = nullptr;

	template <class T>
	bool ResolveInstance(T& out, const char *name)
	{
		if(proc_filter && !proc_filter(nullptr, name)) {
			error = String("missing Vulkan procedure: ") + name;
			return false;
		}
		out = reinterpret_cast<T>(get_instance_proc_addr(instance, name));
		if(!out) {
			error = String("missing Vulkan procedure: ") + name;
			return false;
		}
		return true;
	}

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
		if(!ResolveInstance(get_physical_device_memory_properties, "vkGetPhysicalDeviceMemoryProperties"))
			return false;
		get_physical_device_memory_properties(physical_device, &memory_properties);
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
		       Resolve(cmd_pipeline_barrier_2, "vkCmdPipelineBarrier2") &&
		       Resolve(cmd_copy_buffer_to_image, "vkCmdCopyBufferToImage") &&
		       Resolve(create_image_view, "vkCreateImageView") &&
		       Resolve(destroy_image_view, "vkDestroyImageView") &&
		       Resolve(create_shader_module, "vkCreateShaderModule") &&
		       Resolve(destroy_shader_module, "vkDestroyShaderModule") &&
		       Resolve(create_pipeline_layout, "vkCreatePipelineLayout") &&
		       Resolve(destroy_pipeline_layout, "vkDestroyPipelineLayout") &&
		       Resolve(create_graphics_pipelines, "vkCreateGraphicsPipelines") &&
		       Resolve(destroy_pipeline, "vkDestroyPipeline") &&
		       Resolve(cmd_begin_rendering, "vkCmdBeginRendering") &&
		       Resolve(cmd_end_rendering, "vkCmdEndRendering") &&
		       Resolve(cmd_bind_pipeline, "vkCmdBindPipeline") &&
		       Resolve(cmd_bind_vertex_buffers, "vkCmdBindVertexBuffers") &&
		       Resolve(cmd_draw, "vkCmdDraw") &&
		       Resolve(cmd_bind_index_buffer, "vkCmdBindIndexBuffer") &&
		       Resolve(cmd_draw_indexed, "vkCmdDrawIndexed") &&
		       Resolve(cmd_push_constants, "vkCmdPushConstants") &&
		       Resolve(cmd_set_viewport, "vkCmdSetViewport") &&
		       Resolve(cmd_set_scissor, "vkCmdSetScissor") &&
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

	int FindMemoryType(uint32_t bits, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, VkMemoryPropertyFlags *out_flags = nullptr) const
	{
		auto find = [&](VkMemoryPropertyFlags wanted) -> int {
			for(uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
				if((bits & (1u << i)) == 0)
					continue;
				const VkMemoryPropertyFlags flags = memory_properties.memoryTypes[i].propertyFlags;
				if((flags & wanted) == wanted) {
					if(out_flags)
						*out_flags = flags;
					return (int)i;
				}
			}
			return -1;
		};
		int index = find(required | preferred);
		if(index < 0)
			index = find(required);
		return index;
	}

	bool HasOpenCommands() const { return commands.GetCount() != 0; }

	bool HasFrameCommands(GpuFrameId frame) const
	{
		for(int i = 0; i < commands.GetCount(); ++i)
			if(commands[i].frame == frame)
				return true;
		return false;
	}

	bool PipelineUsesShader(int shader_id) const
	{
		for(int i = 0; i < pipelines.GetCount(); ++i)
			if(pipelines[i].desc.vertex_shader.value == shader_id || pipelines[i].desc.fragment_shader.value == shader_id)
				return true;
		return false;
	}

	TextureState *FindTexture(GpuTextureId id)
	{
		int index = textures.Find(id.value);
		return index >= 0 ? &textures[index] : nullptr;
	}

	SwapchainState *FindSwapchain(GpuSwapchainId id)
	{
		int index = swapchains.Find(id.value);
		return index >= 0 ? &swapchains[index] : nullptr;
	}

	FrameState *FindFrame(GpuFrameId id)
	{
		int index = frames.Find(id.value);
		return index >= 0 ? &frames[index] : nullptr;
	}

	void ClearBorrowedTexture(TextureState& texture)
	{
		texture.image = VK_NULL_HANDLE;
		texture.memory = VK_NULL_HANDLE;
		texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		texture.initialized = false;
		texture.renderable = false;
		texture.owner_frame = GpuFrameId();
		texture.image_index = -1;
	}

	void RemoveFramesForSwapchain(GpuSwapchainId swapchain)
	{
		for(int i = frames.GetCount() - 1; i >= 0; --i)
			if(frames[i].swapchain == swapchain)
				frames.Remove(i);
	}

	bool ApplySessionSwapchainReport(SwapchainState& state)
	{
		const VulkanSurfaceReport& report = session->GetReport();
		const GpuFormat actual_format = FromVkFormat(report.swapchain_format);
		if(!session->HasSwapchain() || report.swapchain_id == 0 || report.swapchain_image_count < 2 ||
		   report.swapchain_extent.cx <= 0 || report.swapchain_extent.cy <= 0 || !IsSwapchainColorFormat(actual_format)) {
			error = "Vulkan session produced an unsupported neutral swapchain description";
			return false;
		}
		state.session_swapchain_id = report.swapchain_id;
		state.desc.size = report.swapchain_extent;
		state.desc.color_format = actual_format;
		state.desc.image_count = report.swapchain_image_count;
		state.active_frame = GpuFrameId();
		state.content_initialized.SetCount(report.swapchain_image_count, 0);
		TextureState *texture = FindTexture(state.backbuffer);
		if(texture) {
			texture->desc.size = report.swapchain_extent;
			texture->desc.format = actual_format;
			ClearBorrowedTexture(*texture);
		}
		return true;
	}

	bool CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage, RawBuffer& out,
	                     GpuBufferMemory memory = GpuBufferMemory::HostVisible)
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
		VkMemoryPropertyFlags memory_flags = 0;
		const bool gpu_only = memory == GpuBufferMemory::DeviceLocal;
		const int memory_type = FindMemoryType(requirements.memoryTypeBits,
		                                       gpu_only ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		                                       gpu_only ? 0 : VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_flags);
		if(memory_type < 0) {
			destroy_buffer(device, out.buffer, nullptr);
			out.buffer = VK_NULL_HANDLE;
			error = "no compatible Vulkan memory type for requested buffer memory policy";
			return false;
		}
		VkMemoryAllocateInfo mai {};
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = requirements.size;
		mai.memoryTypeIndex = (uint32_t)memory_type;
		vr = allocate_memory(device, &mai, nullptr, &out.memory);
		if(vr != VK_SUCCESS) {
			destroy_buffer(device, out.buffer, nullptr);
			out.buffer = VK_NULL_HANDLE;
			error = VkFailure("vkAllocateMemory", vr);
			return false;
		}
		vr = bind_buffer_memory(device, out.buffer, out.memory, 0);
		if(vr != VK_SUCCESS) {
			free_memory(device, out.memory, nullptr);
			destroy_buffer(device, out.buffer, nullptr);
			out = RawBuffer();
			error = VkFailure("vkBindBufferMemory", vr);
			return false;
		}
		out.allocation_size = requirements.size;
		out.memory_flags = memory_flags;
		return true;
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
		if((raw.memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
			VkMappedMemoryRange range {};
			range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range.memory = raw.memory;
			range.offset = 0;
			range.size = VK_WHOLE_SIZE;
			vr = flush_mapped_memory_ranges(device, 1, &range);
			if(vr != VK_SUCCESS) {
				unmap_memory(device, raw.memory);
				error = VkFailure("vkFlushMappedMemoryRanges", vr);
				return false;
			}
		}
		unmap_memory(device, raw.memory);
		return true;
	}

	bool AllocateImageMemory(VkImage image, VkDeviceMemory& out_memory)
	{
		out_memory = VK_NULL_HANDLE;
		VkMemoryRequirements requirements {};
		get_image_memory_requirements(device, image, &requirements);
		const int memory_type = FindMemoryType(requirements.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if(memory_type < 0) {
			error = "no compatible Vulkan memory type for texture";
			return false;
		}
		VkMemoryAllocateInfo mai {};
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = requirements.size;
		mai.memoryTypeIndex = (uint32_t)memory_type;
		VkResult vr = allocate_memory(device, &mai, nullptr, &out_memory);
		if(vr != VK_SUCCESS) {
			error = VkFailure("vkAllocateMemory", vr);
			return false;
		}
		vr = bind_image_memory(device, image, out_memory, 0);
		if(vr != VK_SUCCESS) {
			free_memory(device, out_memory, nullptr);
			out_memory = VK_NULL_HANDLE;
			error = VkFailure("vkBindImageMemory", vr);
			return false;
		}
		return true;
	}

	bool UploadTexture(TextureState& texture, const GpuTextureWriteDesc& desc, const void *data, int64 data_size)
	{
		const int bytes_per_pixel = BytesPerPixel(texture.desc.format);
		if(bytes_per_pixel <= 0 || IsDepthFormat(texture.desc.format)) {
			error = "VulkanGpuDevice texture upload format is unsupported";
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
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
		texture.initialized = true;
		cleanup();
		return true;
	}

	void DestroyCommandState(CommandState& state)
	{
		for(VkImageView view : state.target_views)
			if(view)
				destroy_image_view(device, view, nullptr);
		if(state.pool)
			destroy_command_pool(device, state.pool, nullptr);
		state = CommandState();
	}

	void DestroyAllResources()
	{
		if(!ready || !session || !session->IsReady()) {
			commands.Clear();
			pipelines.Clear();
			shaders.Clear();
			buffers.Clear();
			textures.Clear();
			frames.Clear();
			swapchains.Clear();
			surfaces.Clear();
			return;
		}
		queue_wait_idle(graphics_queue);
		while(commands.GetCount()) {
			DestroyCommandState(commands[commands.GetCount() - 1]);
			commands.Remove(commands.GetCount() - 1);
		}
		while(pipelines.GetCount()) {
			PipelineState& state = pipelines[pipelines.GetCount() - 1];
			if(state.pipeline) destroy_pipeline(device, state.pipeline, nullptr);
			if(state.layout) destroy_pipeline_layout(device, state.layout, nullptr);
			pipelines.Remove(pipelines.GetCount() - 1);
		}
		while(shaders.GetCount()) {
			ShaderState& state = shaders[shaders.GetCount() - 1];
			if(state.module) destroy_shader_module(device, state.module, nullptr);
			shaders.Remove(shaders.GetCount() - 1);
		}
		if(swapchains.GetCount() && session->HasSwapchain()) {
			const uint64_t current_id = session->GetReport().swapchain_id;
			for(int i = 0; i < swapchains.GetCount(); ++i)
				if(swapchains[i].session_swapchain_id == current_id) {
					session->DestroySwapchain();
					break;
				}
		}
		frames.Clear();
		swapchains.Clear();
		surfaces.Clear();
		std::lock_guard<std::mutex> immutable_guard(ImmutableMutex());
		while(textures.GetCount()) {
			TextureState& state = textures[textures.GetCount() - 1];
			if(!state.borrowed_swapchain && !state.immutable) {
				if(state.image) destroy_image(device, state.image, nullptr);
				if(state.memory) free_memory(device, state.memory, nullptr);
			}
			textures.Remove(textures.GetCount() - 1);
		}
		while(buffers.GetCount()) {
			DestroyRawBuffer(buffers[buffers.GetCount() - 1].raw);
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
	impl->adapter_info.capability_flags = GpuCapability_Buffers | GpuCapability_Textures |
	                                      GpuCapability_RenderPass | GpuCapability_Pipelines | GpuCapability_Shaders;
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
	impl->instance = interop.instance;
	impl->physical_device = interop.physical_device;
	impl->device = interop.device;
	impl->pipeline_cache = interop.pipeline_cache;
	impl->graphics_queue = interop.graphics_queue;
	impl->graphics_queue_family_index = interop.graphics_queue_family_index;
	impl->get_instance_proc_addr = interop.get_instance_proc_addr;
	impl->get_device_proc_addr = interop.get_device_proc_addr;
	impl->proc_filter = interop.proc_filter;
	if(!impl->instance || !impl->physical_device || !impl->device || !impl->pipeline_cache || !impl->graphics_queue ||
	   !impl->get_instance_proc_addr || !impl->get_device_proc_addr) {
		impl->error = "VulkanGpuDevice session interop is incomplete";
		return;
	}
	impl->ready = impl->ResolveResourceDispatch();
}

VulkanGpuDevice::~VulkanGpuDevice()
{
	DestroyComputeExtension();
	DestroySampledExtension();
	if(impl)
		impl->DestroyAllResources();
}

bool VulkanGpuDevice::IsReady() const { return impl && impl->ready && impl->session && impl->session->IsReady(); }
const String& VulkanGpuDevice::GetError() const { return impl->error; }
bool VulkanGpuDevice::QueryPresentationState(VulkanPresentationState& out) const
{
	out = VulkanPresentationState();
	if(!impl || !impl->session) return false;
	out.surface = impl->session->GetReport();
	out.frame = impl->session->GetFrameReport();
	if(!impl->ready || !impl->get_instance_proc_addr ||
	   impl->instance == VK_NULL_HANDLE || impl->physical_device == VK_NULL_HANDLE) {
		out.device_identity_error = "The actual Vulkan device is not ready for identity query.";
		return true;
	}
	const auto get_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
		impl->get_instance_proc_addr(impl->instance, "vkGetPhysicalDeviceProperties2"));
	if(!get_properties) {
		out.device_identity_error = "vkGetPhysicalDeviceProperties2 is unavailable.";
		return true;
	}
	VkPhysicalDeviceIDProperties identity{};
	identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
	VkPhysicalDeviceProperties2 properties{};
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &identity;
	get_properties(impl->physical_device, &properties);
	out.device_identity_available = true;
	out.device_luid_valid = identity.deviceLUIDValid == VK_TRUE;
	out.device_node_mask = identity.deviceNodeMask;
	std::memcpy(out.device_luid, identity.deviceLUID, VK_LUID_SIZE);
	std::memcpy(out.device_uuid, identity.deviceUUID, VK_UUID_SIZE);
	return true;
}

uint64 VulkanGpuDevice::GetLiveAllocationBytes() const
{
	uint64 bytes = 0;
	if(impl) {
		for(const Impl::BufferState& buffer : impl->buffers.GetValues())
			bytes += buffer.raw.allocation_size;
		for(const Impl::TextureState& texture : impl->textures.GetValues())
			if(!texture.immutable) bytes += texture.allocation_size;
	}
	return bytes;
}
int VulkanGpuDevice::GetLiveBufferCount() const { return impl ? impl->buffers.GetCount() : 0; }
int VulkanGpuDevice::GetLiveTextureCount() const { return impl ? impl->textures.GetCount() : 0; }
int VulkanGpuDevice::GetLiveShaderCount() const { return impl ? impl->shaders.GetCount() : 0; }
int VulkanGpuDevice::GetLivePipelineCount() const { return impl ? impl->pipelines.GetCount() : 0; }
int VulkanGpuDevice::GetLiveCommandCount() const { return impl ? impl->commands.GetCount() : 0; }
int VulkanGpuDevice::GetLiveSurfaceCount() const { return impl ? impl->surfaces.GetCount() : 0; }
int VulkanGpuDevice::GetLiveSwapchainCount() const { return impl ? impl->swapchains.GetCount() : 0; }
int VulkanGpuDevice::GetLiveFrameCount() const
{
	if(!impl) return 0;
	int count = 0;
	for(int i = 0; i < impl->frames.GetCount(); ++i)
		if(impl->frames[i].active)
			++count;
	return count;
}
GpuDeviceId VulkanGpuDevice::GetDeviceId() const { return impl->device_id; }
GpuBackendKind VulkanGpuDevice::GetBackendKind() const { return GpuBackendKind::Vulkan; }
GpuAdapterInfo VulkanGpuDevice::GetAdapterInfo() const { return impl->adapter_info; }
String VulkanGpuDevice::GetLastError() const { return impl ? impl->error : String(); }

GpuResult VulkanGpuDevice::CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out)
{
	out = GpuBufferId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(desc.size <= 0 || (desc.memory != GpuBufferMemory::HostVisible && desc.memory != GpuBufferMemory::DeviceLocal)) {
		impl->error = "CreateBuffer requires positive size and a valid memory policy"; return GpuResult::InvalidArgument;
	}
	if((desc.usage & GpuBufferUsage_Index) &&
	   (desc.memory != GpuBufferMemory::HostVisible || (desc.usage & GpuBufferUsage_Storage) || desc.size > 64 * 1024 * 1024)) {
		impl->error = "index buffers require host-visible non-storage memory and at most 64 MiB";
		return GpuResult::Unsupported;
	}
	Impl::RawBuffer raw;
	if(!impl->CreateRawBuffer((VkDeviceSize)desc.size, ToVkBufferUsage(desc.usage), raw, desc.memory)) return GpuResult::InvalidState;
	GpuBufferId id; id.value = impl->next_buffer_id++;
	Impl::BufferState& state = impl->buffers.Add(id.value, Impl::BufferState());
	state.desc = desc; state.raw = pick(raw); out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->buffers.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "WriteBuffer received an unknown buffer"; return GpuResult::InvalidHandle; }
	if(impl->buffers[index].desc.memory == GpuBufferMemory::DeviceLocal) {
		impl->error = "WriteBuffer cannot map a device-local buffer"; return GpuResult::InvalidState;
	}
	if(offset < 0 || size <= 0 || !data) { impl->error = "WriteBuffer received an invalid write range"; return GpuResult::InvalidArgument; }
	const int64 buffer_size = impl->buffers[index].desc.size;
	if(offset > buffer_size || size > buffer_size - offset) { impl->error = "WriteBuffer range exceeds the buffer"; return GpuResult::InvalidArgument; }
	Impl::BufferState& buffer = impl->buffers[index];
	const bool indices = (buffer.desc.usage & GpuBufferUsage_Index) != 0;
	if(indices && impl->HasOpenCommands()) {
		impl->error = "index buffer writes require no live command lists"; return GpuResult::InvalidState;
	}
	if(!impl->WriteRawBuffer(buffer.raw, (VkDeviceSize)offset, data, (VkDeviceSize)size)) {
		// A non-coherent flush can fail after native memory changed. Old CPU index
		// bounds must never authorize that data; require a complete successful upload.
		if(indices) { buffer.index_bytes.Clear(); buffer.index_ranges.Clear(); }
		return GpuResult::InvalidState;
	}
	if(indices) {
		if(offset == 0 && size == buffer_size) buffer.index_bytes.SetCount((int)buffer_size);
		if(buffer.index_bytes.GetCount() == buffer_size)
			std::memcpy(buffer.index_bytes.Begin() + (int)offset, data, (size_t)size);
		buffer.index_ranges.Clear();
	}
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroyBuffer(GpuBufferId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->buffers.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "DestroyBuffer received an unknown buffer"; return GpuResult::InvalidHandle; }
	if(impl->HasOpenCommands()) { impl->error = "DestroyBuffer is forbidden while command lists are live"; return GpuResult::InvalidState; }
	impl->DestroyRawBuffer(impl->buffers[index].raw); impl->buffers.Remove(index);
	return GpuResult::Ok;
}

bool VulkanGpuDevice::ReadTexturePixels(GpuTextureId id, Vector<byte>& pixels)
{
	pixels.Clear();
	auto& d = *impl;
	auto *texture = d.FindTexture(id);
	if(!d.ready || !texture || texture->borrowed_swapchain || !texture->initialized ||
	   d.HasOpenCommands() || !(texture->desc.usage & GpuTextureUsage_TransferSrc) ||
	   BytesPerPixel(texture->desc.format) <= 0 || IsDepthFormat(texture->desc.format)) {
		d.error = "readback requires initialized owned colour TransferSrc content and no open commands";
		return false;
	}
	const int64 pixel_count = (int64)texture->desc.size.cx * texture->desc.size.cy;
	const int bpp = BytesPerPixel(texture->desc.format);
	if(pixel_count <= 0 || pixel_count > 64 * 1024 * 1024 / bpp) {
		d.error = "diagnostic readback exceeds the 64 MiB limit";
		return false;
	}
	const int bytes = (int)(pixel_count * bpp);
	PFN_vkCmdCopyImageToBuffer copy_image = nullptr;
	PFN_vkInvalidateMappedMemoryRanges invalidate = nullptr;
	if(!d.Resolve(copy_image, "vkCmdCopyImageToBuffer") ||
	   !d.Resolve(invalidate, "vkInvalidateMappedMemoryRanges"))
		return false;
	Impl::RawBuffer staging;
	if(!d.CreateRawBuffer((VkDeviceSize)bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, staging))
		return false;
	VkCommandPool pool = VK_NULL_HANDLE;
	auto cleanup = [&]() {
		if(pool) d.destroy_command_pool(d.device, pool, nullptr);
		d.DestroyRawBuffer(staging);
	};
	auto fail = [&](const char *operation, VkResult result) {
		d.error = VkFailure(operation, result);
		cleanup();
		return false;
	};
	VkCommandPoolCreateInfo pci {};
	pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	pci.queueFamilyIndex = d.graphics_queue_family_index;
	VkResult vr = d.create_command_pool(d.device, &pci, nullptr, &pool);
	if(vr != VK_SUCCESS) return fail("vkCreateCommandPool", vr);
	VkCommandBuffer command = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo ai {};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = pool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;
	vr = d.allocate_command_buffers(d.device, &ai, &command);
	if(vr != VK_SUCCESS) return fail("vkAllocateCommandBuffers", vr);
	VkCommandBufferBeginInfo bi {};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vr = d.begin_command_buffer(command, &bi);
	if(vr != VK_SUCCESS) return fail("vkBeginCommandBuffer", vr);
	VkImageMemoryBarrier barrier {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = AccessForLayout(texture->layout);
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = texture->layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = texture->image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
	d.cmd_pipeline_barrier(command, StageForLayout(texture->layout), VK_PIPELINE_STAGE_TRANSFER_BIT,
	                       0, 0, nullptr, 0, nullptr, 1, &barrier);
	VkBufferImageCopy region {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { (uint32_t)texture->desc.size.cx, (uint32_t)texture->desc.size.cy, 1 };
	copy_image(command, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &region);
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = AccessForLayout(texture->layout);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = texture->layout;
	d.cmd_pipeline_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, StageForLayout(texture->layout),
	                       0, 0, nullptr, 0, nullptr, 1, &barrier);
	VkBufferMemoryBarrier host {};
	host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	host.buffer = staging.buffer;
	host.size = VK_WHOLE_SIZE;
	d.cmd_pipeline_barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
	                       0, 0, nullptr, 1, &host, 0, nullptr);
	vr = d.end_command_buffer(command);
	if(vr != VK_SUCCESS) return fail("vkEndCommandBuffer", vr);
	VkSubmitInfo submit {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &command;
	vr = d.queue_submit(d.graphics_queue, 1, &submit, VK_NULL_HANDLE);
	if(vr != VK_SUCCESS) return fail("vkQueueSubmit", vr);
	vr = d.queue_wait_idle(d.graphics_queue);
	if(vr != VK_SUCCESS) return fail("vkQueueWaitIdle", vr);
	void *mapped = nullptr;
	vr = d.map_memory(d.device, staging.memory, 0, VK_WHOLE_SIZE, 0, &mapped);
	if(vr != VK_SUCCESS) return fail("vkMapMemory", vr);
	if(!(staging.memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
		VkMappedMemoryRange range {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = staging.memory;
		range.size = VK_WHOLE_SIZE;
		vr = invalidate(d.device, 1, &range);
		if(vr != VK_SUCCESS) {
			d.unmap_memory(d.device, staging.memory);
			return fail("vkInvalidateMappedMemoryRanges", vr);
		}
	}
	pixels.SetCount((int)bytes);
	std::memcpy(pixels.Begin(), mapped, (size_t)bytes);
	d.unmap_memory(d.device, staging.memory);
	cleanup();
	return true;
}

GpuResult VulkanGpuDevice::GetTextureCapabilities(GpuFormat format, int usage,
                                                   GpuTextureCapabilities& out)
{
	out = GpuTextureCapabilities();
	auto& d = *impl;
	if(!d.CheckReady()) return GpuResult::InvalidState;
	const int allowed = GpuTextureUsage_ColorAttachment | GpuTextureUsage_Sampled |
	                    GpuTextureUsage_TransferSrc | GpuTextureUsage_TransferDst | GpuTextureUsage_DepthAttachment;
	if(usage & ~allowed) { d.error = "unknown texture usage bits"; return GpuResult::InvalidArgument; }
	if(IsDepthFormat(format)) {
		if(format != GpuFormat::D32F || usage != GpuTextureUsage_DepthAttachment) {
			d.error = "depth supports D32F attachment-only usage; sampling/upload/readback are not implemented";
			return GpuResult::Unsupported;
		}
	}
	else if(usage & GpuTextureUsage_DepthAttachment) {
		d.error = "DepthAttachment requires a depth format"; return GpuResult::InvalidArgument;
	}
	const VkFormat native = ToVkFormat(format);
	if(native == VK_FORMAT_UNDEFINED) {
		d.error = "texture format is unsupported"; return GpuResult::Unsupported;
	}
	const auto key = std::make_pair((int)format, usage);
	auto found = d.texture_capabilities.find(key);
	if(found != d.texture_capabilities.end()) { out = found->second; return GpuResult::Ok; }
	PFN_vkGetPhysicalDeviceFormatProperties get_format = nullptr;
	PFN_vkGetPhysicalDeviceImageFormatProperties get_image = nullptr;
	if(!d.ResolveInstance(get_format, "vkGetPhysicalDeviceFormatProperties") ||
	   !d.ResolveInstance(get_image, "vkGetPhysicalDeviceImageFormatProperties"))
		return GpuResult::InvalidState;
	VkFormatProperties properties {};
	get_format(d.physical_device, native, &properties);
	VkImageFormatProperties image {};
	const VkResult result = get_image(d.physical_device, native, VK_IMAGE_TYPE_2D,
	                                 VK_IMAGE_TILING_OPTIMAL, ToVkImageUsage(usage), 0, &image);
	if(result != VK_SUCCESS) {
		d.error = VkFailure("vkGetPhysicalDeviceImageFormatProperties", result);
		return result == VK_ERROR_FORMAT_NOT_SUPPORTED ? GpuResult::Unsupported : GpuResult::InvalidState;
	}
	if(!(image.sampleCounts & VK_SAMPLE_COUNT_1_BIT)) {
		d.error = "single-sample texture format is unsupported"; return GpuResult::Unsupported;
	}
	GpuTextureCapabilities caps;
	caps.format = format; caps.bytes_per_pixel = BytesPerPixel(format);
	const auto flags = properties.optimalTilingFeatures;
	caps.sampled = (flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
	caps.linear_filter = caps.sampled && (flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
	caps.color_attachment = (flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
	caps.depth_attachment = (flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
	caps.color_blend = caps.color_attachment && (flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0;
	caps.transfer_src = (flags & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0;
	caps.transfer_dst = (flags & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0;
	if(IsDepthFormat(format)) {
		// Capabilities describe this backend's usable operations, not just hardware.
		caps.sampled = caps.linear_filter = caps.color_attachment = caps.color_blend = false;
		caps.transfer_src = caps.transfer_dst = false;
	}
	caps.max_size = Size((int)min<uint32_t>(image.maxExtent.width, INT_MAX),
	                     (int)min<uint32_t>(image.maxExtent.height, INT_MAX));
	caps.max_resource_bytes = image.maxResourceSize;
	d.texture_capabilities[key] = caps;
	out = caps;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out)
{
	out = GpuTextureId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(desc.size.cx <= 0 || desc.size.cy <= 0 || desc.format == GpuFormat::Unknown) { impl->error = "CreateTexture requires positive size and a known format"; return GpuResult::InvalidArgument; }
	const VkFormat format = ToVkFormat(desc.format);
	if(format == VK_FORMAT_UNDEFINED) { impl->error = "CreateTexture format is unsupported"; return GpuResult::Unsupported; }
	GpuTextureCapabilities capabilities;
	const auto supported = GetTextureCapabilities(desc.format, desc.usage, capabilities);
	if(supported != GpuResult::Ok) return supported;
	if((desc.usage & GpuTextureUsage_DepthAttachment) && !capabilities.depth_attachment) {
		impl->error = "requested depth attachment format is unsupported"; return GpuResult::Unsupported;
	}
	if(desc.size.cx > capabilities.max_size.cx || desc.size.cy > capabilities.max_size.cy) {
		impl->error = "CreateTexture exceeds device format dimensions"; return GpuResult::Unsupported;
	}
	VkImageCreateInfo ici {};
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; ici.imageType = VK_IMAGE_TYPE_2D; ici.format = format;
	ici.extent = { (uint32_t)desc.size.cx, (uint32_t)desc.size.cy, 1 }; ici.mipLevels = 1; ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT; ici.tiling = VK_IMAGE_TILING_OPTIMAL; ici.usage = ToVkImageUsage(desc.usage);
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE; ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage image = VK_NULL_HANDLE;
	VkResult vr = impl->create_image(impl->device, &ici, nullptr, &image);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkCreateImage", vr); return GpuResult::InvalidState; }
	VkDeviceMemory memory = VK_NULL_HANDLE;
	if(!impl->AllocateImageMemory(image, memory)) { impl->destroy_image(impl->device, image, nullptr); return GpuResult::InvalidState; }
	GpuTextureId id; id.value = impl->next_texture_id++;
	Impl::TextureState& state = impl->textures.Add(id.value, Impl::TextureState());
	state.desc = desc; state.image = image; state.memory = memory;
	VkMemoryRequirements requirements {};
	impl->get_image_memory_requirements(impl->device, image, &requirements);
	state.allocation_size = requirements.size;
	out = id;
	return GpuResult::Ok;
}

uint64 VulkanGpuDevice::GetSharedImmutableAllocationBytes()
{
	std::lock_guard<std::mutex> guard(Impl::ImmutableMutex());
	uint64 bytes = 0;
	auto& cache = Impl::ImmutableCache();
	for(auto it = cache.begin(); it != cache.end();) {
		if(auto allocation = it->second.lock()) {
			bytes += allocation->bytes;
			++it;
		}
		else it = cache.erase(it);
	}
	return bytes;
}

GpuResult VulkanGpuDevice::AcquireImmutableTexture(uint64 identity, const GpuTextureDesc& desc,
                                                   const void *data, int64 size, GpuTextureId& out,
                                                   bool& uploaded)
{
	out = GpuTextureId();
	uploaded = false;
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int bpp = BytesPerPixel(desc.format);
	if(bpp <= 0 || IsDepthFormat(desc.format)) return GpuResult::Unsupported;
	if(!identity || !data || size <= 0 || desc.size.cx <= 0 || desc.size.cy <= 0 ||
	   size / bpp / desc.size.cx < desc.size.cy)
		return GpuResult::InvalidArgument;
	if(desc.usage != (GpuTextureUsage_Sampled | GpuTextureUsage_TransferDst))
		return GpuResult::InvalidArgument;
	std::lock_guard<std::mutex> guard(Impl::ImmutableMutex());
	auto& cache = Impl::ImmutableCache();
	for(auto it = cache.begin(); it != cache.end();)
		if(it->second.expired()) it = cache.erase(it);
		else ++it;
	const Impl::ImmutableKey key((uintptr_t)impl->device, identity,
	                             desc.size.cx, desc.size.cy, (int)desc.format);
	auto found = cache.find(key);
	std::shared_ptr<Impl::ImmutableAllocation> allocation;
	if(found != cache.end()) allocation = found->second.lock();
	if(allocation) {
		GpuTextureId id; id.value = impl->next_texture_id++;
		auto& state = impl->textures.Add(id.value, Impl::TextureState());
		state.desc = desc; state.image = allocation->image; state.memory = allocation->memory;
		state.allocation_size = allocation->bytes; state.immutable = allocation;
		state.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; state.initialized = true;
		out = id;
		return GpuResult::Ok;
	}
	GpuResult result = CreateTexture(desc, out);
	if(result != GpuResult::Ok) return result;
	GpuTextureWriteDesc write;
	write.size = desc.size; write.row_pitch = (int64)desc.size.cx * bpp;
	result = WriteTexture(out, write, data, size);
	if(result != GpuResult::Ok) {
		DestroyTexture(out); out = GpuTextureId(); return result;
	}
	auto& state = impl->textures.Get(out.value);
	allocation = std::make_shared<Impl::ImmutableAllocation>();
	allocation->device = impl->device; allocation->image = state.image; allocation->memory = state.memory;
	allocation->bytes = state.allocation_size;
	allocation->destroy_image = impl->destroy_image; allocation->free_memory = impl->free_memory;
	state.immutable = allocation;
	cache[key] = allocation;
	uploaded = true;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->textures.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "WriteTexture received an unknown texture"; return GpuResult::InvalidHandle; }
	Impl::TextureState& state = impl->textures[index];
	if(state.immutable) { impl->error = "WriteTexture cannot mutate an immutable shared texture"; return GpuResult::InvalidState; }
	if(state.borrowed_swapchain) { impl->error = "WriteTexture cannot write a borrowed swapchain backbuffer"; return GpuResult::InvalidState; }
	if(!data || data_size <= 0 || desc.origin.x < 0 || desc.origin.y < 0 || desc.size.cx <= 0 || desc.size.cy <= 0 || desc.row_pitch <= 0) {
		impl->error = "WriteTexture received an invalid write layout"; return GpuResult::InvalidArgument;
	}
	if(desc.size.cx > state.desc.size.cx || desc.size.cy > state.desc.size.cy || desc.origin.x > state.desc.size.cx - desc.size.cx || desc.origin.y > state.desc.size.cy - desc.size.cy) {
		impl->error = "WriteTexture region exceeds the texture"; return GpuResult::InvalidArgument;
	}
	if(IsDepthFormat(state.desc.format)) { impl->error = "WriteTexture depth/stencil upload is deferred"; return GpuResult::Unsupported; }
	const int bytes_per_pixel = BytesPerPixel(state.desc.format);
	const int64 tight_row = (int64)desc.size.cx * bytes_per_pixel;
	if(desc.row_pitch < tight_row) { impl->error = "WriteTexture row pitch is too small"; return GpuResult::InvalidArgument; }
	if(desc.size.cy > 1 && desc.row_pitch > (std::numeric_limits<int64>::max() - tight_row) / (desc.size.cy - 1)) { impl->error = "WriteTexture layout overflows int64"; return GpuResult::InvalidArgument; }
	const int64 required_size = desc.row_pitch * (desc.size.cy - 1) + tight_row;
	if(data_size < required_size) { impl->error = "WriteTexture data span is too small"; return GpuResult::InvalidArgument; }
	return impl->UploadTexture(state, desc, data, data_size) ? GpuResult::Ok : GpuResult::InvalidState;
}

GpuResult VulkanGpuDevice::DestroyTexture(GpuTextureId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->textures.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "DestroyTexture received an unknown texture"; return GpuResult::InvalidHandle; }
	if(impl->textures[index].borrowed_swapchain) { impl->error = "DestroyTexture cannot destroy a borrowed swapchain backbuffer"; return GpuResult::InvalidState; }
	if(impl->HasOpenCommands()) { impl->error = "DestroyTexture is forbidden while command lists are live"; return GpuResult::InvalidState; }
	VkResult vr = impl->queue_wait_idle(impl->graphics_queue);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkQueueWaitIdle", vr); return GpuResult::InvalidState; }
	Impl::TextureState& state = impl->textures[index];
	if(state.immutable) {
		std::lock_guard<std::mutex> guard(Impl::ImmutableMutex());
		state.immutable.reset();
	}
	else {
		if(state.image) impl->destroy_image(impl->device, state.image, nullptr);
		if(state.memory) impl->free_memory(impl->device, state.memory, nullptr);
	}
	impl->textures.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreateSurface(const GpuSurfaceDesc& desc, GpuSurfaceId& out)
{
	out = GpuSurfaceId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(desc.size.cx <= 0 || desc.size.cy <= 0 || !desc.native_window.IsValid() || desc.native_window.kind == GpuNativeWindowKind::None) {
		impl->error = "CreateSurface requires positive size and a valid native presentation surface";
		return GpuResult::InvalidArgument;
	}
	const GpuNativeWindowDesc& session_window = impl->session->GetReport().native_window;
	if(desc.native_window.kind != session_window.kind || desc.native_window.handle != session_window.handle) {
		impl->error = "CreateSurface must describe the VulkanSurfaceSession native window";
		return GpuResult::InvalidArgument;
	}
	if(impl->surfaces.GetCount()) {
		impl->error = "VulkanGpuDevice exposes one logical surface for its borrowed session";
		return GpuResult::InvalidState;
	}
	GpuSurfaceId id; id.value = impl->next_surface_id++;
	Impl::SurfaceState& state = impl->surfaces.Add(id.value, Impl::SurfaceState());
	state.desc = desc; out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroySurface(GpuSurfaceId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->surfaces.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "DestroySurface received an unknown surface"; return GpuResult::InvalidHandle; }
	if(impl->surfaces[index].live_swapchains > 0) { impl->error = "DestroySurface is forbidden while a logical swapchain is live"; return GpuResult::InvalidState; }
	impl->surfaces.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreateSwapchain(const GpuSwapchainDesc& desc, GpuSwapchainId& out)
{
	out = GpuSwapchainId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int surface_index = impl->surfaces.Find(desc.surface.value);
	if(!desc.surface.IsValid() || surface_index < 0) { impl->error = "CreateSwapchain received an unknown surface"; return GpuResult::InvalidHandle; }
	if(desc.size.cx <= 0 || desc.size.cy <= 0 || desc.image_count < 2 || !IsSwapchainColorFormat(desc.color_format)) {
		impl->error = "CreateSwapchain requires positive size, at least two images, and an 8-bit color format";
		return GpuResult::InvalidArgument;
	}
	if(impl->swapchains.GetCount() || impl->session->HasSwapchain()) {
		impl->error = "CreateSwapchain refuses duplicate or externally-created session swapchain ownership";
		return GpuResult::InvalidState;
	}
	if(!impl->session->CreateSwapchain(desc.size)) {
		impl->error = impl->session->GetReport().swapchain_error;
		if(impl->error.IsEmpty()) impl->error = "VulkanSurfaceSession swapchain creation failed";
		return GpuResult::InvalidState;
	}
	const VulkanSurfaceReport& report = impl->session->GetReport();
	const GpuFormat actual_format = FromVkFormat(report.swapchain_format);
	if(report.swapchain_id == 0 || report.swapchain_image_count < 2 || report.swapchain_extent.cx <= 0 || report.swapchain_extent.cy <= 0 || !IsSwapchainColorFormat(actual_format)) {
		impl->session->DestroySwapchain();
		impl->error = "VulkanSurfaceSession selected a swapchain format not represented by GpuFormat";
		return GpuResult::Unsupported;
	}

	GpuSwapchainId id; id.value = impl->next_swapchain_id++;
	GpuTextureId backbuffer; backbuffer.value = impl->next_texture_id++;
	Impl::SwapchainState& state = impl->swapchains.Add(id.value, Impl::SwapchainState());
	state.desc = desc;
	state.desc.size = report.swapchain_extent;
	state.desc.color_format = actual_format;
	state.desc.image_count = report.swapchain_image_count;
	state.backbuffer = backbuffer;
	state.session_swapchain_id = report.swapchain_id;
	state.content_initialized.SetCount(report.swapchain_image_count, 0);

	Impl::TextureState& texture = impl->textures.Add(backbuffer.value, Impl::TextureState());
	texture.desc.size = report.swapchain_extent;
	texture.desc.format = actual_format;
	texture.desc.usage = GpuTextureUsage_ColorAttachment;
	texture.desc.label = desc.label;
	texture.borrowed_swapchain = true;
	texture.owner_swapchain = id;
	impl->surfaces[surface_index].live_swapchains++;
	out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroySwapchain(GpuSwapchainId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->swapchains.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "DestroySwapchain received an unknown swapchain"; return GpuResult::InvalidHandle; }
	Impl::SwapchainState& state = impl->swapchains[index];
	if(state.active_frame.IsValid()) { impl->error = "DestroySwapchain is forbidden while a frame is active"; return GpuResult::InvalidState; }
	if(!impl->session->HasSwapchain() || impl->session->GetReport().swapchain_id != state.session_swapchain_id) {
		impl->error = "DestroySwapchain detected session swapchain ownership changed outside VulkanGpuDevice";
		return GpuResult::InvalidState;
	}
	if(!impl->session->DestroySwapchain()) {
		impl->error = impl->session->GetReport().swapchain_error;
		if(impl->error.IsEmpty()) impl->error = "VulkanSurfaceSession swapchain destruction failed";
		return GpuResult::InvalidState;
	}
	int surface_index = impl->surfaces.Find(state.desc.surface.value);
	if(surface_index >= 0 && impl->surfaces[surface_index].live_swapchains > 0)
		impl->surfaces[surface_index].live_swapchains--;
	int texture_index = impl->textures.Find(state.backbuffer.value);
	if(texture_index >= 0) impl->textures.Remove(texture_index);
	impl->RemoveFramesForSwapchain(id);
	impl->swapchains.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::ResizeSwapchain(GpuSwapchainId id, Size size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->swapchains.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "ResizeSwapchain received an unknown swapchain"; return GpuResult::InvalidHandle; }
	if(size.cx <= 0 || size.cy <= 0) { impl->error = "ResizeSwapchain requires positive size"; return GpuResult::InvalidArgument; }
	Impl::SwapchainState& state = impl->swapchains[index];
	if(state.active_frame.IsValid()) { impl->error = "ResizeSwapchain is forbidden while a frame is active"; return GpuResult::InvalidState; }
	if(!impl->session->HasSwapchain() || impl->session->GetReport().swapchain_id != state.session_swapchain_id) {
		impl->error = "ResizeSwapchain detected session swapchain ownership changed outside VulkanGpuDevice";
		return GpuResult::InvalidState;
	}
	const Size old_size = state.desc.size;
	if(!impl->session->DestroySwapchain()) {
		impl->error = "ResizeSwapchain could not destroy the current session swapchain";
		return GpuResult::InvalidState;
	}
	if(!impl->session->CreateSwapchain(size)) {
		String failure = impl->session->GetReport().swapchain_error;
		bool rolled_back = impl->session->CreateSwapchain(old_size);
		if(rolled_back)
			impl->ApplySessionSwapchainReport(state);
		impl->error = String("ResizeSwapchain recreation failed") + (failure.IsEmpty() ? String() : String(": ") + failure);
		return GpuResult::InvalidState;
	}
	if(!impl->ApplySessionSwapchainReport(state)) {
		impl->session->DestroySwapchain();
		bool rolled_back = impl->session->CreateSwapchain(old_size);
		if(rolled_back)
			impl->ApplySessionSwapchainReport(state);
		return GpuResult::Unsupported;
	}
	impl->RemoveFramesForSwapchain(id);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::BeginFrame(GpuSwapchainId swapchain, GpuFrameInfo& out)
{
	out.frame = GpuFrameId(); out.swapchain = GpuSwapchainId(); out.color_target = GpuTextureId(); out.size = Size(0, 0); out.color_format = GpuFormat::Unknown;
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	Impl::SwapchainState *state = impl->FindSwapchain(swapchain);
	if(!swapchain.IsValid() || !state) { impl->error = "BeginFrame received an unknown swapchain"; return GpuResult::InvalidHandle; }
	if(state->active_frame.IsValid()) { impl->error = "BeginFrame refuses a second active frame"; return GpuResult::InvalidState; }
	if(!impl->session->HasSwapchain() || impl->session->GetReport().swapchain_id != state->session_swapchain_id) {
		impl->error = "BeginFrame detected session swapchain ownership changed outside VulkanGpuDevice";
		return GpuResult::InvalidState;
	}
	impl->RemoveFramesForSwapchain(swapchain);
	if(!impl->session->AcquireFrame()) {
		impl->error = impl->session->GetFrameReport().error;
		if(impl->error.IsEmpty()) impl->error = "VulkanSurfaceSession frame acquisition failed";
		return impl->session->GetFrameReport().out_of_date ? GpuResult::OutOfDate : GpuResult::InvalidState;
	}
	VulkanSurfaceSession::FrameInterop interop;
	uint32_t image_index = UINT32_MAX;
	bool layout_initialized = false;
	if(!impl->session->GetAcquiredFrameInterop(interop, image_index, layout_initialized) || interop.swapchain_id != state->session_swapchain_id || image_index >= (uint32_t)state->content_initialized.GetCount()) {
		impl->session->PresentFrame();
		impl->error = "VulkanGpuDevice could not map the acquired session frame";
		return GpuResult::InvalidState;
	}
	Impl::TextureState *texture = impl->FindTexture(state->backbuffer);
	if(!texture || image_index >= (uint32_t)interop.images.GetCount()) {
		impl->session->PresentFrame();
		impl->error = "VulkanGpuDevice logical backbuffer is missing";
		return GpuResult::InvalidState;
	}
	GpuFrameId frame; frame.value = impl->next_frame_id++;
	Impl::FrameState& frame_state = impl->frames.Add(frame.value, Impl::FrameState());
	frame_state.swapchain = swapchain;
	frame_state.color_target = state->backbuffer;
	frame_state.image_index = image_index;
	frame_state.active = true;
	state->active_frame = frame;
	texture->image = interop.images[image_index];
	texture->layout = layout_initialized ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
	texture->initialized = state->content_initialized[image_index] != 0;
	texture->renderable = true;
	texture->owner_frame = frame;
	texture->image_index = (int)image_index;
	out.frame = frame;
	out.swapchain = swapchain;
	out.color_target = state->backbuffer;
	out.size = state->desc.size;
	out.color_format = state->desc.color_format;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::Present(GpuFrameId frame)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	Impl::FrameState *state = impl->FindFrame(frame);
	if(!frame.IsValid() || !state) { impl->error = "Present received an unknown frame"; return GpuResult::InvalidHandle; }
	if(state->presented || !state->active) { impl->error = "Present requires an active unpresented frame"; return GpuResult::InvalidState; }
	Impl::SwapchainState *swapchain = impl->FindSwapchain(state->swapchain);
	if(!swapchain || swapchain->active_frame != frame) { impl->error = "Present frame is not active on its swapchain"; return GpuResult::InvalidState; }
	if(impl->HasFrameCommands(frame)) { impl->error = "Present refuses unsubmitted command work for the active frame"; return GpuResult::InvalidState; }
	bool ok = state->rendered ? impl->session->PresentFrameImpl(true) : impl->session->PresentFrame();
	if(!ok) {
		impl->error = impl->session->GetFrameReport().error;
		if(!impl->session->HasAcquiredFrame()) {
			Impl::TextureState *texture = impl->FindTexture(state->color_target);
			if(texture) impl->ClearBorrowedTexture(*texture);
			state->active = false;
			swapchain->active_frame = GpuFrameId();
		}
		return impl->session->GetFrameReport().out_of_date ? GpuResult::OutOfDate : GpuResult::InvalidState;
	}
	Impl::TextureState *texture = impl->FindTexture(state->color_target);
	if(texture) impl->ClearBorrowedTexture(*texture);
	state->active = false;
	state->presented = true;
	swapchain->active_frame = GpuFrameId();
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreateShader(const GpuShaderDesc& desc, GpuShaderId& out)
{
	out = GpuShaderId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if((desc.stage != GpuShaderStage::Vertex && desc.stage != GpuShaderStage::Fragment && desc.stage != GpuShaderStage::Compute) || desc.format != GpuShaderFormat::SpirV || desc.entry_point.IsEmpty()) {
		impl->error = "CreateShader requires a vertex/fragment/compute SPIR-V shader and entry point"; return GpuResult::InvalidArgument;
	}
	if(desc.code.GetCount() < 20 || (desc.code.GetCount() & 3) != 0) { impl->error = "CreateShader SPIR-V payload has invalid size"; return GpuResult::InvalidArgument; }
	uint32_t magic = 0; std::memcpy(&magic, desc.code.Begin(), sizeof(magic));
	if(magic != 0x07230203u) { impl->error = "CreateShader SPIR-V payload has invalid magic"; return GpuResult::InvalidArgument; }
	Vector<uint32_t> words; words.SetCount(desc.code.GetCount() / 4); std::memcpy(words.Begin(), desc.code.Begin(), desc.code.GetCount());
	VkShaderModuleCreateInfo ci {}; ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; ci.codeSize = (size_t)desc.code.GetCount(); ci.pCode = words.Begin();
	VkShaderModule module = VK_NULL_HANDLE;
	VkResult vr = impl->create_shader_module(impl->device, &ci, nullptr, &module);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkCreateShaderModule", vr); return GpuResult::InvalidState; }
	GpuShaderId id; id.value = impl->next_shader_id++;
	Impl::ShaderState& state = impl->shaders.Add(id.value, Impl::ShaderState()); state.desc = desc; state.module = module; out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroyShader(GpuShaderId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->shaders.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "DestroyShader received an unknown shader"; return GpuResult::InvalidHandle; }
	if(impl->PipelineUsesShader(id.value) || impl->HasOpenCommands()) { impl->error = "DestroyShader is forbidden while referenced"; return GpuResult::InvalidState; }
	impl->destroy_shader_module(impl->device, impl->shaders[index].module, nullptr); impl->shaders.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out)
{
	out = GpuPipelineId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int vi = impl->shaders.Find(desc.vertex_shader.value), fi = impl->shaders.Find(desc.fragment_shader.value);
	if(!desc.vertex_shader.IsValid() || !desc.fragment_shader.IsValid() || vi < 0 || fi < 0) { impl->error = "CreatePipeline requires valid shader handles"; return GpuResult::InvalidHandle; }
	if(impl->shaders[vi].desc.stage != GpuShaderStage::Vertex || impl->shaders[fi].desc.stage != GpuShaderStage::Fragment) { impl->error = "CreatePipeline shader stages do not match"; return GpuResult::InvalidArgument; }
	if((desc.vertex_layout != GpuVertexLayout::Position2Color4F &&
	    desc.vertex_layout != GpuVertexLayout::Position3Normal3Uv2Color4F &&
	    desc.vertex_layout != GpuVertexLayout::Position3Normal3Color4F &&
	    desc.vertex_layout != GpuVertexLayout::Position3Covariance6Corner2Color4F) ||
	   ToVkFormat(desc.color_format) == VK_FORMAT_UNDEFINED || IsDepthFormat(desc.color_format) || !ValidDepthState(desc) ||
	   (desc.topology != GpuPrimitiveTopology::TriangleList && desc.topology != GpuPrimitiveTopology::LineList)) {
		impl->error = "CreatePipeline received invalid layout, format, topology, depth, cull or parameter state";
		return GpuResult::InvalidArgument;
	}
	if(desc.depth_format != GpuFormat::Unknown) {
		GpuTextureCapabilities caps;
		GpuResult supported = GetTextureCapabilities(desc.depth_format, GpuTextureUsage_DepthAttachment, caps);
		if(supported != GpuResult::Ok) return supported;
		if(!caps.depth_attachment) return GpuResult::Unsupported;
	}
	if(desc.blend_mode != GpuBlendMode::Opaque && desc.blend_mode != GpuBlendMode::SourceOver &&
	   desc.blend_mode != GpuBlendMode::DestinationInvert &&
	   desc.blend_mode != GpuBlendMode::PremultipliedSourceOver) {
		impl->error = "CreatePipeline received an unsupported blend mode"; return GpuResult::InvalidArgument;
	}
	VkPushConstantRange parameters {};
	parameters.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	parameters.size = (uint32_t)desc.parameter_bytes;
	VkPipelineLayoutCreateInfo lci {}; lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if(desc.parameter_bytes) { lci.pushConstantRangeCount = 1; lci.pPushConstantRanges = &parameters; }
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkResult vr = impl->create_pipeline_layout(impl->device, &lci, nullptr, &layout);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkCreatePipelineLayout", vr); return GpuResult::InvalidState; }
	VkPipelineShaderStageCreateInfo stages[2] {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = impl->shaders[vi].module; stages[0].pName = impl->shaders[vi].desc.entry_point.Begin();
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = impl->shaders[fi].module; stages[1].pName = impl->shaders[fi].desc.entry_point.Begin();
	VkVertexInputBindingDescription binding {}; binding.binding = 0; binding.stride = VertexStride(desc.vertex_layout); binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	const bool mesh_uv = desc.vertex_layout == GpuVertexLayout::Position3Normal3Uv2Color4F;
	const bool gaussian = desc.vertex_layout == GpuVertexLayout::Position3Covariance6Corner2Color4F;
	const bool mesh = mesh_uv || gaussian || desc.vertex_layout == GpuVertexLayout::Position3Normal3Color4F;
	VkVertexInputAttributeDescription attrs[5] {};
	for(uint32_t i = 0; i < 5; ++i) { attrs[i].location = i; attrs[i].binding = 0; }
	attrs[0].format = mesh ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32_SFLOAT;
	attrs[1].format = mesh ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
	attrs[1].offset = mesh ? 12 : 8;
	attrs[2].format = mesh_uv ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT; attrs[2].offset = 24;
	attrs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[3].offset = 32;
	if(gaussian) {
		attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[2].offset = 24;
		attrs[3].format = VK_FORMAT_R32G32_SFLOAT; attrs[3].offset = 36;
		attrs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[4].offset = 44;
	}
	VkPipelineVertexInputStateCreateInfo vertex {}; vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO; vertex.vertexBindingDescriptionCount = 1; vertex.pVertexBindingDescriptions = &binding; vertex.vertexAttributeDescriptionCount = gaussian ? 5 : mesh_uv ? 4 : mesh ? 3 : 2; vertex.pVertexAttributeDescriptions = attrs;
	VkPipelineInputAssemblyStateCreateInfo assembly {}; assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; assembly.topology = ToVkTopology(desc.topology);
	VkPipelineViewportStateCreateInfo viewport {}; viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; viewport.viewportCount = 1; viewport.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo raster {}; raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = desc.cull_mode == GpuCullMode::Back ? VK_CULL_MODE_BACK_BIT : desc.cull_mode == GpuCullMode::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE; raster.frontFace = desc.front_face == GpuFrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth = 1.0f;
	VkPipelineDepthStencilStateCreateInfo depth {}; depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthTestEnable = desc.depth_test; depth.depthWriteEnable = desc.depth_write;
	depth.depthCompareOp = (VkCompareOp)desc.depth_compare; // Both enums follow Vulkan's documented order.
	depth.minDepthBounds = 0.0f; depth.maxDepthBounds = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms {}; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState blend_attachment {};
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if(desc.blend_mode == GpuBlendMode::SourceOver || desc.blend_mode == GpuBlendMode::PremultipliedSourceOver) {
		blend_attachment.blendEnable = VK_TRUE;
		blend_attachment.srcColorBlendFactor = desc.blend_mode == GpuBlendMode::PremultipliedSourceOver ?
		                                     VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
		blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	else if(desc.blend_mode == GpuBlendMode::DestinationInvert) {
		blend_attachment.blendEnable = VK_TRUE;
		blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.colorBlendOp = VK_BLEND_OP_SUBTRACT;
		blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo blend {}; blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; blend.attachmentCount = 1; blend.pAttachments = &blend_attachment;
	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic {}; dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamic_states;
	VkFormat color_format = ToVkFormat(desc.color_format);
	VkPipelineRenderingCreateInfo rendering {}; rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO; rendering.colorAttachmentCount = 1; rendering.pColorAttachmentFormats = &color_format; rendering.depthAttachmentFormat = ToVkFormat(desc.depth_format);
	VkGraphicsPipelineCreateInfo pci {}; pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; pci.pNext = &rendering; pci.stageCount = 2; pci.pStages = stages;
	pci.pVertexInputState = &vertex; pci.pInputAssemblyState = &assembly; pci.pViewportState = &viewport; pci.pRasterizationState = &raster; pci.pMultisampleState = &ms; pci.pDepthStencilState = &depth; pci.pColorBlendState = &blend; pci.pDynamicState = &dynamic; pci.layout = layout;
	VkPipeline pipeline = VK_NULL_HANDLE;
	vr = impl->create_graphics_pipelines(impl->device, impl->pipeline_cache, 1, &pci, nullptr, &pipeline);
	if(vr != VK_SUCCESS) { impl->destroy_pipeline_layout(impl->device, layout, nullptr); impl->error = VkFailure("vkCreateGraphicsPipelines", vr); return GpuResult::InvalidState; }
	GpuPipelineId id; id.value = impl->next_pipeline_id++;
	Impl::PipelineState& state = impl->pipelines.Add(id.value, Impl::PipelineState()); state.desc = desc; state.layout = layout; state.pipeline = pipeline; out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroyPipeline(GpuPipelineId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int index = impl->pipelines.Find(id.value);
	if(!id.IsValid() || index < 0) { impl->error = "DestroyPipeline received an unknown pipeline"; return GpuResult::InvalidHandle; }
	if(impl->HasOpenCommands()) { impl->error = "DestroyPipeline is forbidden while command lists are live"; return GpuResult::InvalidState; }
	impl->destroy_pipeline(impl->device, impl->pipelines[index].pipeline, nullptr); impl->destroy_pipeline_layout(impl->device, impl->pipelines[index].layout, nullptr); impl->pipelines.Remove(index);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::BeginCommands(GpuCommandListId& out)
{
	out = GpuCommandListId();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	Impl::CommandState state;
	VkCommandPoolCreateInfo pci {}; pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; pci.queueFamilyIndex = impl->graphics_queue_family_index;
	VkResult vr = impl->create_command_pool(impl->device, &pci, nullptr, &state.pool);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkCreateCommandPool", vr); return GpuResult::InvalidState; }
	VkCommandBufferAllocateInfo ai {}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; ai.commandPool = state.pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
	vr = impl->allocate_command_buffers(impl->device, &ai, &state.buffer);
	if(vr == VK_SUCCESS) { VkCommandBufferBeginInfo bi {}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vr = impl->begin_command_buffer(state.buffer, &bi); }
	if(vr != VK_SUCCESS) { impl->DestroyCommandState(state); impl->error = VkFailure("begin command list", vr); return GpuResult::InvalidState; }
	GpuCommandListId id; id.value = impl->next_command_id++; impl->commands.Add(id.value, pick(state)); out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value), ti = impl->textures.Find(desc.color_target.value);
	if(!list.IsValid() || ci < 0) { impl->error = "BeginRenderPass received an unknown command list"; return GpuResult::InvalidHandle; }
	if(!desc.color_target.IsValid() || ti < 0) { impl->error = "BeginRenderPass received an unknown color target"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	Impl::TextureState& texture = impl->textures[ti];
	if(command.pass_active || command.ended) { impl->error = "BeginRenderPass command state is invalid"; return GpuResult::InvalidState; }
	auto valid_load = [](GpuLoadOp op) { return op == GpuLoadOp::Load || op == GpuLoadOp::Clear || op == GpuLoadOp::DontCare; };
	auto valid_store = [](GpuStoreOp op) { return op == GpuStoreOp::Store || op == GpuStoreOp::DontCare; };
	if(!valid_load(desc.color_load) || !valid_store(desc.color_store) ||
	   !std::isfinite(desc.clear_color.red) || !std::isfinite(desc.clear_color.green) ||
	   !std::isfinite(desc.clear_color.blue) || !std::isfinite(desc.clear_color.alpha)) {
		impl->error = "BeginRenderPass received invalid color operations/clear values"; return GpuResult::InvalidArgument;
	}
	if(!(texture.desc.usage & GpuTextureUsage_ColorAttachment) || desc.color_format != texture.desc.format || IsDepthFormat(desc.color_format)) {
		impl->error = "BeginRenderPass color target usage/format mismatch"; return GpuResult::InvalidArgument;
	}
	GpuFrameId frame_binding;
	if(texture.borrowed_swapchain) {
		Impl::FrameState *frame = impl->FindFrame(texture.owner_frame);
		Impl::SwapchainState *swapchain = impl->FindSwapchain(texture.owner_swapchain);
		if(!texture.renderable || !texture.image || !frame || !frame->active || frame->presented || !swapchain || swapchain->active_frame != texture.owner_frame) {
			impl->error = "BeginRenderPass borrowed swapchain backbuffer is not active"; return GpuResult::InvalidState;
		}
		if(command.frame.IsValid() && command.frame != texture.owner_frame) {
			impl->error = "BeginRenderPass command list cannot target multiple active frames"; return GpuResult::InvalidState;
		}
		frame_binding = texture.owner_frame;
	}
	auto layout_of = [&](GpuTextureId id, const Impl::TextureState& t) {
		const int i = command.texture_layouts.Find(id.value);
		return i >= 0 ? command.texture_layouts[i] : t.layout;
	};
	auto initialized = [&](GpuTextureId id, const Impl::TextureState& t) {
		const int i = command.texture_initialized.Find(id.value);
		return i >= 0 ? command.texture_initialized[i] : t.initialized;
	};
	if(desc.color_load == GpuLoadOp::Load && !initialized(desc.color_target, texture)) {
		impl->error = "BeginRenderPass cannot load an uninitialized color target"; return GpuResult::InvalidState;
	}
	Impl::TextureState *depth = nullptr;
	if(desc.depth_target.IsValid()) {
		int di = impl->textures.Find(desc.depth_target.value);
		if(di < 0) { impl->error = "BeginRenderPass received an unknown depth target"; return GpuResult::InvalidHandle; }
		depth = &impl->textures[di];
		if(desc.depth_format != GpuFormat::D32F || depth->desc.format != desc.depth_format ||
		   !(depth->desc.usage & GpuTextureUsage_DepthAttachment) || depth->desc.size != texture.desc.size ||
		   !valid_load(desc.depth_load) || !valid_store(desc.depth_store) || !std::isfinite(desc.clear_depth) ||
		   desc.clear_depth < 0 || desc.clear_depth > 1) {
			impl->error = "BeginRenderPass depth format/usage/size/operations/clear mismatch"; return GpuResult::InvalidArgument;
		}
		if(desc.depth_load == GpuLoadOp::Load && !initialized(desc.depth_target, *depth)) {
			impl->error = "BeginRenderPass cannot load an uninitialized depth target"; return GpuResult::InvalidState;
		}
	}
	else if(desc.depth_format != GpuFormat::Unknown) {
		impl->error = "BeginRenderPass depth format requires a target"; return GpuResult::InvalidArgument;
	}
	VkImageView color_view = VK_NULL_HANDLE, depth_view = VK_NULL_HANDLE;
	auto create_view = [&](const Impl::TextureState& t, VkImageAspectFlags aspect, VkImageView& view) {
		VkImageViewCreateInfo info {}; info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = t.image; info.viewType = VK_IMAGE_VIEW_TYPE_2D; info.format = ToVkFormat(t.desc.format);
		info.subresourceRange.aspectMask = aspect; info.subresourceRange.levelCount = 1; info.subresourceRange.layerCount = 1;
		const VkResult vr = impl->create_image_view(impl->device, &info, nullptr, &view);
		if(vr != VK_SUCCESS) { impl->error = VkFailure("vkCreateImageView", vr); return false; }
		return true;
	};
	if(!create_view(texture, VK_IMAGE_ASPECT_COLOR_BIT, color_view)) return GpuResult::InvalidState;
	if(depth && !create_view(*depth, VK_IMAGE_ASPECT_DEPTH_BIT, depth_view)) {
		impl->destroy_image_view(impl->device, color_view, nullptr); return GpuResult::InvalidState;
	}
	// Validation/allocation precede recording. A rejected pass changes no tracked state.
	if(frame_binding.IsValid()) command.frame = frame_binding;
	command.target_views.Add(color_view);
	if(depth_view) command.target_views.Add(depth_view);
	auto transition = [&](const Impl::TextureState& t, VkImageLayout old_layout,
	                      VkImageLayout new_layout, VkImageAspectFlags aspect) {
		VkImageMemoryBarrier2 barrier {}; barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.srcStageMask = Stage2ForLayout(old_layout); barrier.srcAccessMask = Access2ForLayout(old_layout);
		barrier.dstStageMask = Stage2ForLayout(new_layout); barrier.dstAccessMask = Access2ForLayout(new_layout);
		barrier.oldLayout = old_layout; barrier.newLayout = new_layout;
		barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = t.image; barrier.subresourceRange.aspectMask = aspect;
		barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
		VkDependencyInfo dep {}; dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dep.imageMemoryBarrierCount = 1; dep.pImageMemoryBarriers = &barrier;
		impl->cmd_pipeline_barrier_2(command.buffer, &dep);
	};
	// Even identical layouts need a dependency between consecutive attachment writes.
	transition(texture, layout_of(desc.color_target, texture), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	if(depth) transition(*depth, layout_of(desc.depth_target, *depth), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	VkRenderingAttachmentInfo color {}; color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color.imageView = color_view; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color.loadOp = ToVkLoadOp(desc.color_load); color.storeOp = ToVkStoreOp(desc.color_store);
	color.clearValue.color.float32[0] = desc.clear_color.red; color.clearValue.color.float32[1] = desc.clear_color.green;
	color.clearValue.color.float32[2] = desc.clear_color.blue; color.clearValue.color.float32[3] = desc.clear_color.alpha;
	VkRenderingAttachmentInfo depth_attachment {}; depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_attachment.imageView = depth_view; depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_attachment.loadOp = ToVkLoadOp(desc.depth_load); depth_attachment.storeOp = ToVkStoreOp(desc.depth_store);
	depth_attachment.clearValue.depthStencil.depth = desc.clear_depth;
	VkRenderingInfo rendering {}; rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering.renderArea.extent = { (uint32_t)texture.desc.size.cx, (uint32_t)texture.desc.size.cy };
	rendering.layerCount = 1; rendering.colorAttachmentCount = 1; rendering.pColorAttachments = &color;
	rendering.pDepthAttachment = depth ? &depth_attachment : nullptr;
	impl->cmd_begin_rendering(command.buffer, &rendering);
	VkViewport viewport {}; viewport.width = (float)texture.desc.size.cx; viewport.height = (float)texture.desc.size.cy;
	viewport.minDepth = 0; viewport.maxDepth = 1;
	impl->cmd_set_viewport(command.buffer, 0, 1, &viewport);
	VkRect2D scissor {}; scissor.extent = rendering.renderArea.extent; impl->cmd_set_scissor(command.buffer, 0, 1, &scissor);
	command.target = desc.color_target; command.depth_target = desc.depth_target;
	command.pipeline = GpuPipelineId(); command.vertex_buffer = GpuBufferId(); command.index_buffer = GpuBufferId();
	command.parameters_set = false; command.color_format = desc.color_format; command.depth_format = desc.depth_format;
	command.final_layout = texture.borrowed_swapchain ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : FinalTextureLayout(texture.desc.usage);
	command.store_result = desc.color_store == GpuStoreOp::Store;
	command.depth_store_result = depth && desc.depth_store == GpuStoreOp::Store;
	command.pass_active = true;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::SetPipeline(GpuCommandListId list, GpuPipelineId pipeline)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value), pi = impl->pipelines.Find(pipeline.value);
	if(!list.IsValid() || ci < 0 || !pipeline.IsValid() || pi < 0) { impl->error = "SetPipeline received an unknown handle"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	if(!command.pass_active || command.ended || impl->pipelines[pi].desc.color_format != command.color_format ||
	   impl->pipelines[pi].desc.depth_format != command.depth_format) { impl->error = "SetPipeline command/pass state or format is invalid"; return GpuResult::InvalidState; }
	impl->cmd_bind_pipeline(command.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, impl->pipelines[pi].pipeline);
	command.pipeline = pipeline; command.parameters_set = impl->pipelines[pi].desc.parameter_bytes == 0;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::SetVertexBuffer(GpuCommandListId list, GpuBufferId buffer)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value), bi = impl->buffers.Find(buffer.value);
	if(!list.IsValid() || ci < 0 || !buffer.IsValid() || bi < 0) { impl->error = "SetVertexBuffer received an unknown handle"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	if(!command.pass_active || command.ended || !(impl->buffers[bi].desc.usage & GpuBufferUsage_Vertex)) { impl->error = "SetVertexBuffer requires an active pass and vertex buffer"; return GpuResult::InvalidState; }
	VkBuffer vk_buffer = impl->buffers[bi].raw.buffer; VkDeviceSize offset = 0; impl->cmd_bind_vertex_buffers(command.buffer, 0, 1, &vk_buffer, &offset); command.vertex_buffer = buffer;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::Draw(GpuCommandListId list, int vertex_count, int first_vertex)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "Draw received an unknown command list"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	if(vertex_count <= 0 || first_vertex < 0) { impl->error = "Draw received an invalid vertex range"; return GpuResult::InvalidArgument; }
	if(!command.pass_active || !command.pipeline.IsValid() || !command.vertex_buffer.IsValid()) { impl->error = "Draw requires an active pass, pipeline, and vertex buffer"; return GpuResult::InvalidState; }
	const int pi = impl->pipelines.Find(command.pipeline.value), bi = impl->buffers.Find(command.vertex_buffer.value);
	if(pi < 0 || bi < 0 || !command.parameters_set) { impl->error = "Draw requires initialized graphics parameters"; return GpuResult::InvalidState; }
	const int stride = VertexStride(impl->pipelines[pi].desc.vertex_layout);
	if(stride <= 0 || (int64)first_vertex + vertex_count > impl->buffers[bi].desc.size / stride) {
		impl->error = "Draw vertex range exceeds bound buffer"; return GpuResult::InvalidArgument;
	}
	impl->cmd_draw(command.buffer, (uint32_t)vertex_count, 1, (uint32_t)first_vertex, 0);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::EndRenderPass(GpuCommandListId list)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "EndRenderPass received an unknown command list"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	if(!command.pass_active || command.ended) { impl->error = "EndRenderPass requires an active pass"; return GpuResult::InvalidState; }
	impl->cmd_end_rendering(command.buffer);
	int ti = impl->textures.Find(command.target.value);
	if(ti < 0) { impl->error = "EndRenderPass lost its color target"; return GpuResult::InvalidState; }
	Impl::TextureState& texture = impl->textures[ti];
	if(command.final_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		VkImageMemoryBarrier2 barrier {}; barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2; barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; barrier.dstStageMask = Stage2ForLayout(command.final_layout); barrier.dstAccessMask = Access2ForLayout(command.final_layout); barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; barrier.newLayout = command.final_layout; barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.image = texture.image; barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
		VkDependencyInfo dep {}; dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO; dep.imageMemoryBarrierCount = 1; dep.pImageMemoryBarriers = &barrier; impl->cmd_pipeline_barrier_2(command.buffer, &dep);
	}
	int layout_index = command.texture_layouts.Find(command.target.value);
	if(layout_index >= 0)
		command.texture_layouts[layout_index] = command.final_layout;
	else
		command.texture_layouts.Add(command.target.value, command.final_layout);
	int initialized_index = command.texture_initialized.Find(command.target.value);
	if(initialized_index >= 0)
		command.texture_initialized[initialized_index] = command.store_result;
	else
		command.texture_initialized.Add(command.target.value, command.store_result);
	if(command.depth_target.IsValid()) {
		int di = command.texture_layouts.Find(command.depth_target.value);
		if(di >= 0) command.texture_layouts[di] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		else command.texture_layouts.Add(command.depth_target.value, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		di = command.texture_initialized.Find(command.depth_target.value);
		if(di >= 0) command.texture_initialized[di] = command.depth_store_result;
		else command.texture_initialized.Add(command.depth_target.value, command.depth_store_result);
	}
	command.depth_target = GpuTextureId(); command.depth_format = GpuFormat::Unknown;
	command.depth_store_result = false; command.index_buffer = GpuBufferId(); command.parameters_set = false;
	command.target = GpuTextureId();
	command.pipeline = GpuPipelineId();
	command.vertex_buffer = GpuBufferId();
	command.color_format = GpuFormat::Unknown;
	command.final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	command.store_result = false;
	command.pass_active = false;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::EndCommands(GpuCommandListId list)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "EndCommands received an unknown command list"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	if(command.pass_active || command.ended) { impl->error = "EndCommands command state is invalid"; return GpuResult::InvalidState; }
	VkResult vr = impl->end_command_buffer(command.buffer);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkEndCommandBuffer", vr); return GpuResult::InvalidState; }
	command.ended = true;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::Submit(GpuCommandListId list)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "Submit received an unknown command list"; return GpuResult::InvalidHandle; }
	Impl::CommandState& command = impl->commands[ci];
	if(!command.ended || command.pass_active || command.submitted) { impl->error = "Submit requires an ended, never-submitted command list"; return GpuResult::InvalidState; }
	Impl::FrameState *frame = nullptr;
	if(command.frame.IsValid()) {
		frame = impl->FindFrame(command.frame);
		if(!frame || !frame->active || frame->presented) { impl->error = "Submit frame command work is no longer active"; return GpuResult::InvalidState; }
	}
	VkSubmitInfo submit {}; submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.commandBufferCount = 1; submit.pCommandBuffers = &command.buffer;
	VkResult vr = impl->queue_submit(impl->graphics_queue, 1, &submit, VK_NULL_HANDLE);
	if(vr == VK_SUCCESS) { command.submitted = true; vr = impl->queue_wait_idle(impl->graphics_queue); }
	if(vr != VK_SUCCESS) { impl->error = VkFailure("Vulkan command submission", vr); return GpuResult::InvalidState; }
	for(int i = 0; i < command.texture_layouts.GetCount(); ++i) {
		int ti = impl->textures.Find(command.texture_layouts.GetKey(i));
		if(ti >= 0) {
			impl->textures[ti].layout = command.texture_layouts[i];
			int initialized_index = command.texture_initialized.Find(command.texture_layouts.GetKey(i));
			if(initialized_index >= 0)
				impl->textures[ti].initialized = command.texture_initialized[initialized_index];
		}
	}
	if(frame) {
		frame->rendered = true;
		Impl::SwapchainState *swapchain = impl->FindSwapchain(frame->swapchain);
		Impl::TextureState *texture = impl->FindTexture(frame->color_target);
		if(swapchain && texture && frame->image_index < (uint32_t)swapchain->content_initialized.GetCount())
			swapchain->content_initialized[frame->image_index] = texture->initialized ? 1 : 0;
	}
	impl->DestroyCommandState(command); impl->commands.Remove(ci);
	return GpuResult::Ok;
}

}
