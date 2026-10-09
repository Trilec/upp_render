# Active Work

## Current direction

User authorized a clean, lean wgpu renderer baseline, with backups and architecture
documents before replacing implementation. Keep C++/U++, Ui controls, Designer,
themes and imaging/application models. Work on main.

Design: WGPU_ARCHITECTURE.md.
Gates: WGPU_IMPLEMENTATION_PLAN.md.
Recovery and verified bundle: WGPU_RECOVERY.md.
ARCHITECTURE.md continues to describe the preserved Vulkan implementation.

## Recovery checkpoint

- Render project: project-028DDA9C9C89 through KlickCurt.
- Root: E:/apps/github/upp_render.
- Clean fetched baseline: 0999308ad1b7e2f60789356b7923fb93955c5480.
- Verified bundle: build/render-before-wgpu-2026-10-08.bundle.
- Bundle SHA-256: 97e1b69a2aec889377fcb2e96fc5686e2eb329b5ee1b1a23e4f177ba283553b0.
- Git bundle preserves tracked history/refs, not ignored dependencies/outputs.
- Existing source remains intact; new design is not a running wgpu renderer.
- Preserve RC1 manifests, previous branch bundles and Patch recovery journals.
- Re-resolve project, inspect status/diff/jobs before resuming. No blind reset.
- Commit/push/publication are separate from this checkpoint.

## New design mandate

- Semantic canvas and U++ host facade above direct private wgpu-native internals.
- No permanent adapter through the old GpuDevice/provider abstraction.
- Immutable frames; GUI-thread Ui access; shared device and independent surfaces.
- Bounded cache bytes/entries, uploads/in-flight frames and deferred retirement.
- Explicit interactive frame replacement versus EveryFrame backpressure.
- Reproducible WGSL; preserve precision, alpha, colour and painter-order contracts.
- Native iPad host/toolchain feasibility is an early gate.
- Browser WebGPU requires a separate WASM binding/host.
- Measure size, startup, memory, input delay and replay before claiming gains.

## Accepted legacy evidence to preserve

- Windows x64, U++ 18468, clang 21.1.1, Vulkan SDK 1.4.350.0.
- Neutral canvas/list/software reference; images/text/vector/SVG; embedded/root
  presentation and compatible device-domain cleanup.
- Representative real Ui controls, PropertyEditor, menus/submenus and tooltips.
- Cache pixel/entry budgets, immutable-image sharing and worker immutable frames.
- RC1 five-minute qualification and owner manual acceptance are historical PASS;
  exact source/dependency/artifact identities remain in their manifests.
- RC1 heavy Iris Xe throughput around 10 FPS was a measured limit.
- UI2 image crop/mask/scale/readback/premultiplied fixes and fault tests passed.
- Default enabled/disabled inventory: 54 painted entries, 216 cases passed;
  populated/input states and full control conformance remain incomplete.
- Latest UI2 300-second validation ended early; sustained gate remains open.
- Native precision transfer/query/readback tests passed, including HDR bit retention.
- Existing queue-idle transfers are not an efficient accepted video streaming path.
- Streaming, programmable colour/LUTs, async readback and compute remain pending.
- Windows font/host rendering is not portable Apple/browser hosting.
- References: WINDOWS_VULKAN_V1.md, UI2_DRAWING_AUDIT.md,
  GPU_IMAGE_PIPELINE.md, CINEVIEW_RENDERER_HANDOFF.md.

## Next concrete gate

wgpu-native v29.0.1.1 official Windows GNU DLL acquired; ZIP digest verified.
Release and non-BLITZ Debug native proofs PASS on RTX 4070 Ti / Vulkan:
exact offscreen pixels, 180 presentation calls per build, resize, minimize/restore,
three device/window cycles, zero reported errors and retained user handles.
Identities/reproduction: WGPU_DEPENDENCY.md and WGPU_NATIVE_PROOF.md.
Curl/tar are approved acquisition tools; no Cargo build or lean feature claim.
Control recording moved unchanged into RenderCtrlBridge; old include forwards.
Neutral recorder and legacy Vulkan root regressions PASS; final ownership zero.
Next: direct RenderWgpu semantic replay, adjacent painter-order batching,
bounded image/glyph/vector resources and an opt-in root/embedded Ui facade.
Keep the accepted source/pixel fixtures; avoid an old-RHI adapter.
Apple/iPad feasibility, browser hosting, portable text, memory/load gates and full
control acceptance remain open. Do not retire Vulkan or claim the project complete.

## Evidence discipline

Tests, real visual review and platform delivery are separate evidence.
FPS is not latency. CPU timings are not GPU timestamps.
No silent fallback counted as GPU success; no iPad acceptance from Mac Metal alone.
