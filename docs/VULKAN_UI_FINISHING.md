# Windows/Vulkan whole-UI finishing

Updated 2026-10-22. **7/10 acceptance gates passed (70%)**:1,2,3,4,6,8,9 from ACTIVE_WORK.md. This is a count of acceptance gates, not engineering effort. Full shaped-text/editing and final physical input/DPI acceptance remain active work. The user explicitly requested full shaping now; it has not been deferred.

## Qualified drawing and scheduling baseline

Current executable: build/GpuUiGalleryFinish.exe, SHA-256
9a4d8f6fc61713e7d8e04dba22289380462219e825325d96939da994420efb57 (8,296,960B).
Ui dependency commit:228a1b968e0b13ef21b4cbb4baf1115c3c7a8686.
Exact source/dependency hashes, jobs and raw reports:
[VULKAN_UI_FINISH_QUALIFICATION.json](VULKAN_UI_FINISH_QUALIFICATION.json).

- Entire client roots, modal workspaces and owned popups are presented through Vulkan. Required-GPU failure/retry and destruction during frame callbacks are tested.
- Semantic Painter/SVG/retained Painting record paths and gradients without CPU coverage images. General curves, winding, caps/joins/dashes, linear/radial/angular gradients, image fills, path clips and bounded blur composition execute through the GPU path.
- Ui faces, focus, shadows, rings, range tracks, layers, dropdowns, tags and graph-grid drawing preserve semantic intent. Bitmap decoding, procedural colour fields and icons remain asset preparation, separately from vector/glyph rasterization.
- The 57-entry current Ui catalogue has56 drawable entries and one OS dialog service. All224 enabled/disabled/two-size drawing cases pass; eight additional hard/soft/inset shadow cases check pixels, cutouts and resource reuse.
- Scene > Browser and colour pickers embeds UiFileBrowser, UiColorPicker and UiColorPickerMicro in a Vulkan modal window. Qualification exercises worker scan, image preview, selection, accept/cancel, filter and nested colour popups,16 picker states, hex validation, RGB, themes and resize.
- Three populated workspace and three compute-dialog cycles pass. Workspace cycles observe one device/two surfaces/two swapchains; final native ownership is ZERO. Current font requests use DirectWrite, with zero legacy GDI font requests.
- Native non-GPU UiLabelDemo still builds without the renderer nest (Release BLITZ).
- Core and Animation were not modified. GpuTopWindow exposes coalesced UI-thread steady-clock callbacks; Gallery steps the existing Animation::State using that clock and skips missed deadlines. Ordinary U++ timers retain their original clock.

## Current measured results

All runs use required Vulkan, validation requested, grid enabled,120FPS target and4second warmup. Normal/heavy measure20seconds; soak measures300seconds.

| Workload | Distinct updates/s | Presented FPS | CPU median/p99 | Peak private bytes |
| --- | ---: | ---: | ---: | ---: |
|96 particles|120.00|120.30|5.04/8.31ms|199,028,736|
|512 particles|120.02|122.22|6.26/10.65ms|198,844,416|
|512 particles,300s|119.92|119.87|6.15/14.27ms|198,815,744|

CPU figures include acquire/replay/present elapsed time; GPU timestamps are unavailable. Additional status/control refreshes explain why presentation can exceed animation cadence; distinct progress is measured separately.

The soak's private-memory early/late means were198,714,368/197,966,848B (747,520B decrease), against the unchanged33,554,432B growth allowance. U++ heap samples ranged2,461,696–4,702,208B; these do not represent all process/driver allocations. Timer-delay p99/max was17.98/79.40ms, within the existing50/100ms gates. No strict per-frame8.33ms deadline claim is made.

Normal/heavy/soak show zero cold and warm CPU vector/glyph rasters, shared helper raster-cache misses, software layer allocations and fallback. Warm glyph/vector coverage misses are zero. One sampled descriptor pool is retained/reused. Every completed run ends with native ownership ZERO and GDI font requests0.

The earlier193.937s soak was interrupted by the user closing the app and is preserved as INCOMPLETE. It is not counted as a failed GPU run or a passed soak.

## Memory and work bounds

Retained payload limits per renderer remain64MiB images/4096entries,32MiB vector reference/GPU pixels/4096entries,16MiB glyph pages/8192entries,8MiB path geometry/256entries,1MiB fast path coverage and32MiB combined active vertices. Clip/filter composition adds a separate64MiB pixel-payload pool with32entries,4nested scopes,64operations/frame and4096pixels/axis. These are limits, not reserved or total process/VRAM sizes.

Clip targets crop to conservative path bounds and reuse idle sizes. In-flight frame ownership remains bounded by one replaceable pending frame; resource retirement relies on existing synchronous GPU completion. This does not introduce asynchronous queue submission.

## Remaining acceptance

1. Full shaped glyph runs, language-aware fallback/direction and cluster-correct measurement/hit-testing/editing. Rendering alone must not diverge from caret/selection/undo. This is the next authorized implementation.
2. Final physical keyboard/mouse/AA and100%/150%/200% monitor-DPI acceptance after text integration. Readback captures and callback automation do not replace those checks.
3. Rerun affected drawing/load/soak tests on the final text-integrated artifact and refresh this manifest. Current evidence describes the identified baseline.

Unsupported Painter operations fail explicitly (including BeginMask, text-on-path and image-pattern strokes). Image repetition has64cells/axis and256total-cell bounds. Software BlurLayer is explicitly unsupported by the neutral reference; ordinary native Ui software helper paths remain available. Layered-mask AA does not promise exact legacy per-primitive compositing.

WebGPU/Metal, Linux hosting, OCIO/LUTs and asynchronous video remain separate platform/imaging work. Current clean-machine Karo qualification applies to the earlier bundle, not this artifact. Historical evidence remains in the2026-10-11/13/15 manifests and Git history.
