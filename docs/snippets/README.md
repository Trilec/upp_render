# Public-API source snippets

These small, copyable entry points replace three duplicate demo projects:

- [embedded.cpp](embedded.cpp): GpuCtrl inside an ordinary native window.
- [gpu-window.cpp](gpu-window.cpp): a custom GpuWindow.
- [gpu-ui.cpp](gpu-ui.cpp): a root-composited GpuTopWindow.

They deliberately show different public integration points. To compile one,
create a small application package with a GpuRender dependency and the chosen
source file. For the maintained complete UI use GpuUiGallery instead.
