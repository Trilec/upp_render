# GPU-rendered U++ interface

## Principle

A GPU-rendered U++ application is still a U++ application.

U++ owns:

- control tree;
- layout;
- input and focus;
- state and event dispatch;
- themes/style resolution;
- text/font choices.

The renderer owns replay and presentation of the resolved drawing intent.

## Root path

```text
GpuTopWindow
  -> U++ DrawCtrl / resolved control painting
  -> RenderCtrlBridge
  -> UiDisplayList
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

## Popup/dialog direction

Each top-level/native popup or dialog that genuinely needs GPU composition should have a presentation surface but should acquire compatible expensive device/resource state from the same `GpuContext`. The renderer must not imply one Vulkan device per dialog.

## Unsupported Draw operations

The control bridge explicitly reports unsupported operations. They must either gain a neutral semantic mapping or deliberately fall back; silently dropping them is not acceptable.

## upp_Ui

`upp_Ui` theme/model/control state is consumed after resolution. GPU rendering must not create a second theme database or second layout system.
