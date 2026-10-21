# Windows / Vulkan whole-Ui qualification

Updated2026-10-22. **7 of10 current acceptance gates pass (70%)**.
Text and whole-Ui drawing are implemented and pass automated checks.
Two diagnostic five-minute runs now pass, but earlier long delays remain
unexplained. Physical input/DPI and visual acceptance also remain open.
This is not a finished-release claim.

## Current source and executable

Renderer:c1eb86f733dcf250ee14797fba0191aa732e825a.
Ui:f88aab5150ca7f375b8c4a61a01da57832c82e03, built from a verified committed-main
export so unrelated working Ui changes are excluded and preserved.

Pre-diagnostic comparison:build/GpuUiGalleryFinal.exe,8,886,272B.
SHA-256:9d3ac0a1f0eed7c575c008bd7996973836c5749c8d7f41c74a875ce87074bfd4.

[Exact build, runtime, regression and benchmark evidence](WINDOWS_VULKAN_FINAL_QUALIFICATION.json).
Earlier scalar/drawing baselines remain in VULKAN_UI_FINISH_QUALIFICATION.json;
they are historical results for different executable hashes.

Current diagnostic candidate:build/GpuUiGalleryLatency.exe,8,890,368B.
SHA-256:b4a20e21e44b4dc3a71d6de61c7683391b64de20c9b6b0ab7409a55641134d49.
It adds benchmark diagnostics only; renderer code and pass limits are unchanged.
Exact demo source SHA-256:61f066ecf2bbf4c38dcda8073fbd196f3a4e360316d7863329eecaf2f1b900fe.

## Implemented and checked

- The entire client root, modal workspaces and owned popups present through
  Vulkan. Required-GPU failure/retry and callback destruction are tested.
  Native window frames and OS input services remain Windows services.
- Semantic Painter/SVG/vector paths, gradients, masks, shadows, clipping and
  bounded blur use GPU coverage. Ui helpers preserve that semantic drawing.
- All56 drawable catalogue controls pass224 enabled/disabled/two-size probes
  at standard scale and224 at doubled application units.
- The Vulkan controls dialog includes UiFileBrowser, UiColorPicker and
  UiColorPickerMicro. Automation covers worker scan, preview, selection,
  accept/cancel/filter/nested colour popups,16 picker states,RGB/hex,themes/resize.
- Three workspace and three compute dialog cycles pass. Workspaces share one
  device with two surfaces/swapchains; final tracked native ownership is zero.
- Shared DirectWrite/ICU shaping and editing cover Arabic,Indic,mixed bidi,
  combining and supplementary text. Rich labels and UiDoc share native paragraph
  and line geometry; native IME composition uses a Vulkan overlay.
- Release and Debug pass64 GPU pixels,37 editor,36 styled,27 document,38 IME and
  80 font-layer checks. COLRv0 palettes and embedded PNG/JPEG/TIFF/BGRA assets
  reuse bounded glyph/image resources. SVG glyphs/COLRv1 paint trees are excluded.
- Repeated bitmap glyphs charge shared immutable pixels once per snapshot.
  Distinct images retain the unchanged4MiB payload gate.
- Normal native Ui without a renderer nest still builds in Release BLITZ.
  Core and Animation were not modified. The demo steps existing Animation::State
  using a coalesced renderer-owned steady clock and skips missed deadlines.

## Measured final-candidate results

RTX4070Ti,512 particles and grid where indicated. Four-second warmup.
CPU timings include acquire/replay/present elapsed time; GPU timestamps are
unavailable. Presented frames and distinct simulation updates are separate.

| Workload | Validation | Distinct Hz | Presented FPS | CPU median/p99 ms | Peak private bytes | Max timer delay ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
|96 particles,20s|on|120.00|120.30|4.88/10.46|202,928,128|18.44|
|512 particles,20s|on|120.00|121.30|5.97/13.43|202,858,496|18.81|
|512 particles,300s|on|119.79|118.61|5.77/13.32|206,012,416|361.00|
|512 particles,300s|off|119.64|122.96|3.55/8.34|179,949,568|404.19|
|Diagnostic,512 particles,300s|off|120.00|121.82|3.73/8.26|180,785,152|21.23|
|Diagnostic,512 particles,300s|on|120.00|119.31|5.88/13.29|206,610,432|19.30|

The two pre-diagnostic300s runs **FAIL responsiveness** because maximum timer delay exceeds the
unchanged100ms limit. Timer p99 is17.95/17.82ms, within50ms, and cadence passes.
The validation-on CPU phase maximum is522.70ms; validation-off91.00ms.
The cause is not established. A separate diagnostic demo records bounded stall
events with process/UI CPU deltas, presentation snapshots and native move/menu
activity. It does not change renderer behavior or pass thresholds. Both diagnostic300s runs **PASS**, with zero events above50ms. Early/late private growth is908,288B production and935,936B validation-enabled. Those passing repeats do not explain or erase the earlier failures.

Memory plateau passes both runs:validation-on early/late204,378,112/205,435,904B;
production177,980,416/179,266,560B. Growth is below the unchanged33,554,432B gate.
U++ heap counters are a subset of process memory; declared cache payload limits
are not total COM/driver/process or VRAM measurements.

Warm coverage misses/uploads,CPU vector/glyph rasters,Ui helper raster misses,
software layers,fallback and legacy GDI font requests are zero. Each run retains
and reuses one sampled descriptor pool and ends with native ownership zero.
The earlier user-closed soak is INCOMPLETE, not a crash or pass.

## Bounds and remaining acceptance

Per renderer:images64MiB/4096 entries; vector pixels32MiB/4096;glyph pages16MiB/
8192;path geometry8MiB/256;fast coverage1MiB;active vertices32MiB. Clip/filter
payload has a separate64MiB pool,32 entries,4 nested scopes,64 operations/frame
and4096 pixels/axis. These are explicit limits, not reserved allocations.

One replaceable pending frame bounds worker backlog. Existing synchronous GPU
completion owns retirement; asynchronous streaming is not implemented.

Finish sustained responsiveness diagnosis, then qualify the actual final binary.
Physical keyboard/mouse/AA,real OS IME candidates and100/150/200% monitor changes
are unqualified. Doubled application units and generated native messages do not
replace those checks. Current Caro runtime transfer awaits owner reapproval of
its changed Robocopy identity; no compiler or Vulkan SDK is needed there.

Unsupported semantic operations fail explicitly, including BeginMask,text-on-path
and image-pattern strokes. Repetition is bounded to64 cells/axis and256 total.
WebGPU/Metal,Linux,OCIO/LUTs and asynchronous video remain separate work.
Particle tests do not resolve the historical4K CineView callback outlier.
