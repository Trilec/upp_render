# Developer tools

- `RenderShaderBake`: shader preparation tooling; generated shader artifacts and
  source live under `shaders/`.
- `VulkanProbe`, `VulkanSurfaceProbe`: focused device/surface diagnostics.
- `audit_ui_drawing.py`: static Ui dependency observations; not GPU acceptance.

Build tool executables only when needed and keep them under `build/`.
The current release procedure is [the candidate checklist](../docs/RELEASE_CANDIDATE.md).
The October RC1 packaging script was retired rather than left selecting obsolete
executables and evidence. Public runtime archives must contain the stable
qualified executable, its manifest/readme and the applicable dependency notices.
