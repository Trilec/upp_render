# upp_render

GPU drawing and native Ultimate++ controls, with Windows/Vulkan as the current
qualified backend. Application code uses **GpuRender**; it does not manage Vulkan
instances, swapchains or command buffers.

## Run the Windows demo

The stable local executable is **[bin/win64/UppVulkanUiDemo.exe](bin/win64/UppVulkanUiDemo.exe)**.
This directory contains only the current Release executable. Future qualified
builds replace that same file; executables and compiler output are not committed.

The demo opens in **Olive dark**, with an animated Vulkan scene, a live inspector,
ring meters and a cache composition chart. Use:

- **GPU dialog** for a second Vulkan window containing editable controls.
- **Scene > Browser and colour pickers** for the file browser, full colour picker,
  micro picker and multilingual text/IME editors.
- **Scene > Image and compute** for GPU-generated images and a dithered ramp.
- **View > Light / Dark** to compare the two theme modes.

The client area, controls and owned dialogs use Vulkan drawing. The application
retains Windows windowing/input and DirectWrite text services. A compatible
Windows x64 machine needs its graphics driver's Vulkan runtime, **not the Vulkan
SDK, U++ or a compiler**. The optional `--validation` mode requires developer layers.

[Candidate identity and qualification](docs/qualification/windows-vulkan.json) records the
exact executable hash, source/dependency pins and test results.
[Demo behavior and metric definitions](examples/GpuUiGallery/README.md) explains
what the rings measure.

## Use it in an application

Add the `GpuRender` package and include `<GpuRender/GpuRender.h>`.

| Entry point | Application |
| --- | --- |
| `GpuCtrl` | An embedded GPU surface in an ordinary native UI |
| `GpuWindow` | A custom GPU window |
| `GpuTopWindow` | A control tree recorded and replayed through a root GPU surface |

Layout, focus, input and control state remain under U++/Ui authority. Compatible
presenters share the expensive device/context resources while owning separate
surfaces and swapchains. Images, glyphs, vectors and frame queues have explicit
bounds; failures in required-GPU mode are reported rather than hidden by fallback.

[Usage](docs/GPU_CTRL_USAGE.md) · [Integration](docs/GPU_CTRL_INTEGRATION.md) ·
[Package map](render/README.md) · [Examples](examples/README.md)

## Build and repository layout

See [Windows build instructions](docs/BUILD_WINDOWS.md). Copy the
`GitHubOut.var.example` and `CLANGx64_Vulkan.bm.example` templates into local
configuration and set the dependency/toolchain paths. The qualified Ui revision
is pinned in the candidate manifest; an unrelated Ui working tree is not a
drop-in replacement for that snapshot.

| Directory | Contents |
| --- | --- |
| `bin/win64/` | One current runnable Windows showcase |
| `build/` | Disposable outputs, the pinned dependency export and retained local evidence |
| `render/` | Public integration and renderer/backend packages |
| `examples/` | Three focused product demos |
| `tests/` | Deterministic regression and load packages |
| `shaders/` | Shader sources and checked-in generated SPIR-V |
| `tools/` | Development utilities |
| `docs/` | Guides, qualifications, dependency patches and compressed history |

## Scope and release status

Windows/Vulkan whole-UI automated qualification covers direct GPU vector/glyph
drawing, images, owned dialogs, browser/pickers, complex text and bounded load.
The earlier **24 October** build passed five-minute tests on both RTX 4070 Ti and
SDK-free Iris Xe machines. Its frame pacing/memory results remain versioned
evidence; the manifest distinguishes them from tests of the refreshed demo.

Strict whole-UI acceptance remains **8/10 gates (80%)**, with actual OS IME
candidate interaction and physical AA/input/monitor-DPI review recorded separately.
The host currently requests system DPI awareness; live per-monitor scaling is
not qualified. [Release checklist](docs/RELEASE_CANDIDATE.md) lists the remaining
public-release work. Documentation cleanup does not close acceptance gates.

WebGPU, Metal, Linux hosting, asynchronous video streaming and OCIO integration
remain later scopes. GPU timestamps and total driver VRAM accounting are not
available. [Documentation index](docs/README.md) · [Backend roadmap](docs/BACKEND_ROADMAP.md)

Apache-2.0; see [LICENSE](LICENSE). Runtime distributions must include the applicable
dependency notices described in the release checklist.
