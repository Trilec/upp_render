# Windows / Vulkan continuation — 2026-10-23

## Current qualified checkpoint

- Strict acceptance8/10 (80%):1,2,3,4,5,6,8,9. Remaining7/10 require physical
  controls/AA, OS IME candidate interaction and physical monitor DPI.
- Renderer main base993fedfb3a20102797b19dd15a058446af803a11 plus exact file hashes
  in WINDOWS_VULKAN_LATENCY_INVESTIGATION.json. Local commit only; no push/publish.
- Ui tested export build/qualified-ui-main-text:f88aab5150ca7f375b8c4a61a01da57832c82e03.
  Independent Ui main advanced to dc789c1 (theme/title-card/gallery), clean.
  Do not silently substitute it into the qualified candidate.
- External E:/upp-18468/uppsrc and E:/apps/github/upp_animation READ ONLY.
- Proven lost wake:PostMessage pulse became old input after PeekMessage; ordinary
  MsgWaitForMultipleObjects ignored it, while one-pending coalescing suppressed
  fresh messages. Regression timed out202ms before fix; next wake2.2ms after.
- Fix:SendNotifyMessageW (different clock/UI threads), scalar generation, one
  pending notification, stop/join before HWND destruction; separate frame/timer
  callback guards. No external Core/Animation changes.
- Release exec-BDC9D3621E89C07554982FFAD282D8F7 and Debug
  exec-905445ADAA581DDE64B9AA6CC31E9D6C PASS lifecycle/regression, nested pumps,
  callback deletion and final native ZERO.
- Current build/GpuUiGalleryWakeFixed.exe:8,904,192B,
  SHA2d45c0120f073d6d26d71d1bcff5910b570a5fccdfd951ef56cf8d1467a9f813.
- Whole-Ui exec-3A030AFB66F69785AB31411F4DAED26A PASS browser/full/micro/text,
  three workspace/three compute cycles, shared device, GDI/fallback0, finalZERO.
- Current300s validation exec-07B46409F9D4E84F06A867A3D13461CF:
  120distinctHz,118.85presentedFPS,CPU6.14/13.74ms,timermax19.20ms,
  peak224542720B,early/late223115264/220366848B.
- Current300s production exec-2458C1E479F51F7E8FE1FDB20BDB5895:
  120distinctHz,123.85presentedFPS,CPU3.75/8.36ms,timermax18.95ms,
  peak208199680B,early/late193324032/203960320B.
- Both PASS unchanged thresholds,zero measured >50ms events,warm CPU rasters/
  GDI/fallback0,one reused pool,finalZERO. No timed benchmark currently running.
-4K compute20s exec-2E5BB82C49B69ED6ECF5BAF7AB742E8F PASS119.99FPS,
  CPU2.96/4.96ms,callbackmax25.45ms. Compute root does not use new clock;
  a short pass does not prove every historical outlier fixed.
- Fresh Vulkan root/dialog light/dark readback PASS; images inspected. This is
  offscreen replay, not physical scanout/keyboard/DPI acceptance.

## Evidence and limits

- WINDOWS_VULKAN_LATENCY_INVESTIGATION.json contains exact before/after jobs,
  binaries/sources,reports,native-menu/tooltip probes and runtime metadata.
- WINDOWS_VULKAN_FINAL_QUALIFICATION.json preserves older361/404ms FAILED soaks.
  They lacked clock tracing; do not retroactively claim all had the same cause.
- Earlier user-closed soak is INCOMPLETE,not a crash/pass.
- Seven baseline regressions,224 standard/224 doubled-unit catalogue and full
  native text Release/Debug matrix remain preserved. Text checklist7/8 (87.5%).
- Colour fonts:COLRv0/embedded bitmap supported; SVG glyphs/COLRv1 excluded.
- Cache payloads are not total COM/driver/process memory or VRAM measurements.

## Runtime delivery and Caro

- Current ZIP build/upp-render-ui-2026-10-23-wakeup.zip:3,875,443B,
  SHA798ce03a6109a709b91a0058eb8f5d8acaa782d2d7fab3d7536501f38340e096.
- Four files:GpuUiGallery.exe,README.txt,SHA256SUMS.txt,AGENT_TASK.txt.
  Instructions use this actual executable and preserve both soak reports;
  production/validation commands overwrite the same default report.
- Share folder F:/networkshare/upp-render-ui-2026-10-23-wakeup. Independent
  readback verification is recorded in the investigation manifest when complete.
- KlickCaro DESKTOP-ASUSPN6,Render project-ED940563C935,
  C:/GitHub/upp_render. Current build/run and older candidate exe absent.
- Runtime runtime-314D887CC5B0:build/run/*.exe,max600000ms. No compiler/SDK needed.
- Robocopy file-transfer-8B6A91A31C5F saved805d720d...,installed84abbd32...
  was rejected EXECUTABLE_CHANGED. Owner must review/save changed identity.
  Do not auto-refresh hashes or bypass the rejected executable.
- UNC \\\\DESKTOP-GFX1\\networkshare maps Curt F:/networkshare; no F: on Caro.
  No current Caro acceptance claimed. Do not reuse obsolete2026-10-04/22 tasks.

## Tools, source and next step

KlickCurt DESKTOP-GFX1:Render project-028DDA9C9C89,workspace folder-D804EA49C6B4.
Patch for mutations,Execute for builds/runs/Git. One Execute per project.
Hash actual exe before runtime. Recover lost starts from retained jobs,never replay.
Keep main only; local commits with description, no push or publication.

Gallery nests:E:/apps/github/upp_render/render,E:/apps/github/upp_render/examples,
E:/apps/github/upp_render/build/qualified-ui-main-text,
E:/apps/github/upp_animation,E:/upp-18468/uppsrc (one comma-separated argument).
Tests replace examples with tests.
Method E:/apps/github/upp_render/CLANGx64_Vulkan.bm.
Release cache build/cache-qualified-main-text,-r +GUI,GPUUI,CFONTS.
Lifecycle Release used +GUI,CFONTS; Debug cache-directwrite-text-debug
+GUI,GPUUI,CFONTS (Debug default,non-BLITZ).

Finish physical/current-runtime acceptance. If new latency occurs,retain exact
binary/report/interaction,inspect wake vs queue vs callback CPU separately.
Do not weaken gates or blame computer use without evidence.
No // comments in .upp; no inactive uses(GPUUI)RenderCore in Ui.upp.
WebGPU/Metal/Linux and OCIO/async video remain subsequent work.
