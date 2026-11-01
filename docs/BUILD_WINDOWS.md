# Building on Windows

The qualified configuration uses U++ 18468, clang x64, Vulkan SDK headers/libraries
and the pinned Ui/Animation revisions recorded in [the manifest](qualification/windows-vulkan.json).
Building requires the toolchain/SDK; running the resulting application does not.

1. Install/configure U++ and the Windows Vulkan SDK.
2. Obtain Ui and Animation at the recorded revisions. Use a separate immutable
   export if the development Ui checkout contains unrelated edits.
3. Copy `CLANGx64_Vulkan.bm.example` to `CLANGx64_Vulkan.bm`, replace the
   placeholder compiler/SDK paths, and configure it in TheIDE.
4. Copy `GitHubOut.var.example` to `GitHubOut.var` and set the source nests.
   These two workstation files are local and ignored by Git.

A release build may select the committed renderer snapshot at
`build/qualified-render-main/render` while unrelated renderer edits are in progress.
It contains the manifest's renderer-base commit (render/ and shaders/) plus the
recorded RenderGpu2D cache-policy delta. The manifest records that file's raw hash.
The final source commit includes this delta; do not build the unmodified base
and claim the refreshed binary's qualification.

The assembly resolves packages in `render`, `examples`, `tests`, `tools`,
the Ui repository/export, upp_cinespace, Animation and `uppsrc`.
The showcase uses `Shared/UiTheme` from upp_cinespace for the requested theme
profiles. The renderer packages do not depend on that application adapter.
Match its recorded source hashes as well as its commit: the qualified API 3
adapter includes working-tree changes. It is a build dependency only; runtime
users still need only the graphics driver. UMK accepts the nests as a
comma-separated argument; the output directory is not a source nest.

```text
umk <comma-separated-nests> GpuUiGallery <local-method.bm> --out-dir build/cache-showcase-final -r +GUI,GPUUI,CFONTS build/UppVulkanUiDemo.exe
```

CFONTS selects the DirectWrite/native text route used by this candidate. Build
Release with `-r`; omitting it selects Debug. Avoid changing or rebuilding the
independent Ui working tree just to reproduce this candidate.

For this maintainer checkout, `build/qualified-ui-main-text` is the qualified
Ui export. It can be recreated from the recorded Ui Git commit into an empty
folder using `git archive --format=zip --output=<archive.zip> <commit>`, followed
by ZIP extraction. Apply the focused [Ui patch](dependencies/README.md)
to that export before building. Do not substitute an arbitrary newer checkout silently.

Test the build before replacing the stable file:

```text
build/UppVulkanUiDemo.exe --qualify-ui
build/UppVulkanUiDemo.exe --capture-scenes
build/UppVulkanUiDemo.exe --benchmark-soak --require-gpu --benchmark-seconds=300
```

Preserve reports, verify the executable hash and promote it to
`bin/win64/UppVulkanUiDemo.exe`. Update `docs/qualification/windows-vulkan.json` at the same
time. A running Windows process must close before replacement.

`out/` is retired. `build/` contains only current outputs/cache, the pinned
dependency export, retained dependency source checkouts and small `preserved/evidence` reports/history bundles.
Compiler caches can be regenerated; unique Git bundles and measured records
should be preserved before a future full cleanup.
