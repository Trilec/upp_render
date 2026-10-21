# Windows/Vulkan restart checkpoint

Updated 2026-10-22. Resume the existing request in this chat. Overall Windows completion remains in progress (7 of 10 acceptance gates; 70%). The user has authorized full complex-script text support now. No further scope or permission decision is pending.

## Authorization and boundaries

Work on main. Local commits are authorized; the user reserved publishing, so do not push or publish. No subagents. Core E:/upp-18468/uppsrc and Animation E:/apps/github/upp_animation are READ ONLY. Ui implementation changes are authorized.

Use KlickCurt Patch for mutations, Execute for builds/runs/Git mutations, Dash for the existing dashboard. Local shell is for read-only observation. Read klick-project-workflow and upp-ui-development skills. Preserve unrelated/concurrent Ui work.

Render project project-028DDA9C9C89, workspace folder-D804EA49C6B4, E:/apps/github/upp_render.
Ui project project-B3BB7BB32A38, workspace folder-B2DAFD8124F1, E:/apps/github/upp_Ui.
Host DESKTOP-GFX1; Klick 1.0.35 source ca53cec1917ea91498d2feb3f1760431053bb09d; executable SHA 9bb30d69d67e0cd0a1a01816143d40f7ee23ab7b3c28a3cfc81eac7134bd803f.
After restart use klick_resume_context; recover retained jobs before replaying anything. Strip BOM before Patch rewrite; preserve newline style; preview/apply with exact complete-file hashes and digest.

## Qualified and committed drawing/pacing baseline

Ui drawing commit 228a1b968e0b13ef21b4cbb4baf1115c3c7a8686.
Render baseline commit 2ebf036506f612e20d24e3786a498a61a5d655a7.
Current renderer foundation is committed at 3598226; rich-text renderer integration is pending its local commit.
Ui HEAD observed cf237c78e4ef0155b18fa077adc5ca45b7e86604 after concurrent work. Re-read current state.

Exact baseline sources, jobs and raw results: docs/VULKAN_UI_FINISH_QUALIFICATION.json.
Gallery baseline build/GpuUiGalleryFinish.exe, 8296960 bytes, SHA 9a4d8f6fc61713e7d8e04dba22289380462219e825325d96939da994420efb57. Preserve this qualified binary while text work is developed separately.

Baseline passed:
- 56 drawable catalogue entries, two sizes and enabled/disabled = 224 drawing probes; 8 explicit shadows; table edits.
- Release/Debug semantic Painter/general vector checks, SVG/Unicode tests, non-CFONTS Canvas/Vector, root callback destruction; ordinary non-GPU/no-render-nest UiLabelDemo Release BLITZ.
- Main/GPU-dialog workspace and compute cycles; browser scan/64x32 WIC preview/selection/accept/cancel/filter popup; 16 full-picker states; micro picker/nested colour popup; themes/resize. Corrected readback PNGs inspected.
- Normal: 120.00 distinct Hz, CPU median/p99 5.04/8.31ms.
- Heavy 512 + grid: 120.02 distinct Hz, CPU 6.26/10.65ms.
- FULL repeat 300s soak PASSED, job exec-A5DF4ECEC35AAFA198CD9F13DC560641, 304422ms. 119.92 distinct Hz, CPU median/p99 6.15/14.27ms; timer p99/max 17.98/79.40ms against unchanged 50/100ms gate. Private bytes early/late 198714368/197966848 (decreased 747520), peak 198815744. U++ heap is only a subset, not process memory/VRAM.
- Baseline production CPU vector/glyph rasters, helper raster caches, software layers, GDI fonts, fallback all zero; final native ownership zero.
- First 193.937s soak was CLOSED BY THE USER. Incomplete, not a crash or pass. User reconfirmed this in the latest message.

## New full text foundation (uncommitted)

Ui/UiTextLayout.h/.cpp and UiTextLayoutInternal.h/.cpp provide immutable DirectWrite layout, exact U++ scalar/UTF-16 mappings, system ICU grapheme/word boundaries and full paragraph direction (including isolates). Windows 10 1903+ system icu.dll; no shipped ICU binary/SDK headers. Current OS Unicode 13.0.0.0. Cache 256 entries / 4MiB declared payload. Input <=32768 scalars, <=1024 paragraphs. These are payload bounds, not measured native COM/process memory.

render/RenderCore/GpuTextLayout.h/.cpp holds immutable actual font face / glyph IDs and positions. Explicit DrawShapedText operations now traverse Canvas, region recorder, semantic Painter, software reference, Vulkan geometry and shared bounded glyph atlas. Legacy scalar DrawText remains separate. Text outlines/coverage are GPU-generated, no whole-text CPU image. Fractional placement uses 1/64 pixel atlas phases. Native glyph subdivision matches Painter's existing 0.3 tolerance. Fractional underline/strike-through uses GPU area coverage at the fixed 32-sample font quality.

Independent foundation PASSED:
- Build exec-26F0B69CFEE565EAA757247FA6A74500.
- Run exec-ED7D356BF776E029747AC9CA4652584F, exit 0, 1610ms, no truncation.
- Binary then build/UiTextLayoutTest.exe, 5361664 bytes, SHA ba2467af23e4fca270497119ffdff2f38d5bb3f8733e843f6fa9759ed941666e.
- 7 grapheme fixtures, Arabic/Indic/mixed RTL/combining/supplementary/fallback text; caret/hit/selection/cache/visual movement/wrapping tests.
- 14 complete DirectWrite run outline comparisons, worst coordinate discrepancy 0.00000525px.
- 56 actual Vulkan pixel tests (7 strings x plain/styled x 4 colour formats), worst max/mean linear pixel error 0.024053/0.00016382 within 0.04/0.001 gates. Cold CPU glyph rasters 0, warm atlas misses/rasters 0, identical warm pixels; final adapter resources and Vulkan validation messages 0.
- Preserved report/limits in docs/WINDOWS_VULKAN_TEXT_QUALIFICATION.json. Raw build/UiTextLayout-qualification.txt will be overwritten on later runs.
- Reference corrections: skip empty whitespace outlines (Painter Fill otherwise reuses prior path); native complete-run outline geometry is the placement oracle, and per-glyph alpha compositor is the separate atlas pixel oracle. Whole-run union edge differences are diagnostic. No existing baseline tolerances were weakened.

Current integration checkpoint (still uncommitted):
- Release build exec-75E6BF0D43A489BB34F4A1A3A768774D; run exec-B708A7E396E38101519B55111B1468BE, exit 0, 2781ms. Binary 5956608 bytes SHA c631e0dc5b628c56440877abfc00b7b8ac311c92f33b2ff6d76d85053c0f4e4c.
- 16 native outline / 64 real Vulkan pixel cases, including Bahnschrift variable face/weight, and 37 editor interaction/lifetime checks PASS. Every pixel now uses an independent numerical non-zero winding coverage oracle; Painter has an overlap discrepancy, so its pixel output is diagnostic. Maximum/mean error gates remain .04/.001. Exact report and owned source hashes in WINDOWS_VULKAN_TEXT_QUALIFICATION.json integration_checkpoint.
- Font identity includes canonical variable-font axes. Atlas fractional origin is quantized at 1/64px. Cold coverage remains GPU; warm misses/raster0, final GPU/validation0.
- 29 plain-control/helper files use UiGetTextSize/UiDrawText; elision uses grapheme-safe UiFitText. BaseEdit retains unchanged whole-line layouts, uses ICU source boundaries even for passwords, visual bidi navigation, grapheme deletion/overwrite, selection clipping, caret/hit/vertical movement and Unicode whitespace word movement. Ligature caret policy evenly subdivides native advance across graphemes; half-ligature selection shares those same positions. Callback-destruction guards tested.
- UiLabel selection/clipboard is now Unicode scalar based; uniform label selection reuses whole layout. Rich pieces still need shared styled paragraphs.
- Current Debug build exec-04F71759CAD71B74C8C5CC1362FFD7A1 and run exec-E8A113732560D1B76A81BA723C091DE0 PASS (37 editor / 64 GPU cases). Native ordinary no-render-nest Release BLITZ build exec-800CC8D2BABB73CC4EB786EAC3871E29 PASS (9691648-byte UiLabelNativeText.exe). Catalogue run exec-26CF6FD19FB4740F5578DB99E9D2D087 PASS (224/224 probes, GDI-font and validation0). Native visual/complex-script rich-doc review not implied.
- U++ .upp files do not accept // comments. Do not add them. Do not add uses(GPUUI)RenderCore to Ui.upp: UMK resolves inactive uses and breaks no-render-nest native assemblies. GPU hosts supply RenderCore via their assembly/dependencies.

## Rich text checkpoint (2026-10-22)

Ui main commit 594ba7d follows 83ac3bd. Renderer foundation main commit 3598226 follows 2ebf036; current renderer rich changes will be committed next. No push/publish.
- Native styled scalar ranges (font/ink/tracking), physical alignment/character wrap, per-line metrics and inline object placements. Immutable line snapshots reuse actual font faces, avoid replaying whole paragraphs for each line, and are included in declared cache payload. Single-line layouts alias the full snapshot.
- Per-run and decoration ink travels through Canvas/recorders/semantic Painter/software/Vulkan; uniform-ink override recolours selection without reshaping.
- Rich labels share one styled native line across spans/icons/bullets and map native placeholders to clipboard scalar positions. Font defaults corrected: span Font is Null and old zero Font inherits base; DirectWrite styled size never receives zero.
- UiDoc paragraph and table cells use shared native layout for drawing, wrapping, caret, hit, selection, visual movement and grapheme deletion. Table selected-range removal is linear in runs. Metadata text uses native wrapped lines; card and marker outlines use GPU artwork instead of CPU images. UiDoc now overrides Ctrl::GetCaret for IME placement.
- BaseEdit GPU cache no longer computes redundant per-scalar native widths. TitleCard and Olive text calls migrated; only owned text hunks were staged in those concurrently dirty files.
- Release exec-5C1231F9493C8DB89E7D4906C9FCDAF0 PASS 2828ms, 6448640B SHA 54bfe7c2998c2899a51401d72e1fe6ce6dbc5485b48bf65a1696848e09a244b9.
- Debug exec-9923DF11AC312BE7FF23117C27F3E573 PASS 10282ms, 8920064B SHA 4525ebd928f8716da9c6741a023850edf38923e5577ec8c5ff867d69a0897105.
- Both: 16 native / 64 GPU pixel cases, 37 basic editor, 36 styled, 27 rich document/table/expanded metadata checks. Retained line-slice pixels equal whole paragraph pixels. GPU glyph coverage CPU0; metadata CPU vector/helper raster0; final GPU/validation0.
- Native no-render-nest BLITZ exec-BD0AF217D367CAF33761115CD577A97F PASS. Actual 224 control probes exec-9DEDD9CAC485EED8A38AB7FB358B8F0F PASS, GDI-font/validation0.
- Readback build/UiTextLayout-document.png inspected after named-channel capture fix. Exact hashes/evidence in WINDOWS_VULKAN_TEXT_QUALIFICATION.json rich_text_checkpoint.
- Dashboard revision19: overall strict acceptance70% (7/10), text implementation62.5% (5/8). Do not mark final text gate complete.

## Remaining implementation and qualification

1. Complete renderer rich commit/evidence and verify no owned code remains uncommitted. Preserve foreign dirty files. Temporary Ui/.renderer-text-only.patch was used solely to stage owned TitleCard/Olive hunks; rollback of its create transaction requested, verify removed.
2. Implement colour/bitmap font layers; current emoji is outline monochrome. Preserve existing monochrome geometry oracle as an explicit mode and add separate colour/bitmap pixels/caches/ownership tests.
3. IME composition remains open. Read-only Core Win32Proc converts WM_CHAR surrogate pairs and IME results to UTF32, but its app-owned PreeditCtrl draws legacy scalar text/prefix metrics. UiBaseEdit has GetCaret; UiDoc now has GetCaret override. Returning Null GetPreedit is NOT a fix: Core PreeditSync disables ImmAssociateContext when absent. Do not edit Core. A small renderer-host IME bridge and Ui-owned composition capability can intercept WM_IME_COMPOSITION before Core, use shared layout for preedit, leave native candidate selection to Windows, and commit atomically. This is a proposed next implementation, not completed.
4. Broaden bidi affinity/selection collapse/wrapped edge/table navigation and cache-bound/failure/concurrency/memory tests. Table image/caret pagination text_origin now shifts with cell rect.
5. Final Debug/native BLITZ, Canvas/semantic/vector/scalar regressions; rebuild Gallery preserving GpuUiGalleryFinish.exe. Re-run all controls/browser/full picker/micro/dialog/captures/input/DPI; normal/heavy/full300s after complete text.
6. Final evidence/docs/dashboard and local commits; no100% claim or publishing until acceptance passes.

## Unrelated Ui files to preserve

Ui/UiThemeOlive.h, Ui/UiTitleCard.cpp/.h, Utilities/UiThemeTests/ThemeSurface.cpp, docs/01_UI_CONTROLS_GUIDE.md, docs/02_UI_THEME_GUIDE.md, docs/ACTIVE_WORK.md, untracked Snapshot_FileBrowser.jpg. Re-read git status; other agent may advance main. Do not stage those whole files.

## Build / run

Render approved tool upp-builder = E:/upp-18468/umk.exe; method E:/apps/github/upp_render/CLANGx64_Vulkan.bm.
GUI test nests: E:/apps/github/upp_render/render,E:/apps/github/upp_render/tests,E:/apps/github/upp_Ui,E:/apps/github/upp_animation,E:/upp-18468/uppsrc.
UMK argv [nests,"UiTextLayoutTest",method,"--out-dir","E:/apps/github/upp_render/build/cache-directwrite-gui","-r","+GUI,GPUUI,CFONTS","E:/apps/github/upp_render/build/UiTextLayoutTest.exe"].
Gallery nests examples instead of tests.
Debug omit -r and use separate cache-directwrite-debug. BLITZ -br.
After build success, Patch stat(hash:true) actual executable then Execute tool-7A0B237BB3E4 with output_path build/UiTextLayoutTest.exe and expected_sha256. One Execute per project. Git tool ID git.

Dashboard dashboard-4ab27ccd4e88f640515329923bfc8637-A686904788EDBBDF15A5796581F90E51 last saved revision19; read fresh before updating, preserve panels. Overall remains 70% (7/10) until full text, physical/final acceptance gates pass.

Primary references: Microsoft DirectWrite IDWriteTextLayout, IDWriteTextRenderer, IDWriteFontFace::GetGlyphRunOutline, HitTestTextPosition; Windows ICU C ABI docs; Unicode ICU ubidi full paragraph direction. Do not write bespoke Arabic/Indic/bidi/range tables.
