# Windows/Vulkan restart checkpoint

Saved 2026-10-21 before the user restarts Codex. Resume the existing Windows/Vulkan completion request in this chat. This milestone is IN PROGRESS; do not claim completion from historical results.

## Newer resume update — full text work authorized

User explicitly authorized full complex-script shaping/editing now on2026-10-21: do it properly, carefully, clean/efficient/fast/light. No scope decision remains pending. Core/Animation stay READ ONLY. Do not silently close text gate5 or defer it.

Finishing regression results now pass: Release SemanticPainter/general vectors/SVG/DirectWrite Unicode; Debug semantic/general vectors; non-CFONTS Canvas/Vector references; root callback destruction;224drawing cases covering56 catalogue entries plus8hard/soft/inset shadow cases and table edits. UiLabelDemo ordinary non-GPU/no-render-nest Release BLITZ build passes (exec-9B86323521B6E5AB659F4435E2782B3C).
Catalogue57;56drawable probes, only UiOsFileDialog excluded as OS host service. Added UiColorPickerMicro and optional UiFileBrowser.

New Gallery GpuControls.h/.cpp adds Scene > Browser and colour pickers. Browser scan/64x32 WIC image preview/selection/accept/cancel/filter popup/nested New-colour micro popup,16 full-picker states, RGB/valid-invalid hex, themes/resize PASS. Still3workspace +3compute cycles. Source latest compact micro layout qualified; corrected PNGs inspected.
Gallery final build exec-0190110EFF0D8AB1FD2D14593C616B88; build/GpuUiGalleryFinish.exe8296960B SHA9a4d8f6fc61713e7d8e04dba22289380462219e825325d96939da994420efb57.
Qualification exec-AD79CCEF8F33A634734D04D05D85FBA0; capture exec-F134BF914894FC8EF27FF965E91519C7.
56inventory build exec-5F03D683F836601D2C67CBE208AE71E1; run exec-F89A965602E366E96390483600400253 PASS224. Images build/UiGpu-contact.png and GpuUi-{root,dialog}-{light,dark}.png plus GpuUi-{micro-light,micro-dark,picker-dark,browser-dark}.png.

Normal exec-4E6934A58FD73064158D81A8944AE830:120distinctHz/120.30presentedFPS, CPUmedian/p99=5.04/8.31ms,first84.04ms,privatepeak199028736B,timerp99/max17.81/18.70ms.
Heavy exec-21FD22729F358FD640A5B06B2C054386:120.02distinctHz/122.22presentedFPS(extraUi refreshes),CPUmedian/p99=6.26/10.65ms,first82.19ms,privatepeak198844416B,timerp99/max17.98/19.53ms.
Both zero CPU vector/glyph raster, shared helper raster cache, software layers, GDI fonts, fallback; finalnativeZERO. Exact reports build/GpuUiGallery-required-{normal,load}.txt.
First soak exec-61B43E5A80B252B211D41DED10A6EF73 ended193.937s; USER CONFIRMED closing it. INCOMPLETE,notcrash/pass. Saved build/vulkan-ui-soak-interrupted-2026-10-21.json.
REPEAT SOAK RUNNING exec-A5DF4ECEC35AAFA198CD9F13DC560641 (300measured+4warmup,600s budget). Recover result, do not replay/compile/stress CPU during timing.

Ui17ownedpaths staged via exec-52F3B3A96DBE097ABB42DE95156AF58A; scoped diff-check passes. No commits yet. Verify staged names before commit. Ui HEAD495240df1aa7387d9f4b166ba6d195cfa9b74b07,ahead1. Additional concurrent unrelated dirty paths: Ui/UiFileBrowser/README.md,docs/04_UI_DEMO_GUIDE.md,tests/CONTROL_AUDIT.md,tests/ui_release_inventory.json,untracked examples/UiFileDialogDemo/. Preserve those plus earlier dirt.
Next preserve exact final tests/reports in tracked qualification manifest, update stale docs/dashboard/readfreshrevision, commit owned Ui then render baseline locally(no push), then shared full text work.

Text design ONLY, no text code written yet:
- Shared UiTextLayout: positioned glyph runs, scalar/UTF16 cluster mapping, measurement/hit-testing/caret/selection and cluster-safe navigation/deletion. Reuse DirectWrite IDWriteTextLayout + custom IDWriteTextRenderer; no home-grown Arabic/Indic/bidi algorithm.
- CPU shapes/caches immutable bounded layouts; renderer consumes face/glyph-ID positions and uses existing GPU coverage/atlas. Avoid CPU whole-text raster images.
- Add explicit neutral shaped-run alongside legacy scalar DrawText, no Core overrides. Ui text helpers must intercept whole text BEFORE U++ Draw::DrawText fallback splits it.
- UiBaseEdit EnsureTextMetricsCache~718,caret/hit~1418,PaintLine~1850 currentlyperchar; arrow/delete logicalscalar. UiLabel BuildCaretAdvances and selected text splits~1250/1380. Audit UiDoc andmodel-view measurement too.
- RenderGpu2DText.cpp EnsureGlyph keys font/scalar; refactor common GPU atlas packing for explicit font-face/glyph-ID sources. Abstract outline source can use existing FontGlyphConsumer without DWrite types.
- Keep ordinary Ui build without render nest; no low-level renderer dependence on Ui controls. GPUUI optional header boundary already exists.
- System Unicode authority for direction; test Arabic/Indic/mixedRTL/combining/ligatures/supplementary/fallback,caret/click/selection/delete/undo and actual GPU pixels,cold/warm cache bounds.
- Primary references: learn.microsoft.com/en-us/windows/win32/directwrite/introducing-directwrite ; /directwrite/getting-started-with-directwrite ; /api/dwrite/nf-dwrite-idwritetextanalyzer-getglyphs.
- Procedural palette textures/micro swatches/decoded previews remain asset prep; report honestly. OS file dialog and screen eyedropper are host services.

## Authorization and workflow

- Finish direct Windows/Vulkan whole-Ui drawing, frame pacing, measured memory/responsiveness, main window and GPU dialogs. WebGPU/Metal/CineView follow later.
- Core (E:/upp-18468/uppsrc) and Animation (E:/apps/github/upp_animation) are READ ONLY by explicit owner instruction. Ui drawing changes are authorized.
- Work on main; no branches. User authorized local commits and reserved publishing. Do not push or publish. No subagents unless explicitly authorized.
- All mutations through KlickCurt Patch; builds/runs/git mutations through KlickCurt Execute. Local shell read-only searches/observations only. Read klick-project-workflow and upp-ui-development skills.
- Render: project-028DDA9C9C89, folder-D804EA49C6B4, E:/apps/github/upp_render.
- Ui: project-B3BB7BB32A38, folder-B2DAFD8124F1, E:/apps/github/upp_Ui.
- Host DESKTOP-GFX1; Klick1.0.35, policy47, source ca53cec1917ea91498d2feb3f1760431053bb09d, executable SHA9bb30d69d67e0cd0a1a01816143d40f7ee23ab7b3c28a3cfc81eac7134bd803f.
- After reconnect use KlickCurt klick_resume_context if available, otherwise identity/project context and retained jobs. Never replay uncertain starts/applies.
- Patch: inspect current complete-file hashes, preview then identical apply plus plan_digest; inspect receipt. Strip leading BOM from read text before rewrite to avoid double BOM; preserve newline style.

## Source state to preserve

Renderer main HEAD bb32884df1d8ea8693f498a7adec230c7edd456c, ahead3 origin/main. Current finishing changes are UNCOMMITTED. Git status lists authoritative paths. Prior committed work: bb32884 GPU path/glyph coverage, 8f0bfc8 descriptor reuse, 8b49d40 compute/image surfaces.

Unrelated Ui dirty files at turn start: Ui/UiThemeOlive.h, Ui/UiTitleCard.cpp, Ui/UiTitleCard.h, Utilities/UiThemeTests/ThemeSurface.cpp, docs/01_UI_CONTROLS_GUIDE.md, docs/02_UI_THEME_GUIDE.md, docs/ACTIVE_WORK.md. Preserve and do not stage these.

Our Ui paths: UiDrawBase.h, UiDraw.h/.cpp, UiRenderLayer.h, UiSlider.cpp, UiRangeSlider.cpp, UiSplitter.cpp, UiDropdown.cpp, UiDropdownPopup.cpp, UiDoc/UiDocPaintOverlay.cpp, UiProgressRing.cpp, UiChartRing.cpp, UiRangeSegmentsPaint.cpp, UiTag.h/.cpp, UiGraph/UiNodeGraph.h and UiNodeGraphPaint.inc (all under Ui/).

Our new renderer files: render/RenderVector/SemanticPainter.cpp, render/RenderGpu2D/RenderGpu2DLayer.cpp, shaders/box_blur.frag, shaders/BoxBlurSpirv.h, examples/GpuUiGallery/GpuCapture.h/.cpp, tests/RenderSemanticPainterTest/main.cpp and .upp. Other modified renderer paths appear in git diff/status. Do not discard these.

## Implemented this turn

- Real steady-clock frame callbacks in GpuTopWindow and UI-thread coalesced240Hz wake. Gallery explicitly steps Animation::State with steady time, schedules deadlines and skips missed updates. Core/Animation unchanged. Distinct update-rate benchmark now requires >=90% requested FPS; removed forced redraw that inflated presentation counts. Lifetime guard supports callback destroying owning window.
- Semantic Painter records existing Painter/SVG/retained Painting operations into display-list paths, gradients, glyph outlines and image fills instead of CPU raster images. Gradient transforms and premultiplied color conversions corrected. Unsupported operations fail atomically.
- GPU angular gradients, path clip layers, destination-in composition, 3x3 box blur shader. Pool bounded32entries/64MiB pixel payload,4nested layers,64operations/frame,4096axis; cropped clip bounds and global target origin reduce payload. Zero ownership on close.
- Ui shape/style/focus/radio/thumb/badge/grip/flash/ring/range/layer/dropdown/shadow/tag helpers preserve vector intent. Tag cache retains Painting commands, not raster images. GPU node-graph dots are rectangles rather than cached tile bitmap.
- New Gallery --capture-scenes and inventory --capture produce actual Vulkan readback PNGs. Latest source fixes PNG RGBA-to-U++ byte-field conversion and white inventory backdrop. Existing PNG outputs PRE-DATE these fixes and must be regenerated.
- Last correctness patch: SemanticPainter Prepare short-circuits failures, Close resets controls, End marks path finished; unsupported clear-after-content/transform-mid-path reject; SVG finite/size bounds. These compiled in lifecycle build but full regressions still required.

## Verified evidence (scope matters)

1. Latest native lifecycle test PASSED at restart. Build exec-D9FF19E8D32253B6EAF80BF63D503179. Run exec-A885B4643B0F94589C96DB59FFB0BE92, exit0, stdout "GpuTopWindowPresentationTest passed",8.047s.
   Binary build/GpuTopWindowFinish.exe,5373440B, SHA7b07ab790dfbb804add79186e301f5a6aec4db76c5f32232456a2079e1d674c8.
   Includes monotonic frame callback safely destroying owning window on callback8, plus existing failure/retry/shared popup lifecycle. No active Execute job remains at checkpoint.

2. General vector GPU suite PASSED38cases x4formats=152 (RGBA8/BGRA8/each sRGB).
   Build exec-C928258866C5B956BED225642259B300; run exec-81F66EB1A02282E298CCF208F6CD1003.
   Angular independent numeric oracle <=2; blur alpha <=1; repeated rounded image alpha; sibling reuse;4nested pass/5reject/recovery; zero resources/validation. Includes cropped origins. Before latest tiny semantic guard patch.
   Existing legacy stroke max117/mean.2783 tolerance128 was not weakened; independent square-cap union oracle max0.

3. Ui inventory PASSED54controls x2sizes xenabled/disabled=216, not full input/populated coverage.
   Build exec-D6198B7BC262BF5BC0251A16798EDE43; run exec-2DED5165D16838E1D78462F2FEE603B8 (--capture), zero GDI fonts/validation/software layers.
   Independent CPU-reference cache cleared before production probes. Only retained cache tag vector/tag/decoration bytes644; no production CPU helper raster cache.
   build/UiGpuDrawingFinish.exe9133056B. PNG channel/backdrop fixes newer than binary; rebuild and recapture.

4. Gallery root/dialog light/dark capture PASSED zero CPU vector/glyph rasters, GDI font requests0, final native ownership ZERO.
   Build exec-888C3E0BE42B09232082019B736B93B9; run exec-FC3FC4F73C618079ED4DAA515C9ADE05 (--capture-scenes --validation --particles=512).
   Report build/GpuUiGallery-capture.txt; PNGs build/GpuUi-root-light.png/root-dark.png/dialog-light.png/dialog-dark.png. Layout/text coherent, but old PNG helper red/blue byte order corrected only in latest source. Rebuild/recapture before final visual review.
   Capture uses separate offscreen renderer for actual control painting: not proof of live presentation speed.
   Earlier failed capture build exec-6C7C71FFDAA8890E9B04302F08372353 was followed accidentally by old exe run exec-4342A7D2621B40F9C3FB21A20F30BC1A. That wrong run cancelled125, NOT evidence. Error was acknowledged. Always check build success BEFORE selecting executable.

5. SemanticPainter CPU equivalence PASSED10cases (8exact; gradient max2 mean.2266; opacity max2 mean.1479).
   Build exec-0C227BD2A70640DC0C0EDF818A0F6419; run exec-D1B4F9533ABF289452DC8E5AB528A9B1.
   Early version before later clipping/image/guard changes. Rebuild Release+Debug.

6. Pacing heavy512+grid benchmark before latest helper/composition migration:120.00 DISTINCT updates/sec,120.70presentedFPS; CPU acquire/replay/present median6.36ms/p99 14.29ms; timer p99/max17.71/18.53ms; firstframe87.46ms; peakprivate200372224B, zero GDI/fallback/native ownership.
   Build exec-F008EE1F9D9725573798299D02F1DF4D; run exec-11011C9977725B5586C01AB79DEF3F4C.
   Report build/GpuUiGallery-required-load.txt. Historical to current source: final benchmarks and soak NOT YET RUN.

## Exact resume sequence

1. Recover context, inspect git/status and this checkpoint. Latest lifecycle result above is terminal success; no need replay.
2. Review latest semantic edge changes; rebuild/run current RenderSemanticPainterTest Release+Debug, RenderVulkanGeneralVectorTest Release+Debug, RenderVulkanVectorTest, Unicode glyph and relevant RenderCanvas/Null/reference regressions.
   RenderVulkanVectorTest batch assertion changed to7 following GPU SVG. Last retained runtime before assertion correction FAILED; no passing runtime after correction yet.
3. Rebuild/run UiGpuDrawingTest --capture after PNG/backdrop fix; inspect contact/individual controls.
4. Rebuild Gallery; run --qualify-ui --validation (3populated main/dialog cycles plus3compute cycles, input/focus/edit/resize/themes/shared device/surfaces, finalZERO/GDI0).
5. Run corrected --capture-scenes --validation --particles=512 and inspect main/dialog light/dark.
6. Run normal/heavy120FPS benchmarks then300second heavy soak with4sec warmup. Preserve raw output/reports before overwrites:
   --benchmark --require-gpu --validation --animation-fps=120
   --benchmark-load --require-gpu --validation --animation-fps=120
   --benchmark-soak --benchmark-seconds=300 --require-gpu --validation --animation-fps=120
   Preserve existing32MiB plateau, responsiveness p99<=50/max<=100ms and new distinctHz>=90%FPS gates. FPS is not latency; CPU elapsed is not GPU timestamps.
7. Check non-GPU Ui build and BLITZ compatibility. Add explicit enabled-shadow regression since default inventory might not exercise it.
8. Focused manual final app input/AA/speed acceptance and physical DPI if needed. Native CUA unavailable in current environment. User prefers specific targeted tests and may Escape app; ask whether observed app behaved, continue independent work.
9. Update actual evidence/docs/dashboard, review diff and commit only owned paths on main (Ui first; record dependency SHA). No push/publish. Do not claim current candidate clean-machine Karo run; old bundle qualification is historical.

## Build tools / commands

Approved Render Execute: upp-builder (E:/upp-18468/umk.exe), git, tool-7A0B237BB3E4 (build/*.exe, agent args,20min).
Method E:/apps/github/upp_render/CLANGx64_Vulkan.bm.
Console nests E:/apps/github/upp_render/render,E:/apps/github/upp_render/tests,E:/upp-18468/uppsrc.
GUI test nests E:/apps/github/upp_render/render,E:/apps/github/upp_render/tests,E:/apps/github/upp_Ui,E:/apps/github/upp_animation,E:/upp-18468/uppsrc.
Gallery uses examples instead of tests in nests.
UMK argv: [nests,package,method,"--out-dir",cache,"-r","+CONSOLE,CFONTS",output].
GUI flags +GUI,GPUUI,CFONTS; GUI release cache build/cache-directwrite-gui; console release build/cache-directwrite-release; Debug omit-r use build/cache-directwrite-debug. BLITZ -br.
Some old console packages lack conditional CFONTS RenderFontWin32 dependency; use non-CFONTS reference builds or explicit correct package dependency, never run old exe after failed link.
Stat output SHA then execute_start_tool(output_path,expected_sha256,argv). One Execute per project.
ShaderBake build/RenderShaderBake.exe SHA1ff780ea13ce24b7342aa0c11182d84354d6ffe16d8472384d863c69bbdda0dd, tool build/*.exe. Args [C:/VulkanSDK/1.4.350.0/Bin/shaderc_shared.dll,source,header,symbol]. SPIRV baked; no new end-user SDK dependency.

## Boundaries / review concerns

- Ordinary current Ui scalar text/DirectWrite metrics/outlines supported; full shaped glyph runs/bidi still OPEN. Do not silently redefine gate5.
- BeginMask, text-on-path, image-pattern stroke, some unsupported Painter operations fail explicitly. Image fill bounded256tiles/64axis.
- SoftwareUiRenderer ClipPath supported; BlurLayer explicitly GPU-required. Native software Ui fallbacks retained. GPU layered mask edge AA does not promise exact legacy per-primitive compositing.
- CPU old shadow fallback double-premultiplication behavior intentionally not changed. GPU follows neutral premultiplied contract. Test shadow explicitly.
- Ui shadow dynamic_cast<GpuPainterEffects&> assumes our recorder; consider safe optional capability for third-party Painter. Avoid broad scope expansion.
- Source bitmap decode/tint/preparation remains legitimate CPU asset work.
- Other docs still Oct15 6/10 (60%) historical acceptance. Do not claim new100% before qualification/shaping/DPI/manual gates. Keep exact denominator, evidence and limitations explicit.
- Dashboard dashboard-4ab27ccd4e88f640515329923bfc8637-A686904788EDBBDF15A5796581F90E51 revision16 at start; read fresh before update. Preserve other panels.
- Update docs/VULKAN_UI_FINISHING.md, GPU_VECTOR_COVERAGE.md, WINDOWS_FRAME_PACING.md, ACTIVE_WORK.md after final results. This checkpoint supersedes stale implementation prose, not historical evidence.
