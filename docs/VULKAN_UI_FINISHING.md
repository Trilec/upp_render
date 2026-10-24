# Windows/Vulkan whole-UI qualification

Updated2026-10-24. **8/10 strict acceptance gates (80%); text7/8 (87.5%).**
Automated functional tests pass. Physical AA/input, OS IME candidates and physical
monitor DPI acceptance remain separate. Owner confirmed inspector, workspace and
animation worked and closed normally; the exact scope is recorded.

## Current exact candidate

build/GpuUiGalleryWindowsV1Final.exe, Windows x64 Release,8,911,872B.
SHA-256:65f771f7f1d2f7a1b4b7c451cce9bebec5ce9945bc842c44d17125bc703c683e.
Renderer base1d85294 plus11 exact source hashes in current_candidate of
[WINDOWS_VULKAN_CARO_QUALIFICATION.json](WINDOWS_VULKAN_CARO_QUALIFICATION.json).
[Qualification history/index](WINDOWS_VULKAN_FINAL_QUALIFICATION.json).

Ui pinned exportf88aab5150ca7f375b8c4a61a01da57832c82e03. Independent Ui dc789c1
has a substantial uncommitted refactor and is excluded/preserved. Core and
Animation were read-only. Caro requires the graphics driver's Vulkan runtime,
with no U++ compiler or Vulkan SDK installation.

## Actual remaining GDI path corrected

The inspector's colour editor opens a Sizeable().Zoomable() TopWindow. Windows
removesWS_POPUP for that style. The transient hook previously requiredWS_POPUP,
so this real workflow escaped GPU rendering despite having the correct owner:
one surface and242 GDI font requests. The hook now also admits ordinary
TopWindow dialogs only when they have a ready/required Vulkan ancestor.
Unowned/unrelated application windows remain excluded.

The actual inspector swatch keyboard activation, accept and cancel now produce
two surfaces sharing one device and0 GDI requests. Resizable/maximizable nested
dialog regressions pass Release/Debug. A prior nonmaximizable standard-dialog
probe passed before the fix because it retained popup style; its insufficient
coverage and reverted speculative change are preserved explicitly.

Demo IDs987..995 also exceeded this build's104-byte Ctrl timer range; owned
TimeCallback members now control their lifetime. GpuCtrl services normal U++
timers on coalesced frame notifications, avoiding WM_TIMER starvation during
continuous paint. A filtered-pump test excludesWM_TIMER and verifies timer
progress plus callback-driven destruction of its refreshing host.

## Current tests

- Release fullUI with validation:PASS15.375s.
- Debug fullUI with validation:PASS20.375s.
- Caro production fullUI on the same executable hash:PASS13.078s.
- Root/nested owned-dialog lifecycle Release/Debug PASS; shared device, resize,
  failure/fallback/retry, old-input wake regression and final ownershipZERO.
- Embedded frame-timer/lifetime regression Release2.797s/Debug3.547s PASS.
- Actual inspector colour dialogs; three workspace and three compute cycles;
  focus/tree/table editing; pause/resume/speed/inspector state; resize/themes.
- File-browser scan/preview/selection/accept/cancel/filter/nested micro popup;
  full/micro pickers,16 states,RGB/hex; text composition overlay/cancel/commit.
- Current reports:GDI0,software fallback0,CPU glyph/vector rasters0,final nativeZERO.

Capture helpers respect the caller's validation mode. An explicit --validation
still fails when its optional developer layer is absent; omit it on SDK-free Caro.
Debug capture budget120s versus Release30s; the root frame-wait deadline restarts
after independently timed modal stages. Benchmark thresholds were unchanged.
Initial Debug deadline failures and all other failed runs remain in the evidence.

The preserved broader baseline covers56 controls,224 standard/224 doubled-unit
states and native text Release/Debug checks:64 GPU,37 editor,36 styled,27 document,
38 IME and80 font-layer cases. DirectWrite/ICU shared geometry covers Arabic,
Indic,bidi,combining/supplementary text,rich labels and UiDoc. COLRv0/bitmap assets
are supported; SVG glyphs/COLRv1 remain excluded. Doubled units and generated IME
messages do not certify physical monitor scaling or OS candidate interaction.

## Measured performance and limits

Current Caro compute20s (Iris Xe):60.02 presentedFPS,CPU median/p99 4.36/6.58ms,
UI callbackmax21.68ms,privatepeak161329152B,one shader/image build,reused pools,
memory/resource/ownership PASS. GPU timestamps unavailable; CPU values include
elapsed submission/presentation work and are not pure GPU execution time.

Current final RTX production300s PASS119.99distinctHz/122.84FPS; CPU median/p99
3.73/10.98ms,timerp99/max17.89/22.22ms,0events>50ms. Privatepeak182329344B,
early180369408/late182031360,growth1661952B (about1.6MiB). All12000samples visible,
none minimized;one pool reused37269 times,GDI0/fallback0/finalZERO. Earlier8058...RTX512/grid validation20s passed
119.81 distinctHz/115.51presentedFPS,timermax23.67ms. Earlier wake-fixed RTX300s
validation/production both passed120distinctHz,timermax19.20/18.95ms and bounded
memory. Those are historical exact binaries, not this final executable.

Caro five-minute attempts were user-closed,mostly minimized,or changing workload.
The unminimized interaction run changed rate to127FPS,opened additional resources
and entered6move loops; it failed latency/memory/GDI. This is retained as stress
evidence,not a steady-load pass. The newly proven inspector GDI defect is fixed;
do not retroactively assign every historical spike or failed run to that cause.

Cache payload bounds:images64MiB/4096,vector pixels32MiB/4096,glyph pages16MiB/8192,
geometry8MiB/256,fast coverage1MiB,active vertices32MiB; clip/filter separate64MiB/
32entries/4scopes. These are not total COM/driver/process or VRAM memory limits.
Retirement remains synchronous. Unsupported BeginMask,text-on-path,image-pattern
strokes fail explicitly; repetitions are bounded64cells/axis,256total.

Fresh package staging:build/upp-render-ui-2026-10-24-final. Older8058/c510
intermediate packets are superseded. Current source/runtime identities,raw jobs,
all reports and delivery verification are in the current JSON; use matching
README/AGENT_TASK/SHA256SUMS. Keep fixed-load windows open and unminimized,without
changing size/controls/rate; they close automatically.

Remaining physical acceptance:AA/keyboard/mouse on all pages,actual OS IME
candidate select/commit/cancel,and100/150/200% monitor DPI. A separate unminimized
Caro300s run remains unqualified. WebGPU/Metal/Linux,OCIO/LUT/video and asynchronous
streaming remain subsequent scope.

Final ZIP:build/upp-render-ui-2026-10-24-final.zip,3884676B,
SHA4b08f8942347ab6ff4a5c981331bde564d5d46638423cba6dd35edf2bb21c5b4.
Share:F:/networkshare/upp-render-ui-2026-10-24-final.zip and extracted final folder.
Caro:C:/GitHub/upp_render/build/runtime-2026-10-24-final/GpuUiGallery.exe.
Original ZIP and executable independently hash-identical on Caro; no test running.
