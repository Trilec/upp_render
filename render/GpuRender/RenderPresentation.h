#pragma once

#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

// Backend-neutral application GPU context.
//
// Ordinary presenters use Default(), so multiple GpuCtrl/GpuWindow/GpuTopWindow
// surfaces can share compatible expensive backend state while keeping their own
// surface/swapchain lifecycle. Advanced applications may create another context
// deliberately (for example a separate adapter/policy domain). A context must
// outlive presenters explicitly opened against it.
class GpuContext {
public:
	GpuContext();
	~GpuContext();

	GpuContext(const GpuContext&) = delete;
	GpuContext& operator=(const GpuContext&) = delete;

	static GpuContext& Default();

private:
	struct Impl;
	One<Impl> impl;

	friend class GpuDisplayPresenter;
};

// Backend-neutral presentation owner for one native window surface.
//
// The public contract deliberately contains no Vulkan/Metal/WebGPU types. It
// owns one logical surface/swapchain and UiRenderer2D lifetime while GpuContext
// supplies compatible application-level backend ownership.
class GpuDisplayPresenter {
public:
	GpuDisplayPresenter();
	~GpuDisplayPresenter();

	GpuDisplayPresenter(const GpuDisplayPresenter&) = delete;
	GpuDisplayPresenter& operator=(const GpuDisplayPresenter&) = delete;

	// Ordinary path: use the application default context.
	bool Open(GpuBackendKind backend, bool request_validation,
	          const GpuNativeWindowDesc& native_window, String& error);

	// Advanced path: explicitly choose a context. The context must outlive this
	// presenter and any surface opened through it.
	bool Open(GpuContext& context, GpuBackendKind backend, bool request_validation,
	          const GpuNativeWindowDesc& native_window, String& error);
	void Close();

	bool IsReady() const;
	GpuBackendKind GetBackend() const;
	String GetError() const;

	bool Present(Size requested_size, const UiDisplayList& list,
	             Rgba8 background, String& error);

private:
	struct Impl;
	One<Impl> impl;
};

}
