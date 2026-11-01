# Current work — 2026-11-01

The stable **bin/win64/UppVulkanUiDemo.exe** now opens Roles & states.
It reuses the Cinespace shared theme adapter for six profiles with paired
dark/light panes and all four roles. Commands, icons, selected/disabled states,
fields, validation, choices, tabs, slider, progress, range and status are live
Ui controls in a Vulkan window. Browser, full/micro picker, text/IME and a small
tree/table/image page remain separate tabs. Use `--scene-inspector` for particles,
metrics, the inspector, GPU dialog and image/compute laboratory.

The Colour-page clipping was reproduced as six 30-pixel fields requiring 36
pixels. The shared picker now applies compact recipes to those readouts and
measures their rows. Switching to five-channel CMYK also relayouts the parent;
it previously squeezed five 24-pixel editors into 22-pixel rows.

Automated UI validation passed in 21.094 seconds, including all four
picker pages, all seven channel modes in both themes and 48 profile/role/mode
cases. CPU vector/glyph rasters, GDI font requests and software fallback were
zero; final native ownership was zero. Captures show complete readout baselines.

[Exact identity and evidence](qualification/README.md) ·
[Build and dependencies](BUILD_WINDOWS.md) · [Release checklist](RELEASE_CANDIDATE.md)

Strict physical acceptance remains **8/10 (80%)**. Real OS IME candidates and
physical AA/input/monitor scaling are separate checks. The owner waived the
five-minute repeat; no completed soak is claimed for this new hash.
The host uses system DPI awareness; live per-monitor transitions are unqualified.

Core and Animation remain read-only. Only the focused Ui picker source was
changed; [its reproducible patch](dependencies/README.md) accompanies the pinned
Ui build. Shared Cinespace theme sources were read-only. Concurrent 3D work is
preserved and excluded from this binary. Historical records are compressed in
[the archive](qualification/README.md#history).
