# Windows/Vulkan continuation — 2026-10-24

## Current authoritative checkpoint

- Main base1d85294 plus exact11 source hashes in current_candidate of
  WINDOWS_VULKAN_CARO_QUALIFICATION.json. Source/evidence commit7fc8e6a4ee61da43625e3e0ea05b48dd9f8711b9.
- Final Release build/GpuUiGalleryWindowsV1Final.exe,8,911,872B,
  SHA65f771f7f1d2f7a1b4b7c451cce9bebec5ce9945bc842c44d17125bc703c683e.
- Release UI exec-097220385C651C781847A814B923CBDE PASS15.375s.
  Debug UI exec-C5015D2B10FF074C71BA167BA32C0BB6 PASS20.375s.
  Caro UI exec-BB1F98010C9F0820F241FC82D605D779 PASS13.078s.
- Actual inspector swatch keyboard activation/accept/cancel included; three
  workspace/compute cycles,browser/full/micro/text,GDI0,fallback0,finalZERO.
- Fixed maximizable ordinary editor dialogs:WS_POPUP disappears withZoomable.
  Before actual inspector:owner=true,surface1,242GDI. After:surface2/device1/GDI0.
  Default standard-dialog probe initially passed because its style was popup;
  speculative expansion was reverted, then correct maximizable case proved fix.
- Root lifecycle Release11.640s,Debug pass; embedded timer/lifetime Release2.797s,
  Debug3.547s pass. Source contains correct tests for resizable/nested dialogs.
- Root frame-wait budget resets on returning from independently timed modal
  stages. This fixed a Debug63.375s all-stages-pass/final-budget failure.
  Benchmark thresholds unchanged.
- Current Caro compute exec-1E4B0ADE50FF4B43EAF32DA64DD955C6 PASS60.02FPS,
  CPU4.36/6.58ms,callbackmax21.68ms,peak161329152B,one shader/image build.
- Current Curt production300s exec-B1714C77AD892BC8704C48EE6B2F0A6E PASS:
  119.99distinctHz/122.84FPS,timermax22.22ms,0events>50,privategrowth1661952B,
  onepool reused37269,GDI0/fallback0/finalZERO. No benchmark running.
- Prior Caro minimized/user-closed attempts cannot qualify visible cadence.
  Unminimized changing-workload test2CDB failed latency/memory/GDI; preserve.
  Original8058/c510 packets are PRE-FINAL, do not distribute as current.
- Strict8/10 (80%), text7/8 (87.5%); physical AA/input/OS IME/DPI still distinct.
  Owner manual inspector/workspace/animation pass recorded with exact scope.

## Access and immutable dependencies

KlickCurt DESKTOP-GFX1, project-028DDA9C9C89, E:/apps/github/upp_render.
Patch for writes; Execute for builds/runs/Git. One Execute per project.
KlickCaro DESKTOP-ASUSPN6, project-ED940563C935, C:/GitHub/upp_render.
Policy30, runtime-314D887CC5B0 nested*.exe,max1200000,args enabled.
Robocopy file-transfer-8B6A91A31C5F approved84abbd32...; codes0..7 accepted.
Share Curt F:/networkshare is UNC \\\\DESKTOP-GFX1\\networkshare on Caro.
Caro current exe build/runtime-2026-10-24-qualified/GpuUiGalleryWindowsV1Final.exe
matches65f7... . Runtime needs driver Vulkan, not compiler/SDK. --validation on
Caro correctly fails because optional developer layer is absent; omit it there.

Core E:/upp-18468/uppsrc and Animation E:/apps/github/upp_animation READ ONLY.
Ui export build/qualified-ui-main-text pinnedf88aab5150ca7f375b8c4a61a01da57832c82e03.
Independent Ui dc789c1 dirty refactor is preserved and excluded. Do not edit it.

## Completed handoff

Current Release/Debug/Caro fullUI,current Caro compute,final RTX300s and delivery
all PASS. Final archive/runtime paths and full hashes below. No test running.
Keep strict80%:physical AA/input/IME/monitorDPI not inferred from automation.
Caro visible300s remains unqualified; do not repeat minimized runs automatically.
Source/evidence commit7fc8e6a4ee61da43625e3e0ea05b48dd9f8711b9 is complete on main; no push/publish.

Build nests:
E:/apps/github/upp_render/render,E:/apps/github/upp_render/examples,E:/apps/github/upp_render/build/qualified-ui-main-text,E:/apps/github/upp_animation,E:/upp-18468/uppsrc
Method E:/apps/github/upp_render/CLANGx64_Vulkan.bm.
Release --out-dir build/cache-qualified-main-text -r +GUI,GPUUI,CFONTS.
Debug --out-dir build/cache-directwrite-text-debug +GUI,GPUUI,CFONTS.
Tests replace examples with tests,+GUI,CFONTS. No // in.upp.
WebGPU/Metal/Linux,OCIO/async streaming remain later scope.

Final ZIP:build/upp-render-ui-2026-10-24-final.zip,3884676B,
SHA4b08f8942347ab6ff4a5c981331bde564d5d46638423cba6dd35edf2bb21c5b4.
Share:F:/networkshare/upp-render-ui-2026-10-24-final.zip and extracted final folder.
Caro:C:/GitHub/upp_render/build/runtime-2026-10-24-final/GpuUiGallery.exe.
Original ZIP and executable independently hash-identical on Caro; no test running.
