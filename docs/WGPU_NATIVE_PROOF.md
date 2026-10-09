# Windows wgpu-native proof

Qualified 2026-10-10 on the selected KlickCurt Render machine.
This isolated test precedes RenderWgpu production implementation.
It uses the pinned official v29.0.1.1 GNU release via an explicit DLL path.
The legacy Vulkan implementation is preserved.

## Accepted scope

Release and non-BLITZ Debug pass on NVIDIA GeForce RTX 4070 Ti / Vulkan.
Each run opens and releases three independent instance/device/window cycles.
Each cycle queries actual adapter/device limits, checks all six pixels of a
3x2 offscreen RGBA8 green clear with 256-byte readback row pitch, creates a
WGSL triangle pipeline, makes 60 successful surface presentation calls, resizes,
minimizes/restores, drains teardown and releases the native objects.
Final native user-handle counts, uncaptured errors and unexpected device-loss
callbacks are zero in each cycle. Polling for readback is bounded to ten seconds;
teardown deliberately drains. This test is not the streaming/performance path.

Adapter max 2D dimension: 32768; requested default device limit: 8192.
The device limit differs from adapter capability and must govern allocations.
No CPU/software adapter is accepted and the instance enables Vulkan only.

Presentation calls do not prove physical display scanout. Offscreen clear pixels
are checked; the window triangle has not received a separate visual/pixel review.
Callbacks are installed, but device-loss/error fault injection is still pending.
The pinned native request-device implementation is synchronous; a browser host
must implement its asynchronous lifetime separately.

## Build and run

Acquire the matching release described in WGPU_DEPENDENCY.md.
The test .upp includes the extracted release headers, not a system header.

Release (separate output cache; ordinary non-BLITZ):
```text
umk E:/apps/github/upp_render/tests,E:/upp-18468/uppsrc RenderWgpuNativeTest E:/apps/github/upp_render/CLANGx64_Vulkan.bm --out-dir E:/apps/github/upp_render/build/cache-wgpu-native-release -r +CONSOLE E:/apps/github/upp_render/build/RenderWgpuNativeTest.exe
```

Debug: omit -r; use build/cache-wgpu-native-debug and
build/RenderWgpuNativeTestDebug.exe. Do not mix cache modes.

```text
RenderWgpuNativeTest.exe --dll E:/apps/github/upp_render/build/deps/wgpu-windows-x86_64-gnu-release-v29.0.1.1/lib/wgpu_native.dll
```

The test windows close automatically. Exit 0 means every stated assertion passed;
exit 1 is a test failure, and exit 2 is invalid invocation/dependency unavailable.
The DLL version must equal 0x1d000101 before any descriptor structs cross the ABI.
The loader converts UTF-8 to Windows UTF-16 explicitly and restricts DLL search.

| Build | Binary SHA-256 | Build job | Run job |
| --- | --- | --- | --- |
| Release | 9e39498e62c0a93ac76d1e0a8a74f4b1d083cc002f71a926b8a4dff11412bab8 | exec-1565BEDE79E148BEB3AC86FDFD4E08BC | exec-866E8EE4EECE01BDA6898ED242D16CFC |
| Debug | 64caabcccaa4b5d1a811610f919393b829ec8a99f6315a1587511fe15345ffd4 | exec-4DCC0EDC6062E14BA5A2D59747266BBF | exec-442FABFEDE57D7F10E5F52797C9880A9 |

Source main.cpp SHA-256:
277cfb062823a2903bd382b9f1fa5f4dd2e973ad5424ebacd065a651dff92297.
Source .upp SHA-256:
736d7d168160f758f76a4040a70f4e00295833f72a4d4d754b4da5fc156237c6.
Tests ran on a working tree based on 0999308; committed identity is recorded
alongside retained evidence rather than inferred from an unchanged Git HEAD.
Detailed evidence: build/wgpu-native-proof-2026-10-10.json.

## Reuse boundary

RenderCore, RenderCanvas, RenderVector and RenderSoftware depend on Core/Draw/
Painter and neutral semantics. Reuse those contracts and reference fixtures.
RenderCtrlBridge now owns the existing recorder without GpuRender/RenderRhi/
RenderVulkan dependencies. GpuRender's old header forwards for existing callers.
The moved header/implementation retain their original raw hashes:
1bd678b85d0dcfe5704abea3553e28603803f642fd9eb17b1ebcac8655a7dd0a /
63bd0b11adc4c30338611f0a042523bcc4607912b80a2e182bf20d1de0974d6f.

The recorder still uses Win32 SystemDraw, font and native-theme facilities.
This extraction does not remove GDI. RenderGpu2D, RenderPlatformWin32 and the
current GpuRender presentation implementation depend on the old RHI and cannot
be reused as the new wgpu GPU implementation without violating the chosen design.

## Remaining completion gates

Direct semantic replay, image/text/vector pixel parity, real Ui root and embedded
surfaces, independent/shared surface lifetimes, bounded uploads/caches/in-flight
work, memory plateau and responsiveness are still required.
So are populated/input/DPI/popup control coverage, clean-machine DLL delivery,
fault recovery, portable text, browser host and Apple/iPad native feasibility.
No whole-Ui, performance, footprint, compute or portable-platform completion is
claimed by this dependency/Win32 host proof.

Reference: [pinned upstream release](https://github.com/gfx-rs/wgpu-native/releases/tag/v29.0.1.1).

## Neutral recorder and preserved Vulkan compatibility

RenderCtrlBridgeTest passes after its dependency changes from GpuRender to
RenderCtrlBridge. Its build contains 15 packages, with no RenderRhi, RenderVulkan
or GpuRender package. Existing recursive control paint, geometry, crop/mask/image
identity, deterministic lists, global-backbuffer restoration and software
reference checks remain in the same test source.
Build: exec-08EC78E31B07FF024DED2CDADBB05C6C.
Run: exec-CA619FB4DC5CD924BD2FE1EC50DF9CC7 (exit 0).
Binary: c1064ba24d60217646fc26fe22b2d69211325a4036956a50ff568fda419f1e7e.

GpuTopWindowPresentationTest also passes with the preserved Vulkan path and
forwarding header. This covers normal and async root capture, resize/hide/show,
failure/fallback, required-GPU failure/retry and owned-popup failure handling;
final Vulkan ownership is zero.
Build: exec-905FBF88D08AD2C5E7FA48F78C9FDA73.
Run: exec-83991C2055C30D99287670553D57EB9B (exit 0).
Binary: 95f31a6ca1751224bb025bb8fa16b4591781da28f6d112d9350336674d5221b5.

These are extraction/compatibility regressions, not acceptance of wgpu control
replay or completion of the outstanding sustained-performance gate.
