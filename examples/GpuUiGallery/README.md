# Vulkan UI showcase

Package: `GpuUiGallery`. Stable executable: `bin/win64/UppVulkanUiDemo.exe`.

## Main control workbench

The default page is **Roles & states**, based on the Cinespace reference.
Dark and light panes remain visible together. A profile selector offers Olive,
Legacy, CineCast R9, CineWrite R4.1, CineFlow v30 and CineStage R3.
The four role buttons change emphasis while preserving values and selection.

Live specimens include ordinary/icon commands, selected/disabled buttons, a real
menu, editable/invalid/read-only fields, dropdown, toggle, checked/mixed checks,
radio choices, underline tabs, slider, progress, segmented range and status tags.
Each pane scrolls when its measured content exceeds the viewport.

Other pages retain the file browser, full/micro colour pickers, text/IME editors
and the compact three-column tree/table/image composition. These are native Ui
controls recorded through a Vulkan root, not screenshots or painted imitations.

`GpuRoles.h/.cpp` owns only the composition and callbacks. It uses the read-only
`Shared/UiTheme` package from upp_cinespace for actual profile recipes; it does not
link the Cinespace demo or copy the production theme implementation.
The renderer library itself has no Cinespace dependency.

## Scene inspector and metrics

Run `--scene-inspector` to open the particle scene and inspector.
Its GPU dialog opens a second populated Vulkan window. Scene menu entries open
the control workbench and image/compute laboratory. View switches Light/Dark.

`GpuMetrics.h/.cpp` composes the existing progress/chart rings and labels.
The existing status callback updates at 4 Hz; FPS is sampled over one second.

| Meter | Arc scale | Numeric reading |
| --- | --- | --- |
| Presented FPS | Measured FPS / animation target | Actual presented FPS |
| CPU replay | Elapsed replay / frame interval | Milliseconds, not GPU time |
| GPU paths | Last frame / observed session peak | Path count, not utilization |
| Motion speed | Authored multiplier / 3.00x | Speed plus paused/live status |
| Cache chart | Image/vector/glyph payload composition | Total MiB and image count |

The cache chart excludes driver allocations and total VRAM.
No second animation timer, renderer or copied ring implementation is introduced.

## Running and checking

```text
UppVulkanUiDemo.exe
UppVulkanUiDemo.exe --scene-inspector --require-gpu
UppVulkanUiDemo.exe --text-controls
UppVulkanUiDemo.exe --qualify-ui --validation
UppVulkanUiDemo.exe --capture-scenes
UppVulkanUiDemo.exe --benchmark-soak --require-gpu --benchmark-seconds=300
UppVulkanUiDemo.exe --benchmark-compute --benchmark-seconds=20
```

Qualification covers all picker pages, every colour channel in both theme modes,
48 paired profile/role/mode cases, retained edits/selection, actual inspector
colour dialogs, browser/micro picker, text composition and repeated GPU dialogs.
The readout regression first reproduced six 30-pixel fields requiring 36 pixels; CMYK also
exposed a missing parent relayout when changing from four to five channels.
Both fixes belong to the shared picker and the [qualified Ui patch](../../docs/dependencies/README.md).

Automation closes itself and writes reports beside the executable. Normal review
stays open until closed by the user. `--validation` needs developer layers;
omit it on SDK-free machines. Real OS IME/physical DPI remain
[separate acceptance](../../docs/WINDOWS_VULKAN_MANUAL_ACCEPTANCE.md).
