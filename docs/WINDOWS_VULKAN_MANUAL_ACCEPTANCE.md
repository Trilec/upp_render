# Windows/Vulkan physical acceptance

Use **bin/win64/UppVulkanUiDemo.exe**, with exact identity recorded in
[the current manifest](qualification/windows-vulkan.json). This checklist applies
to the refreshed demo; older launch details/results below retain their original
24 October candidate identity. Automated captures do not establish these checks.

## AA and control input

Inspect animated circles, curves and text at normal viewing size. Check smooth
edges without missing pixels or clipping. AA is applied by the renderer; this
gallery has no AA toggle. View > Light / Dark changes the theme.
The default window opens Roles & states. Change profiles and roles, type into
both panes, open dropdowns/menus, switch tabs and scroll the range/status controls.
Use Colour picker > Color to inspect all six readouts and switch to CMYK; also
check Image, Micro picker and File browser. Run --scene-inspector for the
animated scene and GPU dialog; check the second window, focus, editing and resize.
Record defects separately by control and theme.

## Real OS IME

Open Text & IME in that controls dialog. Use Win+Space to select an installed
Japanese, Chinese or Korean IME. In both the single-line field and larger editor:
compose text, choose a candidate and Enter to commit; then start a new
composition and Escape to cancel. Check candidate placement near the caret,
single insertion, preserved existing text on cancel, selection and backspace.
Record input language and results for each editor. Generated composition tests
do not qualify this check. If no suitable IME is installed, record unavailable.

## Physical display scaling

Use Windows Settings > System > Display > Scale: 100%, 150%, 200%, where
available. Record every actual scale tested, text sharpness, clipping, popup
position, caret/IME placement and hit testing in the main and controls windows.
Separate live scale changes from appearance after starting at the selected scale.
The read-only U++ host requests system DPI awareness in CtrlLib/ChWin32.cpp
(SetProcessDpiAwareness(1)); per-monitor dynamic scaling is not established.
A fresh launch may be needed for startup-scale checks. Moving both windows
between differently scaled monitors is a separate live-transition check;
do not count doubled application units as a physical DPI pass.

## Historical 24 October Caro fixed-load soak

Caro runtime: C:/GitHub/upp_render/build/runtime-2026-10-24-final/GpuUiGallery.exe,
same SHA-256. Job exec-E3654FF0F7C7D4CECCDBADD514B6DE32.
Arguments: --benchmark-soak --require-gpu --benchmark-seconds=300.
Four-second warmup, 300 measured seconds, 420-second execution bound.
Leave unminimized, do not close or change size, controls, rate or workload.
It may be covered by another window and closes itself after writing
GpuUiGallery-required-soak.txt. Review exit status, visible/minimized samples,
cadence, timer delays, memory plateau, GDI/fallback counters and final ownership.
Preserve failures and partial reports. Physical checks on Curt are independent.

## Historical results

Physical AA/control result: awaiting owner.
OS IME language and both-editor result: awaiting owner.
DPI scales and live/startup result: awaiting owner.
Caro final five-minute result: PASS. PASS: 300 measured seconds, 12001 visible/0 minimized/0 hidden samples; 56.36 presented FPS, 119.35 distinct animation Hz at target120. Timer p99/max19.68/35.44ms, zero measured >50ms events; private growth9694208B (9.25MiB), peak163762176B; one reused descriptor pool, GDI0/fallback0/finalZERO. Existing cadence gate measures animation updates, not presented FPS. 19666 cumulative dropped frames and acquire/replay/present CPU max122.09ms retained; no 120-presented-FPS claim.
Raw report: WINDOWS_VULKAN_CARO_FINAL_SOAK_2026-10-24.txt.


## Historical candidate and launch identity

Candidate: GpuUiGalleryWindowsV1Final.exe, SHA-256
65f771f7f1d2f7a1b4b7c451cce9bebec5ce9945bc842c44d17125bc703c683e.
Source commit: 7fc8e6a4ee61da43625e3e0ea05b48dd9f8711b9.
Curt interactive launch: exec-C1B093A9490589C36C66E8FAD9A03EB2, --require-gpu,
20-minute bound. Launch success does not establish physical acceptance.
