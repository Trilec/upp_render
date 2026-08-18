#pragma once

#include <RenderCanvas/RenderCanvas.h>
#include <RenderRhi/RenderRhi.h>

namespace Upp {

// Backend-neutral presentation owner for one native window surface.
//
// The public contract deliberately contains no Vulkan types. It owns the
// selected backend session, the logical surface/swapchain and UiRenderer2D
// lifetime, while the UI host remains responsible for native-window lifetime
// and for deciding when a frame should be presented.
class GpuDisplayPresenter {
public:
	GpuDisplayPresenter();
	~GpuDisplayPresenter();

	GpuDisplayPresenter(const GpuDisplayPresenter&) = delete;
	GpuDisplayPresenter& operator=(const GpuDisplayPresenter&) = delete;

	bool Open(GpuBackendKind backend, bool request_validation,
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
