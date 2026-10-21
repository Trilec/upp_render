# Windows / Vulkan continuation — 2026-10-22

## Scope and boundaries

Finish the Windows whole-Ui Vulkan application, efficient drawing and scheduling, all controls and dialogs, and the full native text/editing integration selected by the user. External E:/upp-18468/uppsrc and E:/apps/github/upp_animation are READ ONLY. Work on main only; local commits authorized; no push/publish. Use KlickCurt Patch for writes and Execute for builds/runs/Git mutations. Preserve foreign Ui edits.

## Current source and verified checkpoints

- Renderer base 5e63bb70482a1e42495206f77fcb23d2b114b7d0; Ui f88aab5150ca7f375b8c4a61a01da57832c82e03. Latest bitmap payload accounting fix is being committed with this checkpoint.
- Shared DirectWrite/ICU shaping, rich paragraphs, grapheme/bidi editing, Vulkan glyph coverage, native IME bridge and overlay are implemented. COLRv0 palette fonts and embedded PNG/JPEG/TIFF/BGRA assets supported. SVG glyphs and COLRv1 paint trees are not implemented.
- Bitmap payload regression reproduced a false 4 MiB rejection for 64 repeats of one native bitmap glyph. Fixed by counting each shared immutable Image serial once per snapshot. Distinct 600x600 images still exceed and are rejected at the unchanged 4 MiB limit. Snapshot run/glyph limits unchanged.
- Release exec-AAC985BBBDE997C20E5ACC2C109FC831 and Debug exec-08C2979A41667D114DAED4EFDAAD77B8 pass: 64 GPU pixel,37 editor,36 styled,27 document,38 IME and80 font-layer checks. Exact binaries/jobs/output in WINDOWS_VULKAN_FINAL_QUALIFICATION.json.
- Legacy IME CS_INSERTCHAR/CS_NOMOVECARET and native surrogate checks are committed in 5e63bb7. Both Release/Debug pass.
- Standard and doubled application-scale catalogue:224/224 each, GDI-font0, validation0. Jobs exec-C8BB19367D08982CC696D3076BEF06B5 and exec-1A9792B4941756CB209D9D311D662BCB. This does not certify physical Windows monitor DPI.
- Seven committed-main regression suites passed. Existing GalleryQualified.exe baseline and all exact evidence are preserved in WINDOWS_VULKAN_FINAL_QUALIFICATION.json.

## Final candidate work — resume here

1. Freeze implementation after bitmap payload fix; rebuild build/GpuUiGalleryFinal.exe from committed Ui export. Earlier preliminary Final.exe predates this fix and must not be used as final.
2. Hash executable, run --qualify-ui --validation. This covers browser/full colour/micro picker,3 workspace and3 compute dialog cycles and composition scene preservation.
3. Refresh final-binary normal/heavy20s and full300s soak. Include validation-off production measurement to resolve driver/layer footprint. No edits during timed runs. Reports are overwritten; preserve each immediately with binary hash.
4. Clean-machine qualification on KlickCaro through current build/run runtime grant. No compiler or SDK installation. Deliver CURRENT candidate folder via approved F:/networkshare / UNC share, verify hash before execution; never reference the obsolete2026-10-04 ZIP as current.
5. Reconcile manual controls/real IME questions and physical DPI; never infer human acceptance from a timeout. Update evidence, finishing status and dashboard, then local commits only.

## Preserved performance baseline

GalleryQualified.exe SHA2a777e3af816248f9ab4212fd6fb7d478b7af9571fe3b0141ca8d22e760981e8 (renderer76fe04e, Ui exportf88aab5) passed normal/heavy20s/full300s with validation. Heavy soak119.00 presentedFPS,120 distinctHz, CPUmedian/p99 5.98/13.51ms, maximumtimer18.49ms. Peakprivate223375360B, early/late205137920/222807040B; step near50s then plateau; growth below unchanged32MiB gate. Warm CPU vector/glyph raster/upload0,GDI0, finalnativeZERO. These figures belong to the preserved earlier binary, not the final rebuilt candidate.

User confirmed closing the earlier interrupted soak. Classify that run INCOMPLETE/user-closed, never crash/pass. Manual text-controls exec-828E95C4F8010B8364B0F9978FC64CBA reached bounded180s timeout; human checks and optional real OS IME remain pending.

## Build and connection details

- KlickCurt host DESKTOP-GFX1, Render project-028DDA9C9C89, workspace folder-D804EA49C6B4. One Execute per project.
- Committed Ui export build/qualified-ui-main-text from f88aab5; archive SHA4befa3631816a6c75833cc26f7bbcf8ce2c9d0e1a8802456a30fde025873a0c1,796 safe entries. Do not edit export.
- Gallery nests: E:/apps/github/upp_render/render,E:/apps/github/upp_render/examples,E:/apps/github/upp_render/build/qualified-ui-main-text,E:/apps/github/upp_animation,E:/upp-18468/uppsrc. Tests replace examples with tests.
- UMK method E:/apps/github/upp_render/CLANGx64_Vulkan.bm; Release --out-dir build/cache-qualified-main-text -r +GUI,GPUUI,CFONTS.
- Debug focused text uses existing build/cache-directwrite-text-debug, original Ui nest, omit -r. Foreign Ui edits must be disclosed in Debug evidence.
- Before runtime: Patch stat(hash:true), Execute tool-7A0B237BB3E4 with actual output_path/expected_sha256.
- KlickCaro host DESKTOP-ASUSPN6, Render project-ED940563C935, C:/GitHub/upp_render. Runtime runtime-314D887CC5B0 covers build/run/*.exe, max600s. Approved Robocopy file-transfer-8B6A91A31C5F. UNC source \\\\DESKTOP-GFX1\\networkshare. No SDK/toolchain there.
- Native ZIP/copy grants are approved programs with ordinary OS access, not Patch transactions. List trusted archives first, unique destination, then independently hash delivered files.
- .upp accepts no // comments. Do not add uses(GPUUI)RenderCore to Ui.upp; inactive uses still break native assemblies without renderer nests.

## Remaining acceptance and preserved work

Dashboard ID dashboard-4ab27ccd4e88f640515329923bfc8637-A686904788EDBBDF15A5796581F90E51; read latest revision before update. Last known21: overall70% (7/10 strict gates), text75% (6/8). Close gates only with their actual evidence. Live OS IME candidate input and physical DPI/manual review remain separate from automated composition/scaling.

Preserve foreign Ui/UiThemeOlive.h, Ui/UiTitleCard.cpp/.h, Utilities/UiThemeTests/ThemeSurface.cpp, docs/01_UI_CONTROLS_GUIDE.md, docs/02_UI_THEME_GUIDE.md, docs/ACTIVE_WORK.md and Snapshot_FileBrowser.jpg. Check fresh status; stage only owned paths.

WebGPU, Metal, Linux and CineView OCIO/streaming remain future work. No claim that particle tests resolve the separate CineView4K callback outlier. Historical text and baseline evidence stays in WINDOWS_VULKAN_TEXT_QUALIFICATION.json and VULKAN_UI_FINISH_QUALIFICATION.json.
