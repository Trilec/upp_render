# Current work — 2026-11-01

The stable Windows demo is **bin/win64/UppVulkanUiDemo.exe**.
[Exact identity and evidence](qualification/README.md) ·
[Build](BUILD_WINDOWS.md) · [Release checklist](RELEASE_CANDIDATE.md)

The Olive showcase now has graphical status rings, a cache chart and one compact
tree/table/image Controls page. The outer tabs use the underline style.
The shared colour picker owns compact dropdown/edit/toggle recipes and theme
accent navigation, fixing the clipping reproduced by the Image-page regression.

Automated whole-UI validation passed in16.875s with zero GDI font requests,
CPU vector/glyph rasters or software fallback, and zero native ownership at exit.
The vector cache now limits native texture count to256 as well as32MiB pixel
payload. Private samples stayed near189MiB through178.5s of a partial run.
The owner closed/used that run and waived its five-minute repeat; a completed
refreshed memory/responsiveness gate is therefore **not established**.

Strict physical acceptance remains **8/10 (80%)**. Remaining items are real OS
IME candidate interaction and physical AA/input/monitor-scaling review.
The host uses system DPI awareness; live per-monitor transitions are not qualified.

Next release work is the [manual checklist](WINDOWS_VULKAN_MANUAL_ACCEPTANCE.md)
and applicable dependency/runtime notices. WebGPU/Metal, CineView/OCIO and active
3D development are separate scopes. Core and Animation remain read-only;
the focused Ui correction is [reproducible here](dependencies/README.md).

Older work logs and qualification reports are preserved in the
[history archive](qualification/README.md#history), rather than competing with
this current note. Concurrent3D source, documentation and build output are retained.
