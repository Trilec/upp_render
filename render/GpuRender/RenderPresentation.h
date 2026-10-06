#pragma once

#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>
#include <RenderGpu2D/RenderGpu2D.h>

namespace Upp {

struct GpuPresentationStats {
	uint64 presented_frames = 0;
	uint64 dropped_frames = 0;
	int pending_frames = 0;
	// Root control recording/worker enqueue CPU timings; zero for other presenters.
	// Maxima include warmup and span the owning window lifetime.
	double record_ms = 0, record_max_ms = 0;
	double enqueue_ms = 0, enqueue_max_ms = 0;
	double acquire_ms = 0;
	double replay_ms = 0;
	double present_ms = 0;
	UiRenderer2DStats renderer;
};

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

	GpuPresentationStats GetStats() const;
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
