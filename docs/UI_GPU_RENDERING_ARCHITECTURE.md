# UI GPU Rendering Architecture

## Two Separate Cases

### Embedded GPU Content

`GpuCtrl` is the right boundary for embedded accelerated views such as:

- image viewers
- video viewers
- 2D and 3D canvases
- VFX previews
- graphs and specialized visual tools

The control owns its native child host and a backend session. The ordinary U++ layout owns everything around it.

### Entire GPU-Rendered Interface

Do not model the whole UI as one native child window per button.

The long-term shape should be a root compositor or `GpuTopWindow`-style path where:

- U++ still owns layout, input, focus and control state
- existing themes and upp_Ui style resolution remain authoritative
- controls emit neutral drawing commands
- commands are recorded into a display list
- software replay remains the correctness reference
- GPU replay batches and renders the same command stream
- text, clipping and images arrive in controlled stages

## Architecture Sketch

```text
TopWindow / GpuTopWindow
  -> U++ control tree and theme resolution
  -> neutral display list / drawing commands
  -> software replay or GPU replay
  -> RenderRhi
  -> backend implementation
```

## Strategy Comparison

### Direct `GpuPainter` Calls

Pros:

- simple to call from a control
- easy to understand for one-off views

Cons:

- tends to leak backend thinking into UI code
- harder to preserve software reference parity

### Draw-Compatible Recording Adapter

Pros:

- preserves the existing U++/upp_Ui drawing model
- keeps software replay as the reference path
- matches the current neutral architecture well

Cons:

- more work up front
- requires careful command coverage

### Converting Existing Draw Operations Into `RenderCanvas`

Pros:

- good bridge from current code
- can reuse existing control drawing knowledge

Cons:

- may inherit too much legacy shape from `Draw`

### Hybrid Path

Pros:

- practical for staged adoption
- allows new controls and converted legacy controls to coexist

Cons:

- needs discipline so the API does not become a drawer of half-measures

## Recommendation

Use a hybrid path with a neutral recording adapter as the center.

That keeps software replay as the truth source, lets existing controls migrate gradually, and avoids turning the UI layer into a backend-specific API parade.

## Native-Child Airspace

`GpuCtrl` is appropriate for embedded content, but it is not a general answer for overlapping UI.

Native child windows have airspace and compositing limits, so they should not be used to fake a whole control tree.

For the whole interface, the future renderer must draw into a composited surface owned by the UI framework, not into a stack of child HWNDs wearing optimism.

## Theme Data

The existing upp_Ui theme data should be consumed, not replaced.

The GPU layer should receive resolved colors, metrics, glyph choices and control geometry from the existing theme machinery so it does not grow a second theme system with a better haircut and worse compatibility.
