# GPU source images and CineView integration

Reconciled 2026-10-06 with CineView's docs/VULKAN_HANDOFF.md. Renderer changes
belong here; CineView retains decoding, source/CPU caches, playback clock,
seek policy, browser policy, colour intent and authoritative CPU sampling.

## Current precision resource contract

GpuFormat includes RGBA8/BGRA8, their explicit sRGB variants, RGBA16 (UNORM),
RGBA16F, RGBA32F, R16F and R32F. New enum values are appended to preserve existing
identities. GpuFormatBytesPerPixel reports native storage width. RGBA16 uses
four unsigned 16-bit channels; F formats use IEEE binary16/binary32 storage.
Channel order is explicit in the format; there is no arbitrary channel mapping.
CineView packs selected layers/channels and data-window offsets before upload.

GpuDevice::GetTextureCapabilities(format, usage, out) queries the actual device
for optimal 2D, one-mip, single-sample images. It reports sampling, linear
filtering, colour attachment/blending, transfer support and dimensions/resource
size for the exact requested usage plus the existing transfer-destination
requirement. Unsupported queries clear output and return Unsupported.
Absent providers do not claim physical support. Unknown usage bits are rejected.
Vulkan queries are cached; unsupported sizes fail before allocation.
The limits are device support limits, not a promise of available VRAM.

CreateTexture/WriteTexture accept native row pitch, dimensions and update origin.
Rows may have padding; the final row need not include trailing padding. Spans,
origins and integer overflow are checked before source access. Upload copies bits:
no alpha premultiplication, tone clipping, sRGB conversion or float reduction.
The sRGB formats explicitly request hardware sRGB sampling semantics; raw HDR
sources use linear formats. Origin is an update offset, not a vertical-flip flag.

AcquireImmutableTexture accepts tight native source pixels, including precision
formats. Content identity is immutable for its lifetime in the shared domain.
Same identity/description shares an allocation, returns independent handles and
reports whether a new upload occurred. Each handle pins its allocation until
DestroyTexture. Separate clip generations/revisions must receive distinct content
identities. This mechanism is for reused sources, not an asynchronous video ring.

ReadTexturePixels now preserves native bytes for owned colour TransferSrc
textures, with a 64 MiB output limit and restored image layout. It remains a
synchronous Vulkan-only diagnostic. It is not the requested async export API.

Native caller: examples/GpuImageSourceDemo. It queries RGBA32F, uploads negative
and HDR RGB with fractional alpha, and verifies exact native bytes on readback.
It does not yet draw a colour-adjustable embedded surface or qualify playback.
Validation so far: real Vulkan precision tests PASS in Release and Debug/BLITZ,
with zero warnings/errors and final ownership ZERO. The native source example
PASS and existing four-format UI image pixel regressions PASS. This acceptance
covers raw resource transfer, not float shader/OCIO output or video performance.
Tests: RenderVulkanPrecisionTest checks nine formats, padded rows, partial-origin
updates, invalid spans, raw HDR/NaN/Inf/signed-zero bits and immutable lifetime.

## Staged completion gates

| Gate | Implementation and qualification required |
|---|---|
| Precision source resources | Implemented; real-device checks recorded with build/test evidence |
| Bounded asynchronous streaming | Pending: reusable staging/texture slots, fence completion, frame pins and deferred retirement |
| Embedded programmable image pass | Pending: GpuCtrl source pass, source identity plus independent exposure/gamma/viewport parameters |
| Reusable shader/LUT bindings | Pending: uniform updates, multiple texture/sampler slots, real 1D/3D LUT dimensions and explicit colour/target semantics |
| On-demand asynchronous readback | Pending: bounded output span/format/pitch and pollable completion without per-frame presentation stalls |
| Compute resources and dispatch | Subsequent: storage buffers/images, access barriers, limits and reductions qualified against source samples |

Current WriteTexture, Submit, DestroyTexture and diagnostic readback still use
queue-idle waits. Do not use the synchronous source example as evidence of an
efficient video path. Existing sampled pipelines expose one 2D texture slot and
no generic uniform binding. No OCIO GPU programme is accepted yet.

The next implementation gate must bound allocation before accepting work and
report CPU payload, staging capacity, bytes in flight and actual allocation
separately. It needs nonblocking enqueue/poll, explicit Busy/budget failure, and
completion-owned resource pins. Release should retire resources after all
referencing submissions complete; device loss must resolve outstanding tokens.
Keep submission order at the shared device domain and preserve painter order.

Frame generation/media number must travel with source and presentation requests.
Discard stale generations after seek/clip replacement. CinePlayer currently
acknowledges a selected CPU frame; an async root may subsequently drop it.
Actual presentation serial/acknowledgement is a separate required contract.

For the colour pass, unchanged source plus exposure/gamma edits must perform zero
source uploads. Define negative/non-finite handling and alpha/colour semantics
in the pass rather than altering originals at upload. Integrate pinned OCIO
generated code, enumerated uniforms, binding indices and all LUT resources through
reusable renderer bindings. A small built-in tone pass precedes OCIO, then
asynchronous result access. Compute follows; it remains a shader programme.

## Qualification and references

Require CPU-reference comparisons for UINT8/UINT16/HALF/FLOAT, layered sources,
data windows, alpha, linear/sRGB and OCIO outputs. Then measure 1080p/4K/8K
upload/colour/presentation p50/p95, actual presented FPS/drops, bounds and memory
plateau, seeks/paused edits/resize/failure/close. Cached presentation and decode-
through playback are separate results. The handoff's CPU stage measurements
are useful baselines; they are not renderer GPU measurements. Its approximately
130 ms ProRes decode/import mean cannot establish uncached 24 fps through GPU
colour alone.

Device query semantics follow the Vulkan specification:
[format properties](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceFormatProperties.html)
and [image format/usage limits](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceImageFormatProperties.html).
The source transfer uses
[buffer-to-image copies](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBufferToImage.html).
