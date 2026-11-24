# Regression and load packages

These packages exercise different renderer contracts.
They are source tests, not additional release executables. Build only the targets
needed for a change; keep their binaries in `build/`.

| Area | Packages |
| --- | --- |
| Neutral drawing | RenderCanvasTest, RenderCtrlBridgeTest, RenderImageTest, RenderTextTest, RenderVectorTest, RenderSemanticPainterTest, RendererShowcaseTest |
| Backend contracts | GpuBackendRegistryTest, RenderRhiTest, RenderSampledRhiTest, RenderPlatformWin32Test |
| GPU 2D replay | RenderGpu2DTest, RenderGpuTextTest, RenderGpuVectorTest, GpuCtrlReplayTest |
| Vulkan resources and drawing | RenderVulkanTest, RenderVulkanResourceTest, RenderVulkanGraphicsTest, RenderVulkanImageTest, RenderVulkanPathTest, RenderVulkanVectorTest, RenderVulkanGeneralVectorTest, RenderVulkanPrecisionTest, RenderVulkanComputeTest, GpuImageSourceTest |
| Vulkan lifecycle | RenderVulkanFrameTest, RenderVulkanClearFrameTest, RenderVulkanPostCreateCleanupTest |
| 3D viewport | RenderSceneTest, Ui3DViewCameraTest, Ui3DViewTest, Ui3DMaterialTest, RenderVulkan3DTest |
| Native presentation | GpuCtrlHeaderTest, GpuCtrlFrameSourceTest, GpuCtrlPresentationTest, GpuTopWindowPresentationTest, GpuTransientPopupPresentationTest |
| Transient controls | GpuDropListPopupPresentationTest, GpuUiDropdownPopupPresentationTest, GpuUiMenuPopupPresentationTest, GpuUiTooltipPresentationTest |
| Text and control coverage | RenderFontWin32Test, RenderVulkanTextTest, RenderVulkanTextDirectWriteTest, UiTextLayoutTest, UiGpuDrawingTest, GpuUiCoverageTest |
| Sustained load | GpuResourceLoad, GpuSiblingLoad |
| Separate future-backend proof | RenderWgpuNativeTest |

The [macOS test script](../tools/test-macos.sh) selects the portable scene, camera, RHI and embedded presentation checks. Ui3DViewTest uses guarded Windows input and Ui3DMaterialTest uses a Windows native surface fixture.

Software/native reference cases intentionally exercise CPU/GDI behavior to compare
renderer output. Their presence does not select fallback in the required-Vulkan
demo. Tests of generated IME messages and doubled application units do not replace
real OS candidate selection or physical monitor-DPI review.

The gallery's `--qualify-ui` mode additionally covers the actual inspector colour
dialogs, browser, full/micro pickers, three workspace and compute cycles, theme
changes, resize and composition overlay/commit/cancel. Preserve exact executable
hashes and reports when comparing results.

The Image-page field-bounds regression uses public control minimum sizes and
captures both theme modes; it guards against clipped dropdowns, edits and toggles.
