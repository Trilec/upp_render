# GPU-rendered U++ interface

## Principle

A GPU-rendered U++ application is still a U++ application.

U++ / `upp_Ui` own:

- control tree;
- layout;
- input and focus;
- state and event dispatch;
- themes/style resolution;
- text/font choices;
- invalidation/damage semantics.

The renderer owns recording/replay, reusable GPU resources and presentation of the resolved drawing intent.

## Root path

```text
GpuTopWindow
  -> U++ DrawCtrl / resolved control painting
  -> RenderCtrlBridge
  -> UiCanvas / UiDisplayList
  -> UiRenderer2D
  -> RenderRhi
  -> backend
  -> one top-level surface/swapchain
```

Do not create a GPU-native child surface for every button, edit field or label.

## Embedded exception

`GpuCtrl` intentionally does create an embedded native accelerated surface. It is for content that is naturally a separate GPU viewport such as image/video/VFX previews, graphs, editors, 2D/3D scenes and similar views. Normal U++ controls can surround it.

## Software parity

The same neutral display list can be replayed through the software reference renderer. This is the correctness/fallback model and protects the project from inventing backend-specific UI semantics.

Shared semantics do not require identical optimizations. Software may use direct Draw or cached AA images while GPU replay may use shared immutable geometry, instancing, textures, glyph atlases and batching.

## Damage / partial invalidation

Whole-window GPU composition must not erase local invalidation information from the UI contract. The integration path should preserve bounded dirty regions from U++ controls even if a backend chooses broader replay when that is cheaper.

A slider thumb, caret blink or one button-state change is semantically local damage; Graph pan can legitimately damage most of a large viewport.

## Popup/dialog direction

Each top-level/native popup or dialog that genuinely needs GPU composition should have a presentation surface but should acquire compatible expensive device/resource state from the same `GpuContext`. The renderer must not imply one Vulkan device per dialog.

Ordinary child controls remain part of the root scene; transient native popup/menu/tooltip windows are the natural exception when U++ itself creates a real top-level/native window.

## Unsupported Draw operations

The control bridge explicitly reports unsupported operations. They must either gain a neutral semantic mapping or deliberately fall back; silently dropping them is not acceptable.

## upp_Ui dependency direction

`upp_Ui` must continue to work independently through normal U++ Draw / UiDraw and must not gain a hard dependency on `upp_render` or `RenderVulkan`.

The future opt-in GPU path should be assembled from the render/integration side:

```text
upp_Ui control tree
  -> resolved presentation + damage
  -> GpuRender integration / bridge
  -> UiCanvas
  -> immutable UiDisplayList
  -> RenderGpu2D / RenderRhi
  -> backend
```

Do not create a second `UiRenderContext`, theme database, layout authority or model system inside either project.

`UiCanvas` should evolve as the backend-neutral drawing vocabulary. Keep its display list semantic enough to preserve optimization opportunities such as image/text/path identity, batching and resource reuse rather than turning it into a Vulkan command-buffer imitation.

Important future parity items include source-rect image drawing, image opacity/tint, 9-slice helpers, non-rectangular clipping, clear logical-vs-device coordinate semantics, and deliberate group-opacity/shadow policy.

Full convergence constraints are in `docs/UPP_UI_RENDER_CONVERGENCE.md`.
