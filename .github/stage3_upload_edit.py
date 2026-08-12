from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one replacement target, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "render/RenderRhi/RenderRhi.h",
    '''struct GpuTextureDesc : Moveable<GpuTextureDesc> {\n\tSize size = Size(0, 0);\n\tGpuFormat format = GpuFormat::Unknown;\n\tint usage = GpuTextureUsage_None;\n\tString label;\n};\n\nstruct GpuRenderPassDesc''',
    '''struct GpuTextureDesc : Moveable<GpuTextureDesc> {\n\tSize size = Size(0, 0);\n\tGpuFormat format = GpuFormat::Unknown;\n\tint usage = GpuTextureUsage_None;\n\tString label;\n};\n\nstruct GpuTextureWriteDesc : Moveable<GpuTextureWriteDesc> {\n\tPoint origin = Point(0, 0);\n\tSize size = Size(0, 0);\n\tint64 row_pitch = 0;\n};\n\nstruct GpuRenderPassDesc''')

replace_once(
    "render/RenderRhi/RenderRhi.h",
    '''\tvirtual GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) = 0;\n\tvirtual GpuResult DestroyBuffer(GpuBufferId id) = 0;\n\n\tvirtual GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) = 0;\n\tvirtual GpuResult DestroyTexture(GpuTextureId id) = 0;''',
    '''\tvirtual GpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) = 0;\n\tvirtual GpuResult WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size) = 0;\n\tvirtual GpuResult DestroyBuffer(GpuBufferId id) = 0;\n\n\tvirtual GpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) = 0;\n\tvirtual GpuResult WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size) = 0;\n\tvirtual GpuResult DestroyTexture(GpuTextureId id) = 0;''')

replace_once(
    "render/RenderNull/RenderNull.h",
    '''\tGpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) override;\n\tGpuResult DestroyBuffer(GpuBufferId id) override;\n\n\tGpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) override;\n\tGpuResult DestroyTexture(GpuTextureId id) override;''',
    '''\tGpuResult CreateBuffer(const GpuBufferDesc& desc, GpuBufferId& out) override;\n\tGpuResult WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size) override;\n\tGpuResult DestroyBuffer(GpuBufferId id) override;\n\n\tGpuResult CreateTexture(const GpuTextureDesc& desc, GpuTextureId& out) override;\n\tGpuResult WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size) override;\n\tGpuResult DestroyTexture(GpuTextureId id) override;''')

replace_once(
    "render/RenderNull/RenderNull.cpp",
    '''static bool ValidateNativeWindowDesc(const GpuSurfaceDesc& desc, String& reason)\n{''',
    '''static int BytesPerPixel(GpuFormat format)\n{\n\tswitch(format) {\n\tcase GpuFormat::RGBA8:\n\tcase GpuFormat::BGRA8:\n\tcase GpuFormat::D24S8:\n\t\treturn 4;\n\tcase GpuFormat::R16F:\n\t\treturn 2;\n\tcase GpuFormat::Unknown:\n\t\treturn 0;\n\t}\n\treturn 0;\n}\n\nstatic bool ValidateNativeWindowDesc(const GpuSurfaceDesc& desc, String& reason)\n{''')

replace_once(
    "render/RenderNull/RenderNull.cpp",
    '''GpuResult NullGpuDevice::DestroyBuffer(GpuBufferId id)\n{''',
    '''GpuResult NullGpuDevice::WriteBuffer(GpuBufferId id, int64 offset, const void *data, int64 size)\n{\n\tint index = buffers.Find(id.value);\n\tif(!id.IsValid() || index < 0) {\n\t\tFail("WriteBuffer id=" + id.Dump() + " reason=unknown");\n\t\treturn GpuResult::InvalidHandle;\n\t}\n\tif(offset < 0 || size <= 0 || data == nullptr) {\n\t\tFail("WriteBuffer id=" + id.Dump() + " offset=" + AsString(offset) + " size=" + AsString(size) + " reason=invalid_write");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tconst int64 buffer_size = buffers[index].desc.size;\n\tif(offset > buffer_size || size > buffer_size - offset) {\n\t\tFail("WriteBuffer id=" + id.Dump() + " offset=" + AsString(offset) + " size=" + AsString(size) + " reason=out_of_range");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tAppendLog("WriteBuffer id=" + id.Dump() + " offset=" + AsString(offset) + " size=" + AsString(size));\n\treturn GpuResult::Ok;\n}\n\nGpuResult NullGpuDevice::DestroyBuffer(GpuBufferId id)\n{''')

replace_once(
    "render/RenderNull/RenderNull.cpp",
    '''GpuResult NullGpuDevice::DestroyTexture(GpuTextureId id)\n{''',
    '''GpuResult NullGpuDevice::WriteTexture(GpuTextureId id, const GpuTextureWriteDesc& desc, const void *data, int64 data_size)\n{\n\tint index = textures.Find(id.value);\n\tif(!id.IsValid() || index < 0) {\n\t\tFail("WriteTexture id=" + id.Dump() + " reason=unknown");\n\t\treturn GpuResult::InvalidHandle;\n\t}\n\tconst TextureState& state = textures[index];\n\tif(state.swapchain_backbuffer) {\n\t\tFail("WriteTexture id=" + id.Dump() + " reason=swapchain_backbuffer");\n\t\treturn GpuResult::InvalidState;\n\t}\n\tif(data == nullptr || data_size <= 0 || desc.origin.x < 0 || desc.origin.y < 0 ||\n\t   desc.size.cx <= 0 || desc.size.cy <= 0 || desc.row_pitch <= 0) {\n\t\tFail("WriteTexture id=" + id.Dump() + " reason=invalid_write");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tif(desc.origin.x > state.desc.size.cx - desc.size.cx || desc.origin.y > state.desc.size.cy - desc.size.cy) {\n\t\tFail("WriteTexture id=" + id.Dump() + " reason=out_of_range");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tconst int bytes_per_pixel = BytesPerPixel(state.desc.format);\n\tif(bytes_per_pixel <= 0) {\n\t\tFail("WriteTexture id=" + id.Dump() + " reason=unsupported_format");\n\t\treturn GpuResult::Unsupported;\n\t}\n\tconst int64 tight_row = (int64)desc.size.cx * bytes_per_pixel;\n\tif(desc.row_pitch < tight_row) {\n\t\tFail("WriteTexture id=" + id.Dump() + " row_pitch=" + AsString(desc.row_pitch) + " reason=row_pitch_too_small");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tif(desc.size.cy > 1 && desc.row_pitch > (INT64_MAX - tight_row) / (desc.size.cy - 1)) {\n\t\tFail("WriteTexture id=" + id.Dump() + " reason=layout_overflow");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tconst int64 required_size = desc.row_pitch * (desc.size.cy - 1) + tight_row;\n\tif(data_size < required_size) {\n\t\tFail("WriteTexture id=" + id.Dump() + " data_size=" + AsString(data_size) + " required=" + AsString(required_size) + " reason=data_too_small");\n\t\treturn GpuResult::InvalidArgument;\n\t}\n\tAppendLog("WriteTexture id=" + id.Dump() + " origin=" + AsString(desc.origin.x) + "," + AsString(desc.origin.y) +\n\t          " size=" + AsString(desc.size.cx) + "x" + AsString(desc.size.cy) + " row_pitch=" + AsString(desc.row_pitch));\n\treturn GpuResult::Ok;\n}\n\nGpuResult NullGpuDevice::DestroyTexture(GpuTextureId id)\n{''')

insert_before = '''static bool TestPipelineLifecycle(NullGpuDevice& device)\n{'''
resource_test = r'''static bool TestResourceUploads(NullGpuDevice& device)
{
	byte buffer_data[16] = {};
	GpuBufferId buffer;
	if(!Check(device.CreateBuffer(MakeVertexBufferDesc(16), buffer) == GpuResult::Ok, "upload buffer should create")) return false;
	if(!Check(device.WriteBuffer(buffer, 4, buffer_data, 8) == GpuResult::Ok, "in-range buffer write should succeed")) return false;
	if(!Check(device.WriteBuffer(buffer, 0, nullptr, 4) == GpuResult::InvalidArgument, "null buffer write should fail")) return false;
	if(!Check(device.WriteBuffer(buffer, -1, buffer_data, 4) == GpuResult::InvalidArgument, "negative buffer offset should fail")) return false;
	if(!Check(device.WriteBuffer(buffer, 0, buffer_data, 0) == GpuResult::InvalidArgument, "zero buffer write should fail")) return false;
	if(!Check(device.WriteBuffer(buffer, 12, buffer_data, 8) == GpuResult::InvalidArgument, "out-of-range buffer write should fail")) return false;
	GpuBufferId unknown_buffer;
	unknown_buffer.value = 999;
	if(!Check(device.WriteBuffer(unknown_buffer, 0, buffer_data, 4) == GpuResult::InvalidHandle, "unknown buffer write should fail")) return false;
	if(!Check(device.DestroyBuffer(buffer) == GpuResult::Ok, "upload buffer should destroy")) return false;
	if(!Check(device.WriteBuffer(buffer, 0, buffer_data, 4) == GpuResult::InvalidHandle, "destroyed buffer write should fail")) return false;

	GpuTextureDesc texture_desc = MakeTextureDesc(Size(4, 3));
	texture_desc.usage = GpuTextureUsage_Sampled;
	GpuTextureId texture;
	if(!Check(device.CreateTexture(texture_desc, texture) == GpuResult::Ok, "upload texture should create")) return false;
	byte texture_data[64] = {};
	GpuTextureWriteDesc whole;
	whole.size = Size(4, 3);
	whole.row_pitch = 16;
	if(!Check(device.WriteTexture(texture, whole, texture_data, 48) == GpuResult::Ok, "tight texture upload should succeed")) return false;
	GpuTextureWriteDesc partial;
	partial.origin = Point(1, 1);
	partial.size = Size(2, 2);
	partial.row_pitch = 12;
	if(!Check(device.WriteTexture(texture, partial, texture_data, 20) == GpuResult::Ok, "padded partial texture upload should succeed")) return false;
	GpuTextureWriteDesc short_pitch = partial;
	short_pitch.row_pitch = 7;
	if(!Check(device.WriteTexture(texture, short_pitch, texture_data, 20) == GpuResult::InvalidArgument, "short texture row pitch should fail")) return false;
	if(!Check(device.WriteTexture(texture, partial, texture_data, 19) == GpuResult::InvalidArgument, "short texture data should fail")) return false;
	GpuTextureWriteDesc outside = partial;
	outside.origin = Point(3, 2);
	if(!Check(device.WriteTexture(texture, outside, texture_data, 20) == GpuResult::InvalidArgument, "out-of-range texture upload should fail")) return false;
	GpuTextureWriteDesc negative = partial;
	negative.origin = Point(-1, 0);
	if(!Check(device.WriteTexture(texture, negative, texture_data, 20) == GpuResult::InvalidArgument, "negative texture origin should fail")) return false;
	if(!Check(device.WriteTexture(texture, partial, nullptr, 20) == GpuResult::InvalidArgument, "null texture upload should fail")) return false;
	GpuTextureId unknown_texture;
	unknown_texture.value = 999;
	if(!Check(device.WriteTexture(unknown_texture, partial, texture_data, 20) == GpuResult::InvalidHandle, "unknown texture upload should fail")) return false;
	if(!Check(device.DestroyTexture(texture) == GpuResult::Ok, "upload texture should destroy")) return false;
	if(!Check(device.WriteTexture(texture, partial, texture_data, 20) == GpuResult::InvalidHandle, "destroyed texture upload should fail")) return false;

	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	if(!Check(device.CreateSurface(MakeSurfaceDesc(Size(32, 32)), surface) == GpuResult::Ok, "upload guard surface should create")) return false;
	if(!Check(device.CreateSwapchain(MakeSwapchainDesc(surface, Size(32, 32)), swapchain) == GpuResult::Ok, "upload guard swapchain should create")) return false;
	GpuFrameInfo frame;
	if(!Check(device.BeginFrame(swapchain, frame) == GpuResult::Ok, "upload guard frame should begin")) return false;
	GpuTextureWriteDesc backbuffer_write;
	backbuffer_write.size = Size(1, 1);
	backbuffer_write.row_pitch = 4;
	if(!Check(device.WriteTexture(frame.color_target, backbuffer_write, texture_data, 4) == GpuResult::InvalidState, "swapchain backbuffer upload should fail")) return false;
	if(!Check(device.Present(frame.frame) == GpuResult::Ok, "upload guard frame should present")) return false;
	if(!Check(device.DestroySwapchain(swapchain) == GpuResult::Ok, "upload guard swapchain should destroy")) return false;
	if(!Check(device.DestroySurface(surface) == GpuResult::Ok, "upload guard surface should destroy")) return false;
	return true;
}

'''
replace_once("tests/RenderRhiTest/main.cpp", insert_before, resource_test + insert_before)

replace_once(
    "tests/RenderRhiTest/main.cpp",
    '''\tok &= TestFrameLifecycle(device);\n\tok &= TestPipelineLifecycle(device);''',
    '''\tok &= TestFrameLifecycle(device);\n\tok &= TestResourceUploads(device);\n\tok &= TestPipelineLifecycle(device);''')

replace_once(
    "docs/PROJECT_PLAN.md",
    '''- S16G adds translation-only ConcatTransform replay for FillRect geometry, scoped by\n  Save/Restore; scale, rotation, shear and general renderer state remain deferred\n- general 2D rendering, shaders, painter callbacks, and shared control device''',
    '''- S16G adds translation-only ConcatTransform replay for FillRect geometry, scoped by\n  Save/Restore; scale, rotation, shear and general renderer state remain deferred\n- S17A aligns the neutral GpuDevice contract with explicit buffer and texture upload\n  operations and makes RenderNull the validation authority for upload range/layout rules\n- general 2D rendering, shaders, painter callbacks, and shared control device''')

active = Path("docs/ACTIVE_WORK.md")
text = active.read_text(encoding="utf-8")
text = text.replace(
    '''## Recovery Log\n\nBASE: `eab3e59ac40be25d6c974224492c7d27c6850cb8`\nTASK: Stage 3 closure analysis / accelerated Vulkan RHI implementation\nTOUCHED: `docs/ACTIVE_WORK.md` only at this checkpoint\nSTATUS: S16G accepted; Stage 3 closure slice being defined\nPUBLISHED: this status checkpoint\nVALIDATION: implementation validation pending for next code slice\n\n## Next Action\n\nInspect the existing RenderNull implementation and current RenderVulkan ownership/session code against every `GpuDevice` method in `RenderRhi.h`. Define the smallest set of larger vertical implementation slices that can close Stage 3 without duplicating the accepted surface/session path, then implement and publish the first slice before Windows validation.''',
    '''## Recovery Log\n\nBASE: `2218e19299987149a47aa7a688cc36ef5e989037`\nTASK: `TASK-008A1-S17A` neutral resource-upload contract\nTOUCHED: `render/RenderRhi/RenderRhi.h`, `render/RenderNull/RenderNull.h`, `render/RenderNull/RenderNull.cpp`, `tests/RenderRhiTest/main.cpp`, `docs/PROJECT_PLAN.md`, `docs/ACTIVE_WORK.md`\nSTATUS: buffer/texture upload contract and Null validation authority implemented\nPUBLISHED: candidate pending guarded publication\nVALIDATION: source guards pending; Windows RenderRhiTest validation pending\n\n## Next Action\n\nAfter S17A Windows acceptance, implement S17B as the first production Vulkan resource slice: Vulkan GpuDevice ownership plus real buffer allocation/write/destruction and texture allocation/write/destruction, reusing accepted instance/device/session ownership rather than duplicating it. Publish that coherent slice before the command/pipeline/draw integration slice.''')
if text == active.read_text(encoding="utf-8"):
    raise SystemExit("docs/ACTIVE_WORK.md: recovery block target not found")
active.write_text(text, encoding="utf-8")

# Static guards against partial interface edits.
rhi = Path("render/RenderRhi/RenderRhi.h").read_text(encoding="utf-8")
null_h = Path("render/RenderNull/RenderNull.h").read_text(encoding="utf-8")
null_cpp = Path("render/RenderNull/RenderNull.cpp").read_text(encoding="utf-8")
test = Path("tests/RenderRhiTest/main.cpp").read_text(encoding="utf-8")
required = [
    ("RenderRhi WriteBuffer", "virtual GpuResult WriteBuffer" in rhi),
    ("RenderRhi WriteTexture", "virtual GpuResult WriteTexture" in rhi),
    ("Null WriteBuffer override", "GpuResult WriteBuffer" in null_h),
    ("Null WriteTexture override", "GpuResult WriteTexture" in null_h),
    ("Null WriteBuffer implementation", "NullGpuDevice::WriteBuffer" in null_cpp),
    ("Null WriteTexture implementation", "NullGpuDevice::WriteTexture" in null_cpp),
    ("upload test", "TestResourceUploads" in test),
    ("backbuffer rejection", "swapchain backbuffer upload should fail" in test),
]
for name, ok in required:
    if not ok:
        raise SystemExit(f"guard failed: {name}")
