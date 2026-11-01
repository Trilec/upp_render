# Vulkan whole-UI showcase

Build package: `GpuUiGallery`. Stable Windows executable:
`bin/win64/UppVulkanUiDemo.exe`. The default theme is **Olive dark**.

## What it demonstrates

The animated scene, PropertyEditor, menus, dropdowns and all client controls are
recorded by `GpuTopWindow` and drawn through Vulkan. The GPU dialog opens another
Vulkan-composited window sharing the device. The Scene menu opens the file
browser, full/micro colour pickers, text/IME editors and image/compute laboratory.
View > Light / Dark switches the same Ui theme authority used by owned dialogs.

`main.cpp` owns scene/control behavior and the deterministic qualification/load
modes. `GpuMetrics.h/.cpp` composes the existing UiProgressRing, UiChartRing,
UiPanel and UiLabel controls; it contains no custom rendering or timing loop.
Compute, capture and extended control qualification have their existing separate
source files. `GpuWidgets.h/.cpp` supplies the shared three-column Controls
page and workspace: two short tree groups, four table rows, an immutable sample
image, checkbox and progress bar. Production renderer code does not depend on these demo helpers.

## Read the meters

The existing status callback updates the controls at 4 Hz; presented FPS is sampled
over one second. These derived readings do not drive animation or scheduling.

| Meter | Arc scale | Numeric reading |
| --- | --- | --- |
| Presented FPS | Measured FPS / animation target | Actual presented FPS, even above target |
| CPU replay | Elapsed replay / target frame interval | Milliseconds, not GPU execution time |
| GPU paths | Last frame / this window's observed peak | Actual path count; not GPU utilization |
| Motion speed | Authored multiplier / 3.00x | Multiplier plus paused/live status |
| Cache chart | Relative image/vector/glyph payload bytes | Total MiB and image entry count |

The cache chart excludes driver allocations and total VRAM. The Ui controls own
theme resolution, text and arc painting. The gallery owns only layout and metric
projection; there is no second renderer, copied control implementation or worker.

## Modes

```text
UppVulkanUiDemo.exe --require-gpu
UppVulkanUiDemo.exe --text-controls
UppVulkanUiDemo.exe --qualify-ui
UppVulkanUiDemo.exe --capture-scenes
UppVulkanUiDemo.exe --benchmark-soak --require-gpu --benchmark-seconds=300
UppVulkanUiDemo.exe --benchmark-compute --benchmark-seconds=20
```

Automation closes itself and writes reports beside the executable. For normal
review, simply run the executable. Preserve reports before repeating a test.
Leave fixed-load runs open, unminimized and unchanged in size/settings.
`--validation` requires an optional developer layer and should be omitted on
SDK-free machines. See [physical acceptance](../../docs/WINDOWS_VULKAN_MANUAL_ACCEPTANCE.md)
for the separate AA, real OS IME and monitor-scaling checklist.

The outer page tabs explicitly use the compact underline variant. The colour
picker's own dense field/layout correction is in the shared Ui control and the
reproducible [qualified dependency patch](../../docs/dependencies/README.md).
