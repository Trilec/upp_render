#include "RenderVulkanRhi.h"

// Preserve the accepted Stage-3/Stage-4 adapter byte-for-byte and rename only
// the entry points whose behaviour Stage 5 must extend. The public wrappers
// below add sampled-image state without forking resource/frame ownership.
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
		Vector<VkDescriptorPool> pools;
		Vector<int> pool_allocations;
	};

	Impl *core = nullptr;
	VectorMap<int, PipelineState> pipelines;
	VectorMap<int, CommandState> commands;
	bool resolved = false;

	PFN_vkCreateSampler create_sampler = nullptr;
	PFN_vkDestroySampler destroy_sampler = nullptr;
	PFN_vkCreateDescriptorSetLayout create_descriptor_set_layout = nullptr;
	PFN_vkDestroyDescriptorSetLayout destroy_descriptor_set_layout = nullptr;
	PFN_vkCreateDescriptorPool create_descriptor_pool = nullptr;
	PFN_vkDestroyDescriptorPool destroy_descriptor_pool = nullptr;
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
		           core->Resolve(allocate_descriptor_sets, "vkAllocateDescriptorSets") &&
		           core->Resolve(update_descriptor_sets, "vkUpdateDescriptorSets") &&
		           core->Resolve(cmd_bind_descriptor_sets, "vkCmdBindDescriptorSets");
		return resolved;
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

	void DestroyCommand(int command_id)
	{
		int index = commands.Find(command_id);
		if(index < 0)
			return;
		if(core && core->device && destroy_descriptor_pool)
			for(VkDescriptorPool pool : commands[index].pools)
				if(pool)
					destroy_descriptor_pool(core->device, pool, nullptr);
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
		while(pipelines.GetCount()) {
			DestroyPipelineState(pipelines[pipelines.GetCount() - 1]);
			pipelines.Remove(pipelines.GetCount() - 1);
		}
	}

	bool CreateDescriptorPool(CommandState& command)
	{
		VkDescriptorPoolSize size {};
		size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		size.descriptorCount = 64;
		VkDescriptorPoolCreateInfo ci {};
		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		ci.maxSets = 64;
		ci.poolSizeCount = 1;
		ci.pPoolSizes = &size;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		VkResult vr = create_descriptor_pool(core->device, &ci, nullptr, &pool);
		if(vr != VK_SUCCESS) {
			core->error = VkFailure("vkCreateDescriptorPool", vr);
			return false;
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

int VulkanGpuDevice::GetLivePipelineCount() const
{
	return GetLivePipelineCountBase() + (sampled_impl ? sampled_impl->pipelines.GetCount() : 0);
}

GpuResult VulkanGpuDevice::DestroyShader(GpuShaderId id)
{
	if(sampled_impl && sampled_impl->UsesShader(id)) {
		impl->error = "DestroyShader is forbidden while referenced by a sampled pipeline";
		return GpuResult::InvalidState;
	}
	return DestroyShaderBase(id);
}

GpuResult VulkanGpuDevice::CreatePipeline(const GpuPipelineDesc& desc, GpuPipelineId& out)
{
	out = GpuPipelineId();
	if(desc.sampled_texture_count == 0)
		return CreatePipelineBase(desc, out);
	if(!impl->CheckReady())
		return GpuResult::InvalidState;
	if(desc.sampled_texture_count != 1 || desc.vertex_layout != GpuVertexLayout::Position2Uv2Color4F) {
		impl->error = "sampled Vulkan pipeline requires one texture and Position2Uv2Color4F";
		return GpuResult::InvalidArgument;
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
	if(ToVkFormat(desc.color_format) == VK_FORMAT_UNDEFINED || desc.color_format == GpuFormat::D24S8) {
		impl->error = "sampled Vulkan pipeline requires a supported color format";
		return GpuResult::InvalidArgument;
	}
	if(desc.blend_mode != GpuBlendMode::Opaque && desc.blend_mode != GpuBlendMode::SourceOver) {
		impl->error = "sampled Vulkan pipeline received an unsupported blend mode";
		return GpuResult::InvalidArgument;
	}

	SampledImpl& sampled = Sampled();
	if(!sampled.Resolve())
		return GpuResult::InvalidState;

	SampledImpl::PipelineState state;
	state.desc = desc;

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
	VkResult vr = sampled.create_sampler(impl->device, &sci, nullptr, &state.sampler);
	if(vr != VK_SUCCESS) {
		impl->error = VkFailure("vkCreateSampler", vr);
		return GpuResult::InvalidState;
	}

	VkDescriptorSetLayoutBinding descriptor_binding {};
	descriptor_binding.binding = 0;
	descriptor_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_binding.descriptorCount = 1;
	descriptor_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutCreateInfo dlci {};
	dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dlci.bindingCount = 1;
	dlci.pBindings = &descriptor_binding;
	vr = sampled.create_descriptor_set_layout(impl->device, &dlci, nullptr, &state.descriptor_layout);
	if(vr != VK_SUCCESS) {
		sampled.destroy_sampler(impl->device, state.sampler, nullptr);
		impl->error = VkFailure("vkCreateDescriptorSetLayout", vr);
		return GpuResult::InvalidState;
	}

	VkPipelineLayoutCreateInfo lci {};
	lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	lci.setLayoutCount = 1;
	lci.pSetLayouts = &state.descriptor_layout;
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
	binding.stride = 8 * sizeof(float);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription attrs[3] {};
	attrs[0].location = 0; attrs[0].binding = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = 0;
	attrs[1].location = 1; attrs[1].binding = 0; attrs[1].format = VK_FORMAT_R32G32_SFLOAT; attrs[1].offset = 2 * sizeof(float);
	attrs[2].location = 2; attrs[2].binding = 0; attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[2].offset = 4 * sizeof(float);
	VkPipelineVertexInputStateCreateInfo vertex {};
	vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex.vertexBindingDescriptionCount = 1;
	vertex.pVertexBindingDescriptions = &binding;
	vertex.vertexAttributeDescriptionCount = 3;
	vertex.pVertexAttributeDescriptions = attrs;
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
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms {};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState blend_attachment {};
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
	                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if(desc.blend_mode == GpuBlendMode::SourceOver) {
		blend_attachment.blendEnable = VK_TRUE;
		blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
	pci.pColorBlendState = &blend;
	pci.pDynamicState = &dynamic;
	pci.layout = state.layout;
	vr = impl->create_graphics_pipelines(impl->device, VK_NULL_HANDLE, 1, &pci, nullptr, &state.pipeline);
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
	if(result == GpuResult::Ok && sampled_impl) {
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
	if(!command.pass_active || command.ended || sampled_pipeline->desc.color_format != command.color_format) {
		impl->error = "SetPipeline command/pass state or format is invalid";
		return GpuResult::InvalidState;
	}
	impl->cmd_bind_pipeline(command.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sampled_pipeline->pipeline);
	command.pipeline = pipeline;
	SampledImpl::CommandState& sampled_command = Sampled().GetCommand(list);
	sampled_command.pipeline = pipeline;
	sampled_command.bound_texture = GpuTextureId();
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
	const int layout_index = command.texture_layouts.Find(texture.value);
	const VkImageLayout layout = layout_index >= 0 ? command.texture_layouts[layout_index] : texture_state.layout;
	if(!texture_state.initialized || layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
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
	if(vertex_count <= 0 || first_vertex < 0) {
		impl->error = "Draw received an invalid vertex range";
		return GpuResult::InvalidArgument;
	}
	SampledImpl::CommandState *sampled_command = sampled_impl->FindCommand(list);
	if(!command.pass_active || !command.vertex_buffer.IsValid() || !sampled_command ||
	   sampled_command->pipeline != command.pipeline || !sampled_command->bound_texture.IsValid()) {
		impl->error = "sampled Draw requires an active pass, pipeline, vertex buffer, and sampled texture";
		return GpuResult::InvalidState;
	}
	impl->cmd_draw(command.buffer, (uint32_t)vertex_count, 1, (uint32_t)first_vertex, 0);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::Submit(GpuCommandListId list)
{
	GpuResult result = SubmitBase(list);
	if(result == GpuResult::Ok && sampled_impl)
		sampled_impl->DestroyCommand(list.value);
	return result;
}

}
