from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'{path}: expected one match, found {count}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')

replace_once(
    'render/RenderVulkan/RenderVulkanSurfaceSession.h',
    '''\tstruct FrameInterop {\n\t\tVkDevice device = VK_NULL_HANDLE;\n\t\tVkQueue graphics_queue = VK_NULL_HANDLE;''',
    '''\tstruct FrameInterop {\n\t\tVkInstance instance = VK_NULL_HANDLE;\n\t\tVkPhysicalDevice physical_device = VK_NULL_HANDLE;\n\t\tVkDevice device = VK_NULL_HANDLE;\n\t\tVkQueue graphics_queue = VK_NULL_HANDLE;''')
replace_once(
    'render/RenderVulkan/RenderVulkanSurfaceSession.h',
    '''\t\tPFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;\n\t\tVulkanProcResolver proc_filter = nullptr;''',
    '''\t\tPFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;\n\t\tPFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;\n\t\tVulkanProcResolver proc_filter = nullptr;''')

replace_once(
    'render/RenderVulkan/RenderVulkan.cpp',
    '''\tout.device = VK_NULL_HANDLE;\n\tout.graphics_queue = VK_NULL_HANDLE;''',
    '''\tout.instance = VK_NULL_HANDLE;\n\tout.physical_device = VK_NULL_HANDLE;\n\tout.device = VK_NULL_HANDLE;\n\tout.graphics_queue = VK_NULL_HANDLE;''')
replace_once(
    'render/RenderVulkan/RenderVulkan.cpp',
    '''\tout.get_device_proc_addr = nullptr;\n\tout.proc_filter = nullptr;''',
    '''\tout.get_instance_proc_addr = nullptr;\n\tout.get_device_proc_addr = nullptr;\n\tout.proc_filter = nullptr;''')
replace_once(
    'render/RenderVulkan/RenderVulkan.cpp',
    '''\tout.device = impl->device.device;\n\tout.graphics_queue = impl->device.graphics_queue;''',
    '''\tout.instance = owner->instance.instance;\n\tout.physical_device = impl->device.physical_device;\n\tout.device = impl->device.device;\n\tout.graphics_queue = impl->device.graphics_queue;''')
replace_once(
    'render/RenderVulkan/RenderVulkan.cpp',
    '''\tout.get_device_proc_addr = owner->instance.get_device_proc_addr;\n\tout.proc_filter = owner->dispatch.proc_filter;''',
    '''\tout.get_instance_proc_addr = owner->dispatch.get_instance_proc_addr;\n\tout.get_device_proc_addr = owner->instance.get_device_proc_addr;\n\tout.proc_filter = owner->dispatch.proc_filter;''')

replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tVulkanSurfaceSession *session = nullptr;\n\tVkDevice device = VK_NULL_HANDLE;''',
    '''\tVulkanSurfaceSession *session = nullptr;\n\tVkInstance instance = VK_NULL_HANDLE;\n\tVkPhysicalDevice physical_device = VK_NULL_HANDLE;\n\tVkDevice device = VK_NULL_HANDLE;''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tPFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;\n\tVulkanProcResolver proc_filter = nullptr;''',
    '''\tPFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;\n\tPFN_vkGetDeviceProcAddr get_device_proc_addr = nullptr;\n\tVulkanProcResolver proc_filter = nullptr;''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tPFN_vkCreateBuffer create_buffer = nullptr;''',
    '''\tPFN_vkGetPhysicalDeviceMemoryProperties get_physical_device_memory_properties = nullptr;\n\tVkPhysicalDeviceMemoryProperties memory_properties {};\n\n\tPFN_vkCreateBuffer create_buffer = nullptr;''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\ttemplate <class T>\n\tbool Resolve(T& out, const char *name)\n\t{''',
    '''\ttemplate <class T>\n\tbool ResolveInstance(T& out, const char *name)\n\t{\n\t\tif(proc_filter && !proc_filter(nullptr, name)) {\n\t\t\terror = String("missing Vulkan procedure: ") + name;\n\t\t\treturn false;\n\t\t}\n\t\tout = reinterpret_cast<T>(get_instance_proc_addr(instance, name));\n\t\tif(!out) {\n\t\t\terror = String("missing Vulkan procedure: ") + name;\n\t\t\treturn false;\n\t\t}\n\t\treturn true;\n\t}\n\n\ttemplate <class T>\n\tbool Resolve(T& out, const char *name)\n\t{''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tbool ResolveResourceDispatch()\n\t{\n\t\treturn Resolve(create_buffer, "vkCreateBuffer") &&''',
    '''\tbool ResolveResourceDispatch()\n\t{\n\t\tif(!ResolveInstance(get_physical_device_memory_properties, "vkGetPhysicalDeviceMemoryProperties"))\n\t\t\treturn false;\n\t\tget_physical_device_memory_properties(physical_device, &memory_properties);\n\t\treturn Resolve(create_buffer, "vkCreateBuffer") &&''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tbool CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage, RawBuffer& out)\n\t{''',
    '''\tint FindMemoryType(uint32_t bits, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, VkMemoryPropertyFlags *out_flags = nullptr) const\n\t{\n\t\tauto find = [&](VkMemoryPropertyFlags wanted) -> int {\n\t\t\tfor(uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {\n\t\t\t\tif((bits & (1u << i)) == 0)\n\t\t\t\t\tcontinue;\n\t\t\t\tconst VkMemoryPropertyFlags flags = memory_properties.memoryTypes[i].propertyFlags;\n\t\t\t\tif((flags & wanted) == wanted) {\n\t\t\t\t\tif(out_flags)\n\t\t\t\t\t\t*out_flags = flags;\n\t\t\t\t\treturn (int)i;\n\t\t\t\t}\n\t\t\t}\n\t\t\treturn -1;\n\t\t};\n\t\tint index = find(required | preferred);\n\t\tif(index < 0)\n\t\t\tindex = find(required);\n\t\treturn index;\n\t}\n\n\tbool CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage, RawBuffer& out)\n\t{''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tstruct RawBuffer : Moveable<RawBuffer> {\n\t\tVkBuffer buffer = VK_NULL_HANDLE;\n\t\tVkDeviceMemory memory = VK_NULL_HANDLE;\n\t\tVkDeviceSize allocation_size = 0;\n\t};''',
    '''\tstruct RawBuffer : Moveable<RawBuffer> {\n\t\tVkBuffer buffer = VK_NULL_HANDLE;\n\t\tVkDeviceMemory memory = VK_NULL_HANDLE;\n\t\tVkDeviceSize allocation_size = 0;\n\t\tVkMemoryPropertyFlags memory_flags = 0;\n\t};''')

old_buffer_alloc = '''\t\tVkMemoryRequirements requirements {};\n\t\tget_buffer_memory_requirements(device, out.buffer, &requirements);\n\t\tfor(uint32_t memory_type = 0; memory_type < 32; ++memory_type) {\n\t\t\tif((requirements.memoryTypeBits & (1u << memory_type)) == 0)\n\t\t\t\tcontinue;\n\t\t\tVkMemoryAllocateInfo mai {};\n\t\t\tmai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;\n\t\t\tmai.allocationSize = requirements.size;\n\t\t\tmai.memoryTypeIndex = memory_type;\n\t\t\tVkDeviceMemory memory = VK_NULL_HANDLE;\n\t\t\tvr = allocate_memory(device, &mai, nullptr, &memory);\n\t\t\tif(vr != VK_SUCCESS)\n\t\t\t\tcontinue;\n\t\t\tvoid *mapped = nullptr;\n\t\t\tVkResult map_result = map_memory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);\n\t\t\tif(map_result == VK_SUCCESS && mapped) {\n\t\t\t\tunmap_memory(device, memory);\n\t\t\t\tvr = bind_buffer_memory(device, out.buffer, memory, 0);\n\t\t\t\tif(vr == VK_SUCCESS) {\n\t\t\t\t\tout.memory = memory;\n\t\t\t\t\tout.allocation_size = requirements.size;\n\t\t\t\t\treturn true;\n\t\t\t\t}\n\t\t\t}\n\t\t\tfree_memory(device, memory, nullptr);\n\t\t}\n\t\tdestroy_buffer(device, out.buffer, nullptr);\n\t\tout.buffer = VK_NULL_HANDLE;\n\t\terror = "no compatible host-visible Vulkan memory type for buffer";\n\t\treturn false;'''
new_buffer_alloc = '''\t\tVkMemoryRequirements requirements {};\n\t\tget_buffer_memory_requirements(device, out.buffer, &requirements);\n\t\tVkMemoryPropertyFlags memory_flags = 0;\n\t\tconst int memory_type = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,\n\t\t                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_flags);\n\t\tif(memory_type < 0) {\n\t\t\tdestroy_buffer(device, out.buffer, nullptr);\n\t\t\tout.buffer = VK_NULL_HANDLE;\n\t\t\terror = "no compatible host-visible Vulkan memory type for buffer";\n\t\t\treturn false;\n\t\t}\n\t\tVkMemoryAllocateInfo mai {};\n\t\tmai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;\n\t\tmai.allocationSize = requirements.size;\n\t\tmai.memoryTypeIndex = (uint32_t)memory_type;\n\t\tvr = allocate_memory(device, &mai, nullptr, &out.memory);\n\t\tif(vr != VK_SUCCESS) {\n\t\t\tdestroy_buffer(device, out.buffer, nullptr);\n\t\t\tout.buffer = VK_NULL_HANDLE;\n\t\t\terror = VkFailure("vkAllocateMemory", vr);\n\t\t\treturn false;\n\t\t}\n\t\tvr = bind_buffer_memory(device, out.buffer, out.memory, 0);\n\t\tif(vr != VK_SUCCESS) {\n\t\t\tfree_memory(device, out.memory, nullptr);\n\t\t\tdestroy_buffer(device, out.buffer, nullptr);\n\t\t\tout = RawBuffer();\n\t\t\terror = VkFailure("vkBindBufferMemory", vr);\n\t\t\treturn false;\n\t\t}\n\t\tout.allocation_size = requirements.size;\n\t\tout.memory_flags = memory_flags;\n\t\treturn true;'''
replace_once('render/RenderVulkan/RenderVulkanRhi.cpp', old_buffer_alloc, new_buffer_alloc)

replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\t\tVkMappedMemoryRange range {};\n\t\trange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;\n\t\trange.memory = raw.memory;\n\t\trange.offset = 0;\n\t\trange.size = VK_WHOLE_SIZE;\n\t\tvr = flush_mapped_memory_ranges(device, 1, &range);\n\t\tunmap_memory(device, raw.memory);\n\t\tif(vr != VK_SUCCESS) {\n\t\t\terror = VkFailure("vkFlushMappedMemoryRanges", vr);\n\t\t\treturn false;\n\t\t}\n\t\treturn true;''',
    '''\t\tif((raw.memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {\n\t\t\tVkMappedMemoryRange range {};\n\t\t\trange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;\n\t\t\trange.memory = raw.memory;\n\t\t\trange.offset = 0;\n\t\t\trange.size = VK_WHOLE_SIZE;\n\t\t\tvr = flush_mapped_memory_ranges(device, 1, &range);\n\t\t\tif(vr != VK_SUCCESS) {\n\t\t\t\tunmap_memory(device, raw.memory);\n\t\t\t\terror = VkFailure("vkFlushMappedMemoryRanges", vr);\n\t\t\t\treturn false;\n\t\t\t}\n\t\t}\n\t\tunmap_memory(device, raw.memory);\n\t\treturn true;''')

old_image_alloc = '''\t\tVkMemoryRequirements requirements {};\n\t\tget_image_memory_requirements(device, image, &requirements);\n\t\tfor(uint32_t memory_type = 0; memory_type < 32; ++memory_type) {\n\t\t\tif((requirements.memoryTypeBits & (1u << memory_type)) == 0)\n\t\t\t\tcontinue;\n\t\t\tVkMemoryAllocateInfo mai {};\n\t\t\tmai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;\n\t\t\tmai.allocationSize = requirements.size;\n\t\t\tmai.memoryTypeIndex = memory_type;\n\t\t\tVkDeviceMemory memory = VK_NULL_HANDLE;\n\t\t\tVkResult vr = allocate_memory(device, &mai, nullptr, &memory);\n\t\t\tif(vr != VK_SUCCESS)\n\t\t\t\tcontinue;\n\t\t\tvr = bind_image_memory(device, image, memory, 0);\n\t\t\tif(vr == VK_SUCCESS) {\n\t\t\t\tout_memory = memory;\n\t\t\t\treturn true;\n\t\t\t}\n\t\t\tfree_memory(device, memory, nullptr);\n\t\t}\n\t\terror = "no compatible Vulkan memory type for texture";\n\t\treturn false;'''
new_image_alloc = '''\t\tVkMemoryRequirements requirements {};\n\t\tget_image_memory_requirements(device, image, &requirements);\n\t\tconst int memory_type = FindMemoryType(requirements.memoryTypeBits, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);\n\t\tif(memory_type < 0) {\n\t\t\terror = "no compatible Vulkan memory type for texture";\n\t\t\treturn false;\n\t\t}\n\t\tVkMemoryAllocateInfo mai {};\n\t\tmai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;\n\t\tmai.allocationSize = requirements.size;\n\t\tmai.memoryTypeIndex = (uint32_t)memory_type;\n\t\tVkResult vr = allocate_memory(device, &mai, nullptr, &out_memory);\n\t\tif(vr != VK_SUCCESS) {\n\t\t\terror = VkFailure("vkAllocateMemory", vr);\n\t\t\treturn false;\n\t\t}\n\t\tvr = bind_image_memory(device, image, out_memory, 0);\n\t\tif(vr != VK_SUCCESS) {\n\t\t\tfree_memory(device, out_memory, nullptr);\n\t\t\tout_memory = VK_NULL_HANDLE;\n\t\t\terror = VkFailure("vkBindImageMemory", vr);\n\t\t\treturn false;\n\t\t}\n\t\treturn true;'''
replace_once('render/RenderVulkan/RenderVulkanRhi.cpp', old_image_alloc, new_image_alloc)

replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\timpl->device = interop.device;\n\timpl->graphics_queue = interop.graphics_queue;''',
    '''\timpl->instance = interop.instance;\n\timpl->physical_device = interop.physical_device;\n\timpl->device = interop.device;\n\timpl->graphics_queue = interop.graphics_queue;''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\timpl->get_device_proc_addr = interop.get_device_proc_addr;\n\timpl->proc_filter = interop.proc_filter;\n\tif(!impl->device || !impl->graphics_queue || !impl->get_device_proc_addr) {''',
    '''\timpl->get_instance_proc_addr = interop.get_instance_proc_addr;\n\timpl->get_device_proc_addr = interop.get_device_proc_addr;\n\timpl->proc_filter = interop.proc_filter;\n\tif(!impl->instance || !impl->physical_device || !impl->device || !impl->graphics_queue ||\n\t   !impl->get_instance_proc_addr || !impl->get_device_proc_addr) {''')
replace_once(
    'render/RenderVulkan/RenderVulkanRhi.cpp',
    '''\tif(desc.size <= 0)\n\t\treturn impl->error = "CreateBuffer requires positive size", GpuResult::InvalidArgument;''',
    '''\tif(desc.size <= 0) {\n\t\timpl->error = "CreateBuffer requires positive size";\n\t\treturn GpuResult::InvalidArgument;\n\t}''')

print('S17B memory-property patch applied')
