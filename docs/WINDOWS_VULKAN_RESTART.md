# Windows / Vulkan continuation — 2026-10-22

## Scope and boundaries

Complete the Windows whole-Ui Vulkan application, efficient drawing/scheduling,
all controls/dialogs and the full native text/editing integration chosen by the
user. Work on main; local commits authorized; no push/publish. Core
E:/upp-18468/uppsrc and Animation E:/apps/github/upp_animation are READ ONLY.
Use KlickCurt Patch for mutations and Execute for builds/runs/Git changes.
Preserve foreign Ui edits.

## Current result — recover here

- Renderer base c1eb86f733dcf250ee14797fba0191aa732e825a; Ui main f88aab5150ca7f375b8c4a61a01da57832c82e03. Bitmap payload fix is committed. Current demo diagnostic change and evidence are being committed next.
- Shared DirectWrite/ICU layout, rich text, native IME composition, GPU glyph coverage and COLRv0/embedded bitmap fonts are implemented. Release/Debug pass64GPU/37editor/36style/27document/38IME/80font checks. SVG colour glyphs/COLRv1 paint trees excluded.
- Seven clean-main regressions,224 standard/224 doubled-application-scale catalogue cases pass. Physical monitor DPI is not certified.
- Current diagnostic build/GpuUiGalleryLatency.exe:8,890,368B,SHA b4a20e21e44b4dc3a71d6de61c7683391b64de20c9b6b0ab7409a55641134d49.
  Demo source SHA61f066ecf2bbf4c38dcda8073fbd196f3a4e360316d7863329eecaf2f1b900fe.
- Build exec-219F25937F9CD7FAE11240B32E80042D; whole-Ui run exec-EBB9FF90B29CD009E0E50293F35C75B9 PASS.
- Diagnostic production300s exec-0EE9981A861D6E6E11F788FFD44E7611 PASS:120distinctHz,121.82presentedFPS,CPUmedian/p993.73/8.26ms,timermax21.23ms,peak180785152B,early/late179234816/180143104B.
- Diagnostic validation-on300s exec-EA0315E4D0CC761F2AD3042371D56A1B PASS:120distinctHz,119.31presentedFPS,CPU5.88/13.29ms,timermax19.30ms,peak206610432B,early/late205028352/205964288B.
- Both record zero latency events above50ms. Warm CPU glyph/vector raster/upload/helper layers/fallback/GDI requests0; final tracked native ownership ZERO.
- IMPORTANT: Earlier exact c1eb86f candidate completed two300s runs that passed cadence/memory but FAILED max timer delay361.00ms validation-on and404.19ms off. Both failures are retained in WINDOWS_VULKAN_FINAL_QUALIFICATION.json. Cause remains unconfirmed; diagnostics did not change renderer behavior or thresholds. Do not claim a proven fix or erase failures.
- User confirmed closing the much earlier interrupted soak:INCOMPLETE/user-closed, not crash/pass.
- No benchmark is now running. Next:save final evidence/local commit, verify build against committed source, prepare CURRENT runtime, refresh dashboard, launch current controls for concrete human review.

## Acceptance still open

- Explain/resolve prior latency spikes or obtain the pending interaction evidence. The user's move/resize/menu question is pending.
- Physical controls/AA/100/150/200% monitor DPI and actual OS IME candidate interaction. Existing manual questions have no pass answer. Earlier180s timeout is not acceptance.
- Current clean-machine Caro qualification. Transfer tool rejected EXECUTABLE_CHANGED; owner approval question pending. Do not auto-refresh identity or use another route to bypass that rejected tool.
- Dashboard currently revision23,70% (7/10 strict gates). Text drawing gate5closed; gate9 remains open for unresolved repeatability. Gates7/10 physical review open. Current successful diagnostic runs must be reported alongside failures.

## Sources, assembly and tools

- KlickCurt host DESKTOP-GFX1, Render project-028DDA9C9C89,workspace folder-D804EA49C6B4.
- Committed Ui export build/qualified-ui-main-text (f88aab5),archive SHA4befa3631816a6c75833cc26f7bbcf8ce2c9d0e1a8802456a30fde025873a0c1;796 safe entries. Do not edit export.
- Gallery nests:E:/apps/github/upp_render/render,E:/apps/github/upp_render/examples,E:/apps/github/upp_render/build/qualified-ui-main-text,E:/apps/github/upp_animation,E:/upp-18468/uppsrc. Tests replace examples with tests.
- Method E:/apps/github/upp_render/CLANGx64_Vulkan.bm;cache build/cache-qualified-main-text;-r +GUI,GPUUI,CFONTS. Focused Debug uses existing cache-directwrite-text-debug and original Ui nest; its foreign working edits are disclosed in evidence.
- One Execute per project. Hash actual exe before tool-7A0B237BB3E4. Recover lost starts from retained jobs; never blindly replay.
- No // comments in .upp. No uses(GPUUI)RenderCore in Ui.upp:inactive uses still break native assemblies without renderer nests.
- Existing pre-diagnostic ZIP build/upp-render-ui-2026-10-22.zip SHA f969d30872fecdbfb40ce547bcc9cfeac5d4b3c579787942d0140d1010c0687f is a comparison artifact, NOT the current diagnostic handoff. Do not issue obsolete2026-10-04 ZIP instructions.

## Caro

KlickCaro host DESKTOP-ASUSPN6,Render project-ED940563C935,root C:/GitHub/upp_render.
Runtime runtime-314D887CC5B0 covers build/run/*.exe,max600s. No compiler/SDK needed.
Robocopy file-transfer-8B6A91A31C5F saved805d720d...,installed
84abbd32d074881164b8cf6b30593828e53f368c3854854aefe5b3cf06d3e356.
Owner must review/save changed identity. UNC share \\\\DESKTOP-GFX1\\networkshare
maps approved Curt F:/networkshare; F: does not exist on Caro. Native ZIP/copy grants
are ordinary programs, not Patch transactions. Validate trusted archives, avoid
overwriting unknown targets, independently hash delivered files.

## Preserve

Foreign Ui/UiThemeOlive.h,Ui/UiTitleCard.cpp/.h,Utilities/UiThemeTests/ThemeSurface.cpp,
docs/01_UI_CONTROLS_GUIDE.md,docs/02_UI_THEME_GUIDE.md,docs/ACTIVE_WORK.md and
Snapshot_FileBrowser.jpg. Read fresh status; stage only owned paths.
WebGPU,Metal,Linux,OCIO and asynchronous video remain later work. Do not claim
particle tests resolve the preserved CineView4K callback outlier.
