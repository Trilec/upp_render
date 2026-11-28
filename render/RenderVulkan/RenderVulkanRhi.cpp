#include "RenderVulkanRhi.h"

// Keep the core RHI and sampled-texture extension in one translation unit so the
// wrappers can reuse VulkanGpuDevice private Impl state without publishing backend
// internals. The renamed base entry points are deliberate internal layering, not a
// compatibility API.
#define GetLivePipelineCount GetLivePipelineCountBase
#define DestroyShader DestroyShaderBase
#define CreatePipeline CreatePipelineBase
#define DestroyPipeline DestroyPipelineBase
#define BeginRenderPass BeginRenderPassBase
#define SetPipeline SetPipelineBase
#define Draw DrawBase
#define Submit SubmitBase
#include "RenderVulkanRhiBase.inc"
#undef GetLivePipelineCount
#undef DestroyShader
#undef CreatePipeline
#undef DestroyPipeline
#undef BeginRenderPass
#undef SetPipeline
#undef Draw
#undef Submit

#include "RenderVulkanCompute.inc"

namespace Upp {

namespace {

static VkFilter ToVkSamplerFilter(GpuSamplerFilter filter)
{
	return filter == GpuSamplerFilter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

static VkSamplerMipmapMode ToVkMipmapMode(GpuSamplerFilter filter)
{
	return filter == GpuSamplerFilter::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

static VkSamplerAddressMode ToVkSamplerAddress(GpuSamplerAddressMode mode)
{
	return mode == GpuSamplerAddressMode::Repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT
	                                              : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

}

struct VulkanGpuDevice::SampledImpl {
	static constexpr int MaxVertexStorage = 4;
	struct PipelineState : Moveable<PipelineState> {
		GpuPipelineDesc desc;
		VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};

	struct CommandState : Moveable<CommandState> {
		GpuPipelineId pipeline;
		GpuTextureId bound_texture;
		VkDescriptorBufferInfo vertex_storage[MaxVertexStorage]{};
		bool storage_dirty = true;
		int storage_sets = 0;
		Vector<VkDescriptorPool> pools;
		Vector<int> pool_allocations;
	};

	Impl *core = nullptr;
	VectorMap<int, PipelineState> pipelines;
	VectorMap<int, CommandState> commands;
	Vector<VkDescriptorPool> idle_pools;
	uint64 pools_created = 0, pools_reused = 0;
	bool resolved = false, vertex_caps_known = false;
	GpuVertexPullCapabilities vertex_caps;
	PFN_vkCmdDrawIndirect draw_indirect = nullptr;

	PFN_vkCreateSampler create_sampler = nullptr;
	PFN_vkDestroySampler destroy_sampler = nullptr;
	PFN_vkCreateDescriptorSetLayout create_descriptor_set_layout = nullptr;
	PFN_vkDestroyDescriptorSetLayout destroy_descriptor_set_layout = nullptr;
	PFN_vkCreateDescriptorPool create_descriptor_pool = nullptr;
	PFN_vkDestroyDescriptorPool destroy_descriptor_pool = nullptr;
	PFN_vkResetDescriptorPool reset_descriptor_pool = nullptr;
	PFN_vkAllocateDescriptorSets allocate_descriptor_sets = nullptr;
	PFN_vkUpdateDescriptorSets update_descriptor_sets = nullptr;
	PFN_vkCmdBindDescriptorSets cmd_bind_descriptor_sets = nullptr;

	explicit SampledImpl(Impl& _core)
		: core(&_core)
	{
	}

	~SampledImpl()
	{
		DestroyAll();
	}

	bool Resolve()
	{
		if(resolved)
			return true;
		if(!core || !core->CheckReady())
			return false;
		resolved = core->Resolve(create_sampler, "vkCreateSampler") &&
		           core->Resolve(destroy_sampler, "vkDestroySampler") &&
		           core->Resolve(create_descriptor_set_layout, "vkCreateDescriptorSetLayout") &&
		           core->Resolve(destroy_descriptor_set_layout, "vkDestroyDescriptorSetLayout") &&
		           core->Resolve(create_descriptor_pool, "vkCreateDescriptorPool") &&
		           core->Resolve(destroy_descriptor_pool, "vkDestroyDescriptorPool") &&
		           core->Resolve(reset_descriptor_pool, "vkResetDescriptorPool") &&
		           core->Resolve(allocate_descriptor_sets, "vkAllocateDescriptorSets") &&
		           core->Resolve(update_descriptor_sets, "vkUpdateDescriptorSets") &&
		           core->Resolve(cmd_bind_descriptor_sets, "vkCmdBindDescriptorSets");
		return resolved;
	}

	bool QueryVertexStorage()
	{
		if(vertex_caps_known) return true;
		if(!Resolve()) return false;
		PFN_vkGetPhysicalDeviceProperties properties = nullptr;
		if(!core->ResolveInstance(properties, "vkGetPhysicalDeviceProperties")) return false;
		VkPhysicalDeviceProperties device{};
		properties(core->physical_device, &device);
		const auto& lim = device.limits;
		vertex_caps.max_storage_buffers = min<uint32>(MaxVertexStorage,
		       min(lim.maxPerStageDescriptorStorageBuffers, lim.maxDescriptorSetStorageBuffers));
		vertex_caps.max_storage_range = lim.maxStorageBufferRange;
		vertex_caps.storage_offset_alignment = max<uint64>(1, lim.minStorageBufferOffsetAlignment);
		vertex_caps.indirect_draw = lim.maxDrawIndirectCount && core->Resolve(draw_indirect, "vkCmdDrawIndirect");
		vertex_caps_known = true;
		return true;
	}

	PipelineState *FindPipeline(GpuPipelineId id)
	{
		int index = pipelines.Find(id.value);
		return index >= 0 ? &pipelines[index] : nullptr;
	}

	const PipelineState *FindPipeline(GpuPipelineId id) const
	{
		int index = pipelines.Find(id.value);
		return index >= 0 ? &pipelines[index] : nullptr;
	}

	CommandState& GetCommand(GpuCommandListId id)
	{
		int index = commands.Find(id.value);
		if(index >= 0)
			return commands[index];
		return commands.Add(id.value, CommandState());
	}

	CommandState *FindCommand(GpuCommandListId id)
	{
		int index = commands.Find(id.value);
		return index >= 0 ? &commands[index] : nullptr;
	}

	bool UsesShader(GpuShaderId id) const
	{
		for(int i = 0; i < pipelines.GetCount(); ++i)
			if(pipelines[i].desc.vertex_shader == id || pipelines[i].desc.fragment_shader == id)
				return true;
		return false;
	}

	void DestroyCommand(int command_id, bool completed = false)
	{
		int index = commands.Find(command_id);
		if(index < 0)
			return;
		if(core && core->device && destroy_descriptor_pool)
			for(VkDescriptorPool pool : commands[index].pools)
				if(pool) {
					// Only successful synchronous completion releases descriptors for reuse.
					if(completed && idle_pools.GetCount() < 4 &&
					   reset_descriptor_pool(core->device, pool, 0) == VK_SUCCESS)
						idle_pools.Add(pool);
					else
						destroy_descriptor_pool(core->device, pool, nullptr);
				}
		commands.Remove(index);
	}

	void DestroyPipelineState(PipelineState& state)
	{
		if(!core || !core->device)
			return;
		if(state.pipeline && core->destroy_pipeline)
			core->destroy_pipeline(core->device, state.pipeline, nullptr);
		if(state.layout && core->destroy_pipeline_layout)
			core->destroy_pipeline_layout(core->device, state.layout, nullptr);
		if(state.sampler && destroy_sampler)
			destroy_sampler(core->device, state.sampler, nullptr);
		if(state.descriptor_layout && destroy_descriptor_set_layout)
			destroy_descriptor_set_layout(core->device, state.descriptor_layout, nullptr);
		state = PipelineState();
	}

	void DestroyAll()
	{
		if(!core)
			return;
		if(core->ready && core->session && core->session->IsReady() && core->queue_wait_idle)
			core->queue_wait_idle(core->graphics_queue);
		while(commands.GetCount())
			DestroyCommand(commands.GetKey(commands.GetCount() - 1));
		for(VkDescriptorPool pool : idle_pools)
			if(pool) destroy_descriptor_pool(core->device, pool, nullptr);
		idle_pools.Clear();
		while(pipelines.GetCount()) {
			DestroyPipelineState(pipelines[pipelines.GetCount() - 1]);
			pipelines.Remove(pipelines.GetCount() - 1);
		}
	}

	bool CreateDescriptorPool(CommandState& command)
	{
		const VkDescriptorPoolSize sizes[] = {
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 * MaxVertexStorage}
		};
		VkDescriptorPoolCreateInfo ci {};
		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		ci.maxSets = 64;
		ci.poolSizeCount = 2;
		ci.pPoolSizes = sizes;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		if(!idle_pools.IsEmpty()) {
			pool = idle_pools.Pop();
			++pools_reused;
		}
		else {
			VkResult vr = create_descriptor_pool(core->device, &ci, nullptr, &pool);
			if(vr != VK_SUCCESS) {
				core->error = VkFailure("vkCreateDescriptorPool", vr);
				return false;
			}
			++pools_created;
		}
		command.pools.Add(pool);
		command.pool_allocations.Add(0);
		return true;
	}

	bool AllocateDescriptorSet(CommandState& command, VkDescriptorSetLayout layout, VkDescriptorSet& out)
	{
		out = VK_NULL_HANDLE;
		for(;;) {
			if(command.pools.IsEmpty() || command.pool_allocations.Top() >= 64)
				if(!CreateDescriptorPool(command))
					return false;
			VkDescriptorSetAllocateInfo ai {};
			ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			ai.descriptorPool = command.pools.Top();
			ai.descriptorSetCount = 1;
			ai.pSetLayouts = &layout;
			VkResult vr = allocate_descriptor_sets(core->device, &ai, &out);
			if(vr == VK_SUCCESS) {
				command.pool_allocations.Top()++;
				return true;
			}
			if(vr == VK_ERROR_OUT_OF_POOL_MEMORY || vr == VK_ERROR_FRAGMENTED_POOL) {
				command.pool_allocations.Top() = 64;
				continue;
			}
			core->error = VkFailure("vkAllocateDescriptorSets", vr);
			return false;
		}
	}
};

VulkanGpuDevice::SampledCleanup::~SampledCleanup()
{
	if(owner)
		owner->DestroySampledExtension();
}

VulkanGpuDevice::SampledImpl& VulkanGpuDevice::Sampled()
{
	if(!sampled_impl) {
		sampled_impl = new SampledImpl(*impl);
		sampled_cleanup.owner = this;
	}
	return *sampled_impl;
}

void VulkanGpuDevice::DestroySampledExtension()
{
	if(!sampled_impl)
		return;
	delete sampled_impl;
	sampled_impl = nullptr;
	sampled_cleanup.owner = nullptr;
}

GpuResult VulkanGpuDevice::GetSamplingStats(GpuSamplingStats& out)
{
	out = GpuSamplingStats();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(sampled_impl) {
		out.descriptor_pools_created = sampled_impl->pools_created;
		out.descriptor_pools_reused = sampled_impl->pools_reused;
		out.live_commands = sampled_impl->commands.GetCount();
		out.retained_descriptor_pools = sampled_impl->idle_pools.GetCount();
	}
	return GpuResult::Ok;
}

int VulkanGpuDevice::GetLivePipelineCount() const
{
	return GetLivePipelineCountBase() + (sampled_impl ? sampled_impl->pipelines.GetCount() : 0) + GetComputePipelineCount();
}

GpuResult VulkanGpuDevice::DestroyShader(GpuShaderId id)
{
	if(ComputeUsesShader(id)) { impl->error = "DestroyShader is forbidden while a compute pipeline references it"; return GpuResult::InvalidState; }
	if(sampled_impl && sampled_impl->UsesShader(id)) {
		impl->error = "DestroyShader is forbidden while referenced by a sampled pipeline";
		return GpuResult::InvalidState;
	}
	return DestroyShaderBase(id);
}

GpuResult VulkanGpuDevice::CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out)
{
	out = GpuPipelineId();
	const bool pull = desc.vertex_layout == GpuVertexLayout::StoragePull;
	if(!pull && desc.sampled_texture_count == 0 && desc.vertex_storage_count == 0)
		return CreatePipelineBase(desc, out);
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	const bool mesh = desc.vertex_layout == GpuVertexLayout::Position3Normal3Uv2Color4F;
	const bool sampled_layout = desc.sampled_texture_count == 1 && desc.vertex_storage_count == 0 &&
	                            (mesh || desc.vertex_layout == GpuVertexLayout::Position2Uv2Color4F);
	const bool pull_layout = pull && desc.sampled_texture_count == 0 && desc.vertex_storage_count > 0;
	if(!(sampled_layout || pull_layout) || !ValidDepthState(desc) ||
	   (desc.topology != GpuPrimitiveTopology::TriangleList && desc.topology != GpuPrimitiveTopology::LineList &&
	    !(pull && desc.topology == GpuPrimitiveTopology::TriangleStrip))) {
		impl->error = "descriptor graphics pipeline requires a valid sampled or vertex-storage layout";
		return GpuResult::InvalidArgument;
	}
	if(desc.depth_format != GpuFormat::Unknown) {
		GpuTextureCapabilities caps;
		GpuResult supported = GetTextureCapabilities(desc.depth_format, GpuTextureUsage_DepthAttachment, caps);
		if(supported != GpuResult::Ok) return supported;
		if(!caps.depth_attachment) return GpuResult::Unsupported;
	}
	if(desc.sampler_filter != GpuSamplerFilter::Nearest && desc.sampler_filter != GpuSamplerFilter::Linear) {
		impl->error = "sampled Vulkan pipeline received an unsupported filter";
		return GpuResult::InvalidArgument;
	}
	if(desc.sampler_address != GpuSamplerAddressMode::ClampToEdge && desc.sampler_address != GpuSamplerAddressMode::Repeat) {
		impl->error = "sampled Vulkan pipeline received an unsupported address mode";
		return GpuResult::InvalidArgument;
	}
	int vi = impl->shaders.Find(desc.vertex_shader.value);
	int fi = impl->shaders.Find(desc.fragment_shader.value);
	if(!desc.vertex_shader.IsValid() || !desc.fragment_shader.IsValid() || vi < 0 || fi < 0) {
		impl->error = "sampled Vulkan pipeline requires valid shader handles";
		return GpuResult::InvalidHandle;
	}
	if(impl->shaders[vi].desc.stage != GpuShaderStage::Vertex || impl->shaders[fi].desc.stage != GpuShaderStage::Fragment) {
		impl->error = "sampled Vulkan pipeline shader stages do not match";
		return GpuResult::InvalidArgument;
	}
	if(ToVkFormat(desc.color_format) == VK_FORMAT_UNDEFINED || IsDepthFormat(desc.color_format)) {
		impl->error = "sampled Vulkan pipeline requires a supported color format";
		return GpuResult::InvalidArgument;
	}
	if(desc.blend_mode != GpuBlendMode::Opaque && desc.blend_mode != GpuBlendMode::SourceOver &&
	   desc.blend_mode != GpuBlendMode::PremultipliedSourceOver && desc.blend_mode != GpuBlendMode::DestinationIn) {
		impl->error = "sampled Vulkan pipeline received an unsupported blend mode";
		return GpuResult::InvalidArgument;
	}

	SampledImpl& sampled = Sampled();
	if(!sampled.Resolve()) return GpuResult::InvalidState;
	if(pull && (!sampled.QueryVertexStorage() ||
	            uint32(desc.vertex_storage_count) > sampled.vertex_caps.max_storage_buffers)) {
		impl->error = "vertex-storage bindings exceed supported device limits";
		return GpuResult::Unsupported;
	}

	SampledImpl::PipelineState state;
	state.desc = desc;

	VkResult vr;
	if(!pull) {
		VkSamplerCreateInfo sci {};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.magFilter = ToVkSamplerFilter(desc.sampler_filter);
		sci.minFilter = ToVkSamplerFilter(desc.sampler_filter);
		sci.mipmapMode = ToVkMipmapMode(desc.sampler_filter);
		sci.addressModeU = ToVkSamplerAddress(desc.sampler_address);
		sci.addressModeV = ToVkSamplerAddress(desc.sampler_address);
		sci.addressModeW = ToVkSamplerAddress(desc.sampler_address);
		sci.minLod = 0.0f;
		sci.maxLod = 0.0f;
		sci.maxAnisotropy = 1.0f;
		vr = sampled.create_sampler(impl->device, &sci, nullptr, &state.sampler);
		if(vr != VK_SUCCESS) {
			impl->error = VkFailure("vkCreateSampler", vr);
			return GpuResult::InvalidState;
		}
	}
	const int binding_count = pull ? desc.vertex_storage_count : 1;
	VkDescriptorSetLayoutBinding bindings[SampledImpl::MaxVertexStorage]{};
	for(int slot = 0; slot < binding_count; ++slot) {
		auto& binding = bindings[slot];
		binding.binding = uint32(slot);
		binding.descriptorType = pull ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = pull ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo dlci{};
	dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dlci.bindingCount = uint32(binding_count);
	dlci.pBindings = bindings;
	vr = sampled.create_descriptor_set_layout(impl->device, &dlci, nullptr, &state.descriptor_layout);
	if(vr != VK_SUCCESS) {
		sampled.DestroyPipelineState(state);
		impl->error = VkFailure("vkCreateDescriptorSetLayout", vr);
		return GpuResult::InvalidState;
	}

	VkPushConstantRange parameters {};
	parameters.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	parameters.size = (uint32_t)desc.parameter_bytes;
	VkPipelineLayoutCreateInfo lci {};
	lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	lci.setLayoutCount = 1;
	lci.pSetLayouts = &state.descriptor_layout;
	if(desc.parameter_bytes) {
		lci.pushConstantRangeCount = 1;
		lci.pPushConstantRanges = &parameters;
	}
	vr = impl->create_pipeline_layout(impl->device, &lci, nullptr, &state.layout);
	if(vr != VK_SUCCESS) {
		sampled.DestroyPipelineState(state);
		impl->error = VkFailure("vkCreatePipelineLayout", vr);
		return GpuResult::InvalidState;
	}

	VkPipelineShaderStageCreateInfo stages[2] {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = impl->shaders[vi].module;
	stages[0].pName = impl->shaders[vi].desc.entry_point.Begin();
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = impl->shaders[fi].module;
	stages[1].pName = impl->shaders[fi].desc.entry_point.Begin();

	VkVertexInputBindingDescription binding {};
	binding.binding = 0;
	binding.stride = VertexStride(desc.vertex_layout);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription attrs[4] {};
	for(uint32_t i = 0; i < 4; ++i) { attrs[i].location = i; attrs[i].binding = 0; }
	attrs[0].format = mesh ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32_SFLOAT;
	attrs[1].format = mesh ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32_SFLOAT;
	attrs[1].offset = mesh ? 12 : 8;
	attrs[2].format = mesh ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
	attrs[2].offset = mesh ? 24 : 16;
	attrs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[3].offset = 32;
	VkPipelineVertexInputStateCreateInfo vertex {};
	vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	if(!pull) {
		vertex.vertexBindingDescriptionCount = 1;
		vertex.pVertexBindingDescriptions = &binding;
		vertex.vertexAttributeDescriptionCount = mesh ? 4 : 3;
		vertex.pVertexAttributeDescriptions = attrs;
	}
	VkPipelineInputAssemblyStateCreateInfo assembly {};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = ToVkTopology(desc.topology);
	VkPipelineViewportStateCreateInfo viewport {};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo raster {};
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = desc.cull_mode == GpuCullMode::Back ? VK_CULL_MODE_BACK_BIT :
	                  desc.cull_mode == GpuCullMode::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
	raster.frontFace = desc.front_face == GpuFrontFace::Clockwise ?
	                   VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;
	VkPipelineDepthStencilStateCreateInfo depth {};
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthTestEnable = desc.depth_test; depth.depthWriteEnable = desc.depth_write;
	depth.depthCompareOp = (VkCompareOp)desc.depth_compare;
	depth.minDepthBounds = 0.0f; depth.maxDepthBounds = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms {};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState blend_attachment {};
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
	                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
	if(desc.blend_mode == GpuBlendMode::DestinationIn) {
		blend_attachment.blendEnable = VK_TRUE;
		blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo blend {};
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_attachment;
	VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic {};
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynamic_states;
	VkFormat color_format = ToVkFormat(desc.color_format);
	VkPipelineRenderingCreateInfo rendering {};
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &color_format;
	rendering.depthAttachmentFormat = ToVkFormat(desc.depth_format);
	VkGraphicsPipelineCreateInfo pci {};
	pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pci.pNext = &rendering;
	pci.stageCount = 2;
	pci.pStages = stages;
	pci.pVertexInputState = &vertex;
	pci.pInputAssemblyState = &assembly;
	pci.pViewportState = &viewport;
	pci.pRasterizationState = &raster;
	pci.pMultisampleState = &ms;
	pci.pDepthStencilState = &depth;
	pci.pColorBlendState = &blend;
	pci.pDynamicState = &dynamic;
	pci.layout = state.layout;
	vr = impl->create_graphics_pipelines(impl->device, impl->pipeline_cache, 1, &pci, nullptr, &state.pipeline);
	if(vr != VK_SUCCESS) {
		sampled.DestroyPipelineState(state);
		impl->error = VkFailure("vkCreateGraphicsPipelines", vr);
		return GpuResult::InvalidState;
	}

	GpuPipelineId id;
	id.value = impl->next_pipeline_id++;
	sampled.pipelines.Add(id.value, pick(state));
	out = id;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DestroyPipeline(GpuPipelineId id)
{
	if(IsComputePipeline(id)) return DestroyComputePipeline(id);
	if(sampled_impl) {
		int index = sampled_impl->pipelines.Find(id.value);
		if(index >= 0) {
			if(!impl->CheckReady())
				return GpuResult::InvalidState;
			if(impl->HasOpenCommands()) {
				impl->error = "DestroyPipeline is forbidden while command lists are live";
				return GpuResult::InvalidState;
			}
			sampled_impl->DestroyPipelineState(sampled_impl->pipelines[index]);
			sampled_impl->pipelines.Remove(index);
			return GpuResult::Ok;
		}
	}
	return DestroyPipelineBase(id);
}

GpuResult VulkanGpuDevice::BeginRenderPass(GpuCommandListId list, const GpuRenderPassDesc& desc)
{
	GpuResult result = BeginRenderPassBase(list, desc);
	if(result != GpuResult::Ok)
		return result;

	// UiRenderer2D converts U++'s top-left framebuffer coordinates to
	// conventional +Y-up NDC. Vulkan's positive-height viewport would map
	// NDC +1 to the framebuffer bottom and invert the entire UI. Vulkan 1.3
	// supports negative-height viewports, so normalize the backend here rather
	// than teaching images, text, SVG or neutral display-list code about Vulkan.
	const int ci = impl->commands.Find(list.value);
	const int ti = impl->textures.Find(desc.color_target.value);
	if(desc.flip_viewport_y && ci >= 0 && ti >= 0) {
		const Size size = impl->textures[ti].desc.size;
		VkViewport viewport {};
		viewport.y = (float)size.cy;
		viewport.width = (float)size.cx;
		viewport.height = -(float)size.cy;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		impl->cmd_set_viewport(impl->commands[ci].buffer, 0, 1, &viewport);
	}

	if(sampled_impl) {
		SampledImpl::CommandState *state = sampled_impl->FindCommand(list);
		if(state) {
			state->pipeline = GpuPipelineId();
			state->bound_texture = GpuTextureId();
		}
	}
	return result;
}

GpuResult VulkanGpuDevice::SetPipeline(GpuCommandListId list, GpuPipelineId pipeline)
{
	SampledImpl::PipelineState *sampled_pipeline = sampled_impl ? sampled_impl->FindPipeline(pipeline) : nullptr;
	if(!sampled_pipeline) {
		if(sampled_impl) {
			SampledImpl::CommandState *state = sampled_impl->FindCommand(list);
			if(state) {
				state->pipeline = GpuPipelineId();
				state->bound_texture = GpuTextureId();
			}
		}
		return SetPipelineBase(list, pipeline);
	}
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) {
		impl->error = "SetPipeline received an unknown command list";
		return GpuResult::InvalidHandle;
	}
	Impl::CommandState& command = impl->commands[ci];
	if(!command.pass_active || command.ended || sampled_pipeline->desc.color_format != command.color_format ||
	   sampled_pipeline->desc.depth_format != command.depth_format) {
		impl->error = "SetPipeline command/pass state or format is invalid";
		return GpuResult::InvalidState;
	}
	impl->cmd_bind_pipeline(command.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sampled_pipeline->pipeline);
	command.pipeline = pipeline; command.parameters_set = sampled_pipeline->desc.parameter_bytes == 0;
	SampledImpl::CommandState& sampled_command = Sampled().GetCommand(list);
	sampled_command.pipeline = pipeline;
	sampled_command.bound_texture = GpuTextureId();
	for(auto& range : sampled_command.vertex_storage) range = VkDescriptorBufferInfo{};
	sampled_command.storage_dirty = true;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::SetSampledTexture(GpuCommandListId list, int slot, GpuTextureId texture)
{
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	if(slot != 0) {
		impl->error = "SetSampledTexture only supports slot zero";
		return GpuResult::InvalidArgument;
	}
	int ci = impl->commands.Find(list.value);
	int ti = impl->textures.Find(texture.value);
	if(!list.IsValid() || ci < 0 || !texture.IsValid() || ti < 0) {
		impl->error = "SetSampledTexture received an unknown handle";
		return GpuResult::InvalidHandle;
	}
	Impl::CommandState& command = impl->commands[ci];
	SampledImpl::PipelineState *pipeline = sampled_impl ? sampled_impl->FindPipeline(command.pipeline) : nullptr;
	if(!command.pass_active || command.ended || !pipeline || pipeline->desc.sampled_texture_count != 1) {
		impl->error = "SetSampledTexture requires an active sampled pipeline";
		return GpuResult::InvalidState;
	}
	Impl::TextureState& texture_state = impl->textures[ti];
	if(texture_state.borrowed_swapchain || !(texture_state.desc.usage & GpuTextureUsage_Sampled)) {
		impl->error = "SetSampledTexture requires an adapter-owned sampled texture";
		return GpuResult::InvalidArgument;
	}
	if(command.target == texture) {
		impl->error = "SetSampledTexture cannot alias the active color target";
		return GpuResult::InvalidState;
	}
	GpuTextureCapabilities capabilities;
	const auto supported = GetTextureCapabilities(texture_state.desc.format, texture_state.desc.usage, capabilities);
	if(supported != GpuResult::Ok) return supported;
	if(!capabilities.sampled ||
	   (pipeline->desc.sampler_filter == GpuSamplerFilter::Linear && !capabilities.linear_filter)) {
		impl->error = "sampled texture format does not support the pipeline filter";
		return GpuResult::Unsupported;
	}
	const int layout_index = command.texture_layouts.Find(texture.value);
	const VkImageLayout layout = layout_index >= 0 ? command.texture_layouts[layout_index] : texture_state.layout;
	const int initialized_index = command.texture_initialized.Find(texture.value);
	const bool initialized = initialized_index >= 0 ? command.texture_initialized[initialized_index] : texture_state.initialized;
	if(!initialized || layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		impl->error = "SetSampledTexture requires initialized shader-readable image content";
		return GpuResult::InvalidState;
	}
	SampledImpl& sampled = Sampled();
	if(!sampled.Resolve())
		return GpuResult::InvalidState;
	SampledImpl::CommandState& sampled_command = sampled.GetCommand(list);
	if(sampled_command.pipeline != command.pipeline) {
		impl->error = "SetSampledTexture sampled command state does not match the bound pipeline";
		return GpuResult::InvalidState;
	}

	VkImageViewCreateInfo vi {};
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = texture_state.image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = ToVkFormat(texture_state.desc.format);
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	VkImageView view = VK_NULL_HANDLE;
	VkResult vr = impl->create_image_view(impl->device, &vi, nullptr, &view);
	if(vr != VK_SUCCESS) {
		impl->error = VkFailure("vkCreateImageView", vr);
		return GpuResult::InvalidState;
	}

	VkDescriptorSet set = VK_NULL_HANDLE;
	if(!sampled.AllocateDescriptorSet(sampled_command, pipeline->descriptor_layout, set)) {
		impl->destroy_image_view(impl->device, view, nullptr);
		return GpuResult::InvalidState;
	}
	VkDescriptorImageInfo image_info {};
	image_info.sampler = pipeline->sampler;
	image_info.imageView = view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet write {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image_info;
	sampled.update_descriptor_sets(impl->device, 1, &write, 0, nullptr);
	sampled.cmd_bind_descriptor_sets(command.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	                                 pipeline->layout, 0, 1, &set, 0, nullptr);
	command.target_views.Add(view); // existing command cleanup owns all retained image views
	sampled_command.bound_texture = texture;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::Draw(GpuCommandListId list, int vertex_count, int first_vertex)
{
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) {
		impl->error = "Draw received an unknown command list";
		return GpuResult::InvalidHandle;
	}
	Impl::CommandState& command = impl->commands[ci];
	SampledImpl::PipelineState *pipeline = sampled_impl ? sampled_impl->FindPipeline(command.pipeline) : nullptr;
	if(!pipeline)
		return DrawBase(list, vertex_count, first_vertex);
	if(pipeline->desc.vertex_layout == GpuVertexLayout::StoragePull)
		return DrawInstanced(list, vertex_count, 1, first_vertex);
	if(vertex_count <= 0 || first_vertex < 0) {
		impl->error = "Draw received an invalid vertex range";
		return GpuResult::InvalidArgument;
	}
	SampledImpl::CommandState *sampled_command = sampled_impl->FindCommand(list);
	const int vi = impl->buffers.Find(command.vertex_buffer.value);
	if(!command.pass_active || command.ended || !command.vertex_buffer.IsValid() || vi < 0 ||
	   !command.parameters_set || !sampled_command || sampled_command->pipeline != command.pipeline ||
	   !sampled_command->bound_texture.IsValid()) {
		impl->error = "sampled Draw requires an active pass, pipeline, vertex buffer, and sampled texture";
		return GpuResult::InvalidState;
	}
	const int stride = VertexStride(pipeline->desc.vertex_layout);
	if(stride <= 0 || (int64)first_vertex + vertex_count > impl->buffers[vi].desc.size / stride) {
		impl->error = "Sampled Draw exceeds the bound vertex buffer"; return GpuResult::InvalidArgument;
	}
	impl->cmd_draw(command.buffer, (uint32_t)vertex_count, 1, (uint32_t)first_vertex, 0);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::GetVertexPullCapabilities(GpuVertexPullCapabilities& out)
{
	out = GpuVertexPullCapabilities();
	auto& descriptors = Sampled();
	if(!descriptors.QueryVertexStorage()) return GpuResult::Unsupported;
	out = descriptors.vertex_caps;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::SetVertexStorageBuffer(GpuCommandListId list, int slot,
                                                 GpuBufferId buffer, int64 offset, int64 size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int ci = impl->commands.Find(list.value), bi = impl->buffers.Find(buffer.value);
	if(ci < 0 || bi < 0) return GpuResult::InvalidHandle;
	const auto& command = impl->commands[ci];
	auto *pipeline = sampled_impl ? sampled_impl->FindPipeline(command.pipeline) : nullptr;
	auto *state = pipeline ? sampled_impl->FindCommand(list) : nullptr;
	if(!command.pass_active || command.ended || !state ||
	   pipeline->desc.vertex_layout != GpuVertexLayout::StoragePull) {
		impl->error = "vertex-storage binding requires an active vertex-pulling pipeline"; return GpuResult::InvalidState;
	}
	const auto& resource = impl->buffers[bi];
	const auto& caps = sampled_impl->vertex_caps;
	if(slot < 0 || slot >= pipeline->desc.vertex_storage_count ||
	   !(resource.desc.usage & GpuBufferUsage_Storage) || offset < 0 ||
	   offset >= resource.desc.size || size < 0 || uint64(offset) % caps.storage_offset_alignment) {
		impl->error = "invalid vertex-storage slot, usage, offset or alignment"; return GpuResult::InvalidArgument;
	}
	if(!size) size = resource.desc.size - offset;
	if(size > resource.desc.size - offset || uint64(size) > caps.max_storage_range) {
		impl->error = "vertex-storage range exceeds device or buffer bounds"; return GpuResult::InvalidArgument;
	}
	const VkDescriptorBufferInfo next{resource.raw.buffer, VkDeviceSize(offset), VkDeviceSize(size)};
	auto& current = state->vertex_storage[slot];
	if(current.buffer != next.buffer || current.offset != next.offset || current.range != next.range) {
		current = next;
		state->storage_dirty = true;
	}
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::BindVertexStorageForDraw(GpuCommandListId list)
{
	const int ci = impl->commands.Find(list.value);
	if(ci < 0) return GpuResult::InvalidHandle;
	const auto& command = impl->commands[ci];
	auto *pipeline = sampled_impl ? sampled_impl->FindPipeline(command.pipeline) : nullptr;
	auto *state = pipeline ? sampled_impl->FindCommand(list) : nullptr;
	if(!command.pass_active || command.ended || !command.parameters_set || !state ||
	   pipeline->desc.vertex_layout != GpuVertexLayout::StoragePull) {
		impl->error = "draw requires an active vertex-pulling pipeline and parameters"; return GpuResult::InvalidState;
	}
	if(!state->storage_dirty) return GpuResult::Ok;
	const int count = pipeline->desc.vertex_storage_count;
	for(int i = 0; i < count; ++i)
		if(!state->vertex_storage[i].buffer) {
			impl->error = "a declared vertex-storage binding is unset"; return GpuResult::InvalidState;
		}
	if(state->storage_sets >= 256) {
		impl->error = "vertex-storage descriptor budget exceeded"; return GpuResult::InvalidState;
	}
	VkDescriptorSet set = VK_NULL_HANDLE;
	if(!sampled_impl->AllocateDescriptorSet(*state, pipeline->descriptor_layout, set)) return GpuResult::InvalidState;
	VkWriteDescriptorSet writes[SampledImpl::MaxVertexStorage]{};
	for(int i = 0; i < count; ++i) {
		auto& write = writes[i];
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = uint32(i);
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo = &state->vertex_storage[i];
	}
	sampled_impl->update_descriptor_sets(impl->device, uint32(count), writes, 0, nullptr);
	sampled_impl->cmd_bind_descriptor_sets(command.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	                                      pipeline->layout, 0, 1, &set, 0, nullptr);
	state->storage_dirty = false;
	++state->storage_sets;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DrawInstanced(GpuCommandListId list, int vertices, int instances, int first_vertex)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(vertices <= 0 || instances <= 0 || first_vertex < 0 ||
	   int64(first_vertex) + vertices > std::numeric_limits<int>::max()) return GpuResult::InvalidArgument;
	const auto ready = BindVertexStorageForDraw(list);
	if(ready != GpuResult::Ok) return ready;
	impl->cmd_draw(impl->commands.Get(list.value).buffer, uint32(vertices), uint32(instances), uint32(first_vertex), 0);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DrawIndirect(GpuCommandListId list, GpuBufferId indirect, int64 offset)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int bi = impl->buffers.Find(indirect.value);
	if(bi < 0) return GpuResult::InvalidHandle;
	const auto& input = impl->buffers[bi];
	if(!(input.desc.usage & GpuBufferUsage_Indirect) || offset < 0 || offset % 4 ||
	   offset > input.desc.size || int64(sizeof(GpuDrawIndirectArguments)) > input.desc.size - offset) {
		impl->error = "invalid indirect draw buffer usage or arguments range"; return GpuResult::InvalidArgument;
	}
	const auto ready = BindVertexStorageForDraw(list);
	if(ready != GpuResult::Ok) return ready;
	if(!sampled_impl->vertex_caps.indirect_draw) return GpuResult::Unsupported;
	sampled_impl->draw_indirect(impl->commands.Get(list.value).buffer, input.raw.buffer,
	                            VkDeviceSize(offset), 1, sizeof(GpuDrawIndirectArguments));
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::Submit(GpuCommandListId list)
{
	GpuResult result = SubmitBase(list);
	if(result == GpuResult::Ok) {
		if(sampled_impl) sampled_impl->DestroyCommand(list.value, true);
		CompleteComputeCommands(list);
	}
	return result;
}

GpuResult VulkanGpuDevice::SetIndexBuffer(GpuCommandListId list, GpuBufferId buffer, GpuIndexType type)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int ci = impl->commands.Find(list.value), bi = impl->buffers.Find(buffer.value);
	if(!list.IsValid() || ci < 0 || !buffer.IsValid() || bi < 0) {
		impl->error = "SetIndexBuffer received an unknown handle"; return GpuResult::InvalidHandle;
	}
	if(type != GpuIndexType::UInt16 && type != GpuIndexType::UInt32) {
		impl->error = "SetIndexBuffer requires UInt16 or UInt32"; return GpuResult::InvalidArgument;
	}
	auto& command = impl->commands[ci]; const auto& indices = impl->buffers[bi];
	if(!command.pass_active || command.ended || !(indices.desc.usage & GpuBufferUsage_Index) ||
	   indices.index_bytes.GetCount() != indices.desc.size) {
		impl->error = "SetIndexBuffer requires an active pass and fully host-written index buffer"; return GpuResult::InvalidState;
	}
	impl->cmd_bind_index_buffer(command.buffer, indices.raw.buffer, 0,
	                           type == GpuIndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	command.index_buffer = buffer; command.index_type = type;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::SetGraphicsParameters(GpuCommandListId list, const void *data, int size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "SetGraphicsParameters received an unknown command list"; return GpuResult::InvalidHandle; }
	auto& command = impl->commands[ci];
	const int pi = impl->pipelines.Find(command.pipeline.value);
	const auto* sampled = sampled_impl ? sampled_impl->FindPipeline(command.pipeline) : nullptr;
	if(!command.pass_active || command.ended || (pi < 0 && !sampled)) {
		impl->error = "SetGraphicsParameters requires an active graphics pipeline"; return GpuResult::InvalidState;
	}
	const auto& desc = sampled ? sampled->desc : impl->pipelines[pi].desc;
	const VkPipelineLayout layout = sampled ? sampled->layout : impl->pipelines[pi].layout;
	if(!data || size <= 0 || size != desc.parameter_bytes || size % 4) {
		impl->error = "SetGraphicsParameters must match the complete declared payload"; return GpuResult::InvalidArgument;
	}
	impl->cmd_push_constants(command.buffer, layout,
	                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)size, data);
	command.parameters_set = true;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DrawIndexed(GpuCommandListId list, int index_count, int first_index)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "DrawIndexed received an unknown command list"; return GpuResult::InvalidHandle; }
	auto& command = impl->commands[ci];
	if(index_count <= 0 || first_index < 0) { impl->error = "DrawIndexed received an invalid index range"; return GpuResult::InvalidArgument; }
	const int pi = impl->pipelines.Find(command.pipeline.value);
	const auto* sampled = sampled_impl ? sampled_impl->FindPipeline(command.pipeline) : nullptr;
	const int vi = impl->buffers.Find(command.vertex_buffer.value), ii = impl->buffers.Find(command.index_buffer.value);
	if(!command.pass_active || command.ended || (pi < 0 && !sampled) || vi < 0 || ii < 0 || !command.parameters_set) {
		impl->error = "DrawIndexed requires an active pipeline, parameters, vertices and indices"; return GpuResult::InvalidState;
	}
	if(sampled) {
		const auto* bound = sampled_impl->FindCommand(list);
		if(!bound || bound->pipeline != command.pipeline || !bound->bound_texture.IsValid()) {
			impl->error = "Sampled DrawIndexed requires a bound texture"; return GpuResult::InvalidState;
		}
	}
	auto& indices = impl->buffers[ii];
	const int width = command.index_type == GpuIndexType::UInt16 ? 2 : 4;
	if((int64)first_index + index_count > indices.desc.size / width || indices.index_bytes.GetCount() != indices.desc.size) {
		impl->error = "DrawIndexed range exceeds initialized index buffer"; return GpuResult::InvalidArgument;
	}
	const int stride = VertexStride(sampled ? sampled->desc.vertex_layout : impl->pipelines[pi].desc.vertex_layout);
	if(stride <= 0) return GpuResult::InvalidState;
	uint32 maximum = 0; bool cached = false;
	for(const auto& range : indices.index_ranges)
		if(range.first == first_index && range.count == index_count && range.width == width) {
			maximum = range.maximum; cached = true; break;
		}
	if(!cached) {
		const byte *data = indices.index_bytes.Begin() + (int64)first_index * width;
		for(int i = 0; i < index_count; ++i) {
			uint32 value = 0;
			if(width == 2) { uint16 narrow_index; std::memcpy(&narrow_index, data + (int64)i * width, 2); value = narrow_index; }
			else std::memcpy(&value, data + (int64)i * width, 4);
			maximum = max(maximum, value);
		}
		if(indices.index_ranges.GetCount() == 64) indices.index_ranges.Remove(0);
		auto& range = indices.index_ranges.Add(); range.first = first_index; range.count = index_count;
		range.width = width; range.maximum = maximum;
	}
	if((uint64)maximum >= (uint64)(impl->buffers[vi].desc.size / stride)) {
		impl->error = "DrawIndexed index exceeds bound vertex buffer"; return GpuResult::InvalidArgument;
	}
	impl->cmd_draw_indexed(command.buffer, (uint32_t)index_count, 1, (uint32_t)first_index, 0, 0);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::DiscardCommands(GpuCommandListId list)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	const int ci = impl->commands.Find(list.value);
	if(!list.IsValid() || ci < 0) { impl->error = "DiscardCommands received an unknown command list"; return GpuResult::InvalidHandle; }
	auto& command = impl->commands[ci];
	if(command.submitted) {
		impl->error = "DiscardCommands cannot free work with uncertain GPU completion"; return GpuResult::InvalidState;
	}
	// Destroy recording commands first, then their descriptor references. Pending
	// layout/initialization maps intentionally never reach persistent textures.
	impl->DestroyCommandState(command); impl->commands.Remove(ci);
	if(sampled_impl) sampled_impl->DestroyCommand(list.value);
	if(compute_impl) compute_impl->DestroyCommand(list.value);
	return GpuResult::Ok;
}

}
