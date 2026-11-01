// Included once after RenderVulkanRhiBase.inc: shares the adapter's resource owner.
namespace Upp {
namespace {
bool ComputeLocalSize(const GpuShaderDesc& shader, const GpuComputeCapabilities& caps, uint32 local[3])
{
	if(shader.code.GetCount() < 20 || (shader.code.GetCount() & 3)) return false;
	Vector<uint32_t> words;
	words.SetCount(shader.code.GetCount() / 4);
	std::memcpy(words.Begin(), shader.code.Begin(), shader.code.GetCount());
	uint32_t entry = 0;
	for(int p = 5; p < words.GetCount();) {
		const uint32_t n = words[p] >> 16, op = words[p] & 65535;
		if(n == 0 || n > (uint32_t)(words.GetCount() - p)) return false;
		if(op == 15 && n >= 4 && words[p + 1] == 5) { // OpEntryPoint GLCompute
			const char *name = reinterpret_cast<const char *>(&words[p + 3]);
			const char *end = static_cast<const char *>(std::memchr(name, 0, (n - 3) * 4));
			if(!end) return false;
			if(String(name, (int)(end - name)) == shader.entry_point) entry = words[p + 2];
		}
		p += n;
	}
	if(!entry) return false;
	for(int p = 5; p < words.GetCount();) {
		const uint32_t n = words[p] >> 16, op = words[p] & 65535;
		if(op == 16 && n == 6 && words[p + 1] == entry && words[p + 2] == 17) {
			uint64 total = 1;
			for(int axis = 0; axis < 3; ++axis) {
				local[axis] = words[p + 3 + axis];
				if(!local[axis] || local[axis] > caps.max_local_size[axis] ||
				   total > caps.max_local_invocations / local[axis]) return false;
				total *= local[axis];
			}
			return true;
		}
		p += n;
	}
	return false; // LocalSizeId/specialization requires an explicit future contract.
}
}

struct VulkanGpuDevice::ComputeImpl {
	struct Pipeline : Moveable<Pipeline> {
		GpuComputePipelineDesc desc;
		VkDescriptorSetLayout descriptors = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
	};
	struct Range {
		GpuBufferId buffer;
		int64 offset = 0, size = 0;
	};
	struct Command : Moveable<Command> {
		GpuPipelineId pipeline;
		Range ranges[4];
		bool parameters = false;
		int dispatches = 0;
		Vector<VkDescriptorPool> pools;
		Vector<int> allocations;
	};
	Impl *core;
	bool queried = false, available = false;
	GpuComputeCapabilities caps;
	VectorMap<int, Pipeline> pipelines;
	VectorMap<int, Command> commands;
	Vector<VkDescriptorPool> idle_pools;
	uint64 pools_created = 0, pools_reused = 0;
	PFN_vkCreateComputePipelines create_pipelines = nullptr;
	PFN_vkCmdDispatch dispatch = nullptr;
	PFN_vkCmdPushConstants push_constants = nullptr;
	PFN_vkCreateDescriptorSetLayout create_layout = nullptr;
	PFN_vkDestroyDescriptorSetLayout destroy_layout = nullptr;
	PFN_vkCreateDescriptorPool create_pool = nullptr;
	PFN_vkDestroyDescriptorPool destroy_pool = nullptr;
	PFN_vkResetDescriptorPool reset_pool = nullptr;
	PFN_vkAllocateDescriptorSets allocate_sets = nullptr;
	PFN_vkUpdateDescriptorSets update_sets = nullptr;
	PFN_vkCmdBindDescriptorSets bind_sets = nullptr;
	PFN_vkInvalidateMappedMemoryRanges invalidate = nullptr;

	explicit ComputeImpl(Impl& value) : core(&value) {}
	~ComputeImpl() {
		if(core->ready && core->session && core->session->IsReady()) core->queue_wait_idle(core->graphics_queue);
		while(commands.GetCount()) DestroyCommand(commands.GetKey(commands.GetCount() - 1));
		for(auto pool : idle_pools) destroy_pool(core->device, pool, nullptr);
		idle_pools.Clear();
		while(pipelines.GetCount()) {
			Destroy(pipelines[pipelines.GetCount() - 1]);
			pipelines.Remove(pipelines.GetCount() - 1);
		}
	}
	bool Query() {
		if(queried) return available;
		if(!core->CheckReady()) return false;
		PFN_vkGetPhysicalDeviceProperties get_properties = nullptr;
		PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queues = nullptr;
		if(!core->ResolveInstance(get_properties, "vkGetPhysicalDeviceProperties") ||
		   !core->ResolveInstance(get_queues, "vkGetPhysicalDeviceQueueFamilyProperties") ||
		   !core->Resolve(create_pipelines, "vkCreateComputePipelines") ||
		   !core->Resolve(dispatch, "vkCmdDispatch") || !core->Resolve(push_constants, "vkCmdPushConstants") ||
		   !core->Resolve(create_layout, "vkCreateDescriptorSetLayout") ||
		   !core->Resolve(destroy_layout, "vkDestroyDescriptorSetLayout") ||
		   !core->Resolve(create_pool, "vkCreateDescriptorPool") ||
		   !core->Resolve(destroy_pool, "vkDestroyDescriptorPool") ||
		   !core->Resolve(reset_pool, "vkResetDescriptorPool") ||
		   !core->Resolve(allocate_sets, "vkAllocateDescriptorSets") ||
		   !core->Resolve(update_sets, "vkUpdateDescriptorSets") ||
		   !core->Resolve(bind_sets, "vkCmdBindDescriptorSets") ||
		   !core->Resolve(invalidate, "vkInvalidateMappedMemoryRanges")) return false;
		uint32_t count = 0;
		get_queues(core->physical_device, &count, nullptr);
		if(count == 0 || count > 256 || core->graphics_queue_family_index >= count) {
			core->error = "compute queue-family inventory is invalid"; return false;
		}
		Vector<VkQueueFamilyProperties> queues;
		queues.SetCount(count);
		get_queues(core->physical_device, &count, queues.Begin());
		queried = true;
		if(!(queues[core->graphics_queue_family_index].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
			core->error = "existing graphics queue does not support compute"; return false;
		}
		VkPhysicalDeviceProperties props {};
		get_properties(core->physical_device, &props);
		const auto& limits = props.limits;
		for(int i = 0; i < 3; ++i) {
			caps.max_group_count[i] = limits.maxComputeWorkGroupCount[i];
			caps.max_local_size[i] = limits.maxComputeWorkGroupSize[i];
		}
		caps.max_local_invocations = limits.maxComputeWorkGroupInvocations;
		caps.max_storage_buffers = min<uint32>(4, min(limits.maxPerStageDescriptorStorageBuffers, limits.maxDescriptorSetStorageBuffers));
		caps.max_storage_range = limits.maxStorageBufferRange;
		caps.storage_offset_alignment = max<uint64>(1, limits.minStorageBufferOffsetAlignment);
		caps.max_parameter_bytes = min<uint32>(128, limits.maxPushConstantsSize);
		caps.max_dispatches_per_command = 256;
		available = true;
		return true;
	}
	Pipeline *FindPipeline(GpuPipelineId id) {
		int i = pipelines.Find(id.value);
		return i >= 0 ? &pipelines[i] : nullptr;
	}
	Command *FindCommand(GpuCommandListId id) {
		int i = commands.Find(id.value);
		return i >= 0 ? &commands[i] : nullptr;
	}
	Command& GetCommand(GpuCommandListId id) {
		int i = commands.Find(id.value);
		return i >= 0 ? commands[i] : commands.Add(id.value, Command());
	}
	void Destroy(Pipeline& p) {
		if(p.pipeline) core->destroy_pipeline(core->device, p.pipeline, nullptr);
		if(p.layout) core->destroy_pipeline_layout(core->device, p.layout, nullptr);
		if(p.descriptors) destroy_layout(core->device, p.descriptors, nullptr);
		p = Pipeline();
	}
	void DestroyCommand(int id, bool completed = false) {
		int i = commands.Find(id);
		if(i < 0) return;
		for(auto pool : commands[i].pools) {
			// Only completed commands may recycle descriptors. Failed/live work is destroyed at teardown.
			if(completed && idle_pools.GetCount() < 4 && reset_pool(core->device, pool, 0) == VK_SUCCESS)
				idle_pools.Add(pool);
			else destroy_pool(core->device, pool, nullptr);
		}
		commands.Remove(i);
	}
	bool Allocate(Command& command, VkDescriptorSetLayout layout, VkDescriptorSet& set) {
		set = VK_NULL_HANDLE;
		for(;;) {
			if(command.pools.IsEmpty() || command.allocations.Top() >= 64) {
				if(command.pools.GetCount() >= 4) { core->error = "compute descriptor-pool budget exceeded"; return false; }
				VkDescriptorPoolSize size {};
				size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; size.descriptorCount = 64 * 4;
				VkDescriptorPoolCreateInfo info {};
				info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				info.maxSets = 64; info.poolSizeCount = 1; info.pPoolSizes = &size;
				VkDescriptorPool pool = VK_NULL_HANDLE;
				if(!idle_pools.IsEmpty()) { pool = idle_pools.Pop(); ++pools_reused; }
				else {
					VkResult result = create_pool(core->device, &info, nullptr, &pool);
					if(result != VK_SUCCESS) { core->error = VkFailure("vkCreateDescriptorPool(compute)", result); return false; }
					++pools_created;
				}
				command.pools.Add(pool); command.allocations.Add(0);
			}
			VkDescriptorSetAllocateInfo info {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			info.descriptorPool = command.pools.Top(); info.descriptorSetCount = 1; info.pSetLayouts = &layout;
			VkResult result = allocate_sets(core->device, &info, &set);
			if(result == VK_SUCCESS) { ++command.allocations.Top(); return true; }
			if(result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL) {
				core->error = VkFailure("vkAllocateDescriptorSets(compute)", result); return false;
			}
			command.allocations.Top() = 64;
		}
	}
};

VulkanGpuDevice::ComputeImpl& VulkanGpuDevice::Compute()
{
	if(!compute_impl) compute_impl = new ComputeImpl(*impl);
	return *compute_impl;
}

void VulkanGpuDevice::DestroyComputeExtension()
{
	delete compute_impl;
	compute_impl = nullptr;
}
int VulkanGpuDevice::GetComputePipelineCount() const { return compute_impl ? compute_impl->pipelines.GetCount() : 0; }
bool VulkanGpuDevice::ComputeUsesShader(GpuShaderId shader) const
{
	if(compute_impl) for(const auto& p : compute_impl->pipelines.GetValues()) if(p.desc.shader == shader) return true;
	return false;
}
bool VulkanGpuDevice::IsComputePipeline(GpuPipelineId pipeline) const
{
	return compute_impl && compute_impl->FindPipeline(pipeline);
}
void VulkanGpuDevice::CompleteComputeCommands(GpuCommandListId list)
{
	if(compute_impl) compute_impl->DestroyCommand(list.value, true);
}
GpuResult VulkanGpuDevice::GetComputeCapabilities(GpuComputeCapabilities& out)
{
	out = GpuComputeCapabilities();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	auto& compute = Compute();
	if(!compute.Query()) return GpuResult::Unsupported;
	out = compute.caps;
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::GetComputeStats(GpuComputeStats& out)
{
	out = GpuComputeStats();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(compute_impl) {
		out.descriptor_pools_created = compute_impl->pools_created;
		out.descriptor_pools_reused = compute_impl->pools_reused;
		out.live_commands = compute_impl->commands.GetCount();
		out.retained_descriptor_pools = compute_impl->idle_pools.GetCount();
	}
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::CreateComputePipeline(const GpuComputePipelineDesc& desc, GpuPipelineId& out)
{
	out = GpuPipelineId();
	GpuComputeCapabilities caps;
	GpuResult queried = GetComputeCapabilities(caps);
	if(queried != GpuResult::Ok) return queried;
	int si = impl->shaders.Find(desc.shader.value);
	if(si < 0) { impl->error = "compute pipeline received an unknown shader"; return GpuResult::InvalidHandle; }
	if(desc.storage_buffer_count < 1 || desc.storage_buffer_count > (int)caps.max_storage_buffers ||
	   desc.parameter_bytes < 0 || (desc.parameter_bytes & 3) || desc.parameter_bytes > (int)caps.max_parameter_bytes) {
		impl->error = "compute binding/parameter limits exceeded"; return GpuResult::InvalidArgument;
	}
	auto& shader = impl->shaders[si];
	uint32 local[3] {};
	if(shader.desc.stage != GpuShaderStage::Compute || !ComputeLocalSize(shader.desc, caps, local)) {
		impl->error = "compute shader entry point or literal local size is invalid/unsupported"; return GpuResult::InvalidArgument;
	}
	auto& compute = Compute();
	ComputeImpl::Pipeline p;
	p.desc = desc;
	VkDescriptorSetLayoutBinding bindings[4] {};
	for(int i = 0; i < desc.storage_buffer_count; ++i) {
		bindings[i].binding = i; bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[i].descriptorCount = 1; bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	VkDescriptorSetLayoutCreateInfo li {};
	li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	li.bindingCount = desc.storage_buffer_count; li.pBindings = bindings;
	VkResult vr = compute.create_layout(impl->device, &li, nullptr, &p.descriptors);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkCreateDescriptorSetLayout(compute)", vr); return GpuResult::InvalidState; }
	VkPushConstantRange constants {};
	constants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; constants.size = desc.parameter_bytes;
	VkPipelineLayoutCreateInfo layout {};
	layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout.setLayoutCount = 1; layout.pSetLayouts = &p.descriptors;
	if(desc.parameter_bytes) { layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &constants; }
	vr = impl->create_pipeline_layout(impl->device, &layout, nullptr, &p.layout);
	if(vr == VK_SUCCESS) {
		VkComputePipelineCreateInfo ci {};
		ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; ci.layout = p.layout;
		ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; ci.stage.module = shader.module;
		ci.stage.pName = shader.desc.entry_point.Begin();
		vr = compute.create_pipelines(impl->device, impl->pipeline_cache, 1, &ci, nullptr, &p.pipeline);
	}
	if(vr != VK_SUCCESS) { compute.Destroy(p); impl->error = VkFailure("create compute pipeline/layout", vr); return GpuResult::InvalidState; }
	out.value = impl->next_pipeline_id++;
	compute.pipelines.Add(out.value, pick(p));
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::DestroyComputePipeline(GpuPipelineId id)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	if(impl->HasOpenCommands()) { impl->error = "compute pipeline is referenced by live commands"; return GpuResult::InvalidState; }
	int i = compute_impl->pipelines.Find(id.value);
	compute_impl->Destroy(compute_impl->pipelines[i]); compute_impl->pipelines.Remove(i);
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::SetComputePipeline(GpuCommandListId list, GpuPipelineId pipeline)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	auto *p = compute_impl ? compute_impl->FindPipeline(pipeline) : nullptr;
	if(ci < 0 || !p) { impl->error = "unknown compute command/pipeline handle"; return GpuResult::InvalidHandle; }
	auto& command = impl->commands[ci];
	if(command.pass_active || command.ended) { impl->error = "compute pipeline requires an open command outside a render pass"; return GpuResult::InvalidState; }
	if(!compute_impl->FindCommand(list) && compute_impl->commands.GetCount() >= 64) {
		impl->error = "compute live-command budget exceeded"; return GpuResult::InvalidState;
	}
	auto& state = compute_impl->GetCommand(list);
	state.pipeline = pipeline; state.parameters = p->desc.parameter_bytes == 0;
	for(auto& range : state.ranges) range = ComputeImpl::Range();
	impl->cmd_bind_pipeline(command.buffer, VK_PIPELINE_BIND_POINT_COMPUTE, p->pipeline);
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::SetStorageBuffer(GpuCommandListId list, int slot, GpuBufferId buffer, int64 offset, int64 size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value), bi = impl->buffers.Find(buffer.value);
	if(ci < 0 || bi < 0) { impl->error = "unknown compute command/buffer handle"; return GpuResult::InvalidHandle; }
	auto *state = compute_impl ? compute_impl->FindCommand(list) : nullptr;
	auto *p = state ? compute_impl->FindPipeline(state->pipeline) : nullptr;
	if(!p || impl->commands[ci].pass_active || impl->commands[ci].ended) {
		impl->error = "storage binding requires an open compute command"; return GpuResult::InvalidState;
	}
	const auto& desc = impl->buffers[bi].desc;
	if(slot < 0 || slot >= p->desc.storage_buffer_count || !(desc.usage & GpuBufferUsage_Storage) ||
	   offset < 0 || offset >= desc.size || size < 0) {
		impl->error = "invalid compute storage binding"; return GpuResult::InvalidArgument;
	}
	if(size == 0) size = desc.size - offset;
	const auto& caps = compute_impl->caps;
	if(size > desc.size - offset || (uint64)size > caps.max_storage_range ||
	   (uint64)offset % caps.storage_offset_alignment) {
		impl->error = "compute storage range exceeds bounds/alignment"; return GpuResult::InvalidArgument;
	}
	state->ranges[slot].buffer = buffer; state->ranges[slot].offset = offset; state->ranges[slot].size = size;
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::SetComputeParameters(GpuCommandListId list, const void *data, int size)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(ci < 0) { impl->error = "unknown compute command"; return GpuResult::InvalidHandle; }
	auto *state = compute_impl ? compute_impl->FindCommand(list) : nullptr;
	auto *p = state ? compute_impl->FindPipeline(state->pipeline) : nullptr;
	if(!p || impl->commands[ci].pass_active || impl->commands[ci].ended) {
		impl->error = "parameters require an open compute command"; return GpuResult::InvalidState;
	}
	if(size != p->desc.parameter_bytes || !size || !data) { impl->error = "compute parameters require the complete declared payload"; return GpuResult::InvalidArgument; }
	compute_impl->push_constants(impl->commands[ci].buffer, p->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, data);
	state->parameters = true;
	return GpuResult::Ok;
}
GpuResult VulkanGpuDevice::Dispatch(GpuCommandListId list, uint32 x, uint32 y, uint32 z)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value);
	if(ci < 0) { impl->error = "unknown compute command"; return GpuResult::InvalidHandle; }
	auto *state = compute_impl ? compute_impl->FindCommand(list) : nullptr;
	auto *p = state ? compute_impl->FindPipeline(state->pipeline) : nullptr;
	if(!p || impl->commands[ci].pass_active || impl->commands[ci].ended || !state->parameters) {
		impl->error = "dispatch requires an open compute pipeline and parameters"; return GpuResult::InvalidState;
	}
	const auto& caps = compute_impl->caps;
	if(!x || !y || !z || x > caps.max_group_count[0] || y > caps.max_group_count[1] || z > caps.max_group_count[2]) {
		impl->error = "compute workgroup count exceeds device limits"; return GpuResult::InvalidArgument;
	}
	if(state->dispatches >= (int)caps.max_dispatches_per_command) {
		impl->error = "compute dispatch budget exceeded"; return GpuResult::InvalidState;
	}
	VkDescriptorBufferInfo buffers[4] {};
	VkWriteDescriptorSet writes[4] {};
	for(int i = 0; i < p->desc.storage_buffer_count; ++i) {
		const auto& range = state->ranges[i];
		int bi = impl->buffers.Find(range.buffer.value);
		if(bi < 0 || !range.size) { impl->error = "compute dispatch has an unbound storage buffer"; return GpuResult::InvalidState; }
		buffers[i].buffer = impl->buffers[bi].raw.buffer; buffers[i].offset = range.offset; buffers[i].range = range.size;
	}
	VkDescriptorSet set = VK_NULL_HANDLE;
	if(!compute_impl->Allocate(*state, p->descriptors, set)) return GpuResult::InvalidState;
	for(int i = 0; i < p->desc.storage_buffer_count; ++i) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = set; writes[i].dstBinding = i;
		writes[i].descriptorCount = 1; writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &buffers[i];
	}
	compute_impl->update_sets(impl->device, p->desc.storage_buffer_count, writes, 0, nullptr);
	compute_impl->bind_sets(impl->commands[ci].buffer, VK_PIPELINE_BIND_POINT_COMPUTE, p->layout, 0, 1, &set, 0, nullptr);
	VkMemoryBarrier barrier {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	impl->cmd_pipeline_barrier(impl->commands[ci].buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT,
	                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	compute_impl->dispatch(impl->commands[ci].buffer, x, y, z);
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
	impl->cmd_pipeline_barrier(impl->commands[ci].buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	++state->dispatches;
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::CopyBufferToTexture(GpuCommandListId list, GpuBufferId source, int64 offset,
                                              GpuTextureId target, const GpuTextureWriteDesc& region)
{
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int ci = impl->commands.Find(list.value), bi = impl->buffers.Find(source.value), ti = impl->textures.Find(target.value);
	if(ci < 0 || bi < 0 || ti < 0) { impl->error = "unknown GPU image-copy handle"; return GpuResult::InvalidHandle; }
	auto& command = impl->commands[ci]; auto& texture = impl->textures[ti]; auto& buffer = impl->buffers[bi];
	if(command.pass_active || command.ended || texture.immutable || texture.borrowed_swapchain) {
		impl->error = "GPU image copy requires an open command and mutable adapter-owned texture"; return GpuResult::InvalidState;
	}
	if(!(buffer.desc.usage & GpuBufferUsage_TransferSrc) || !(texture.desc.usage & GpuTextureUsage_TransferDst) ||
	   IsDepthFormat(texture.desc.format)) {
		impl->error = "GPU image-copy usage/format is unsupported"; return GpuResult::InvalidArgument;
	}
	const int bpp = BytesPerPixel(texture.desc.format);
	if(offset < 0 || offset > buffer.desc.size || offset % 4 || offset % bpp ||
	   region.origin.x < 0 || region.origin.y < 0 || region.size.cx <= 0 || region.size.cy <= 0 ||
	   region.size.cx > texture.desc.size.cx || region.size.cy > texture.desc.size.cy ||
	   region.origin.x > texture.desc.size.cx - region.size.cx || region.origin.y > texture.desc.size.cy - region.size.cy) {
		impl->error = "GPU image-copy region/offset is invalid"; return GpuResult::InvalidArgument;
	}
	const int64 tight = (int64)region.size.cx * bpp;
	if(region.row_pitch < tight || region.row_pitch % bpp || region.row_pitch / bpp > UINT32_MAX ||
	   (region.size.cy > 1 && region.row_pitch > (std::numeric_limits<int64>::max() - tight) / (region.size.cy - 1))) {
		impl->error = "GPU image-copy row layout overflows or is misaligned"; return GpuResult::InvalidArgument;
	}
	const int64 bytes = region.row_pitch * (region.size.cy - 1) + tight;
	if(bytes > buffer.desc.size - offset) { impl->error = "GPU image copy exceeds source buffer"; return GpuResult::InvalidArgument; }
	int li = command.texture_layouts.Find(target.value), ii = command.texture_initialized.Find(target.value);
	const VkImageLayout old = li >= 0 ? command.texture_layouts[li] : texture.layout;
	const bool initialized = ii >= 0 ? command.texture_initialized[ii] : texture.initialized;
	if(!initialized && (region.origin != Point(0, 0) || region.size != texture.desc.size)) {
		impl->error = "first GPU image copy must initialize the entire texture"; return GpuResult::InvalidState;
	}
	VkMemoryBarrier memory {};
	memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memory.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT;
	memory.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	impl->cmd_pipeline_barrier(command.buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT,
	                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memory, 0, nullptr, 0, nullptr);
	VkImageMemoryBarrier2 barrier {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = Stage2ForLayout(old); barrier.srcAccessMask = Access2ForLayout(old);
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.oldLayout = old; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = texture.image; barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
	VkDependencyInfo dep {};
	dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO; dep.imageMemoryBarrierCount = 1; dep.pImageMemoryBarriers = &barrier;
	impl->cmd_pipeline_barrier_2(command.buffer, &dep);
	VkBufferImageCopy copy {};
	copy.bufferOffset = offset; copy.bufferRowLength = (uint32_t)(region.row_pitch / bpp);
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; copy.imageSubresource.layerCount = 1;
	copy.imageOffset = {region.origin.x, region.origin.y, 0};
	copy.imageExtent = {(uint32_t)region.size.cx, (uint32_t)region.size.cy, 1};
	impl->cmd_copy_buffer_to_image(command.buffer, buffer.raw.buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	const VkImageLayout final = FinalTextureLayout(texture.desc.usage);
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barrier.dstStageMask = Stage2ForLayout(final); barrier.dstAccessMask = Access2ForLayout(final);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = final;
	impl->cmd_pipeline_barrier_2(command.buffer, &dep);
	if(li >= 0) command.texture_layouts[li] = final; else command.texture_layouts.Add(target.value, final);
	if(ii >= 0) command.texture_initialized[ii] = true; else command.texture_initialized.Add(target.value, true);
	return GpuResult::Ok;
}

GpuResult VulkanGpuDevice::ReadBuffer(GpuBufferId buffer, int64 offset, int64 size, Vector<byte>& out)
{
	out.Clear();
	if(!impl->CheckReady()) return GpuResult::InvalidState;
	int bi = impl->buffers.Find(buffer.value);
	if(bi < 0) { impl->error = "unknown readback buffer"; return GpuResult::InvalidHandle; }
	const auto& state = impl->buffers[bi];
	if(state.desc.memory == GpuBufferMemory::DeviceLocal) {
		impl->error = "ReadBuffer cannot map a device-local buffer"; return GpuResult::InvalidState;
	}
	if(impl->HasOpenCommands()) { impl->error = "buffer readback requires no live commands"; return GpuResult::InvalidState; }
	if(!(state.desc.usage & GpuBufferUsage_TransferSrc) || offset < 0 || size <= 0 || size > 64 * 1024 * 1024 ||
	   offset > state.desc.size || size > state.desc.size - offset) {
		impl->error = "invalid TransferSrc buffer readback range"; return GpuResult::InvalidArgument;
	}
	auto& compute = Compute();
	if(!compute.Query()) return GpuResult::Unsupported;
	VkResult vr = impl->queue_wait_idle(impl->graphics_queue);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkQueueWaitIdle(read buffer)", vr); return GpuResult::InvalidState; }
	void *mapped = nullptr;
	vr = impl->map_memory(impl->device, state.raw.memory, 0, VK_WHOLE_SIZE, 0, &mapped);
	if(vr != VK_SUCCESS || !mapped) { impl->error = VkFailure("vkMapMemory(read buffer)", vr); return GpuResult::InvalidState; }
	if(!(state.raw.memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
		VkMappedMemoryRange range {};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; range.memory = state.raw.memory; range.size = VK_WHOLE_SIZE;
		vr = compute.invalidate(impl->device, 1, &range);
	}
	if(vr == VK_SUCCESS) {
		out.SetCount((int)size);
		std::memcpy(out.Begin(), static_cast<const byte *>(mapped) + offset, (size_t)size);
	}
	impl->unmap_memory(impl->device, state.raw.memory);
	if(vr != VK_SUCCESS) { impl->error = VkFailure("vkInvalidateMappedMemoryRanges", vr); return GpuResult::InvalidState; }
	return GpuResult::Ok;
}
}
