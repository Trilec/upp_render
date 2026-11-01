# Windows/Vulkan release candidate

Current identity: [release/windows-vulkan.json](qualification/windows-vulkan.json).
Current runnable file: **bin/win64/UppVulkanUiDemo.exe**.

The whole-UI showcase is the default public demonstration. It uses Olive dark,
native Ui controls, a live inspector, ring metrics and a cache chart. The two
other product examples remain source packages; the small public-API tutorials
are copyable snippets.

## Stable executable policy

- Keep only the current Release executable in `bin/win64/`.
- Build and test a candidate under `build/` before promoting it.
- Replace `UppVulkanUiDemo.exe` only after it passes; keep its stable name.
- Update the manifest with its complete SHA-256, size, source/dependency pins
  and exact qualification jobs. A filename or modification time is not identity.
- Keep reports, manifests, licences and archives outside `bin/win64/`.
- Do not commit generated executables, caches or workstation build settings.

## Qualification boundaries

The 24 October full-UI candidate passed Release/Debug controls, SDK-free Caro UI
and compute, and five-minute RTX/Caro production tests. The refreshed graphical
demo has its own checks in the manifest; older measurements retain their original
executable identity. Historical RC1 evidence is [preserved here](qualification/history-2026-11-01.zip).

Strict acceptance is still **8/10 (80%)** until physical AA/control input, actual
OS IME candidate interaction and monitor-DPI acceptance are explicitly recorded.
The host requests system DPI awareness; live per-monitor transitions are not
qualified. [Manual checklist](WINDOWS_VULKAN_MANUAL_ACCEPTANCE.md).

CPU replay timings include elapsed CPU-side work and are not GPU timestamps.
The Iris Xe baseline presented 56.36 FPS while producing 119.35 distinct animation
updates per second; its passed cadence gate does not establish 120 presented FPS.
Cache payload limits do not measure total process, driver or VRAM allocation.

## Before public publication

1. Match the stable executable to the manifest and rerun the relevant current
   functional, AA/input/text and sustained checks after source changes.
2. Record remaining physical acceptance and known platform/text limitations.
3. Assemble applicable renderer, pinned Ui/Animation, U++, image/compression and
   compiler/runtime notices with the runtime archive. The executable-only local
   folder is not the complete public distribution.
4. Pin source dependencies and provide the portable [build instructions](BUILD_WINDOWS.md).
   Check the font fixture licence if distributing test sources.
5. Review the source diff and archive contents for local settings, generated
   output, credentials, unrelated working trees and stale candidate names.
6. Publish a clearly identified candidate; tagging, pushing and publication
   remain separate owner actions.

Applications need the graphics driver Vulkan runtime, not the Vulkan SDK.
Optional validation layers are required only for `--validation`.
WebGPU/Metal/Linux, async video/OCIO and broader shader effects are later work.

## What the hygiene pass retires

Six duplicate bring-up/stock-GDI demos and their private helper were removed; their
coverage lives in maintained examples and deterministic tests. The old
`tools/package_windows_v1.py` was retired because it selected three historical
build filenames and October RC1 evidence. It must not package the current build.
The old `out/` tree and disposable build caches/binaries were cleared after
preserving the small reports and Git history bundles. Core and Animation were not modified. Only the focused UiColorPicker.cpp
correction was applied to the shared Ui source; unrelated Ui and 3D work is preserved.
