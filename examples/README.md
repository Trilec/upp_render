# Examples

The primary showcase is [GpuUiGallery](GpuUiGallery/README.md), with a single
stable executable at `bin/win64/UppVulkanUiDemo.exe`. It opens the paired Cinespace Roles & states workbench. Its Collections tab and GPU
workspace share one lightweight three-column tree/table/image composition.

Two focused examples remain: [GpuSurfaceDemo](GpuSurfaceDemo/README.md) for
adjustable embedded surfaces and AA/lifetime comparison, and RendererShowcase
for software/GPU drawing comparison. RendererShowcaseScene is shared scene data,
not another application.

The three minimal entry-point tutorials are now [source snippets](../docs/snippets/README.md),
rather than extra demo projects. Six superseded bring-up/stock-GDI examples and
their helper were retired. The raw precision-image caller moved to
`tests/GpuImageSourceTest`; its source and coverage are retained.
