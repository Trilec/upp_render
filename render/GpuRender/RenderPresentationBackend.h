#pragma once

#include "RenderPresentation.h"

namespace Upp {

// Internal backend plug-in seam for GpuRender presentation.
// Application-facing GpuCtrl/GpuWindow/GpuTopWindow code never depends on this
// interface and never sees backend-native types.
class GpuPresentationBackendContext {
public:
	virtual ~GpuPresentationBackendContext() {}
};

class GpuPresentationBackendSession {
public:
	virtual ~GpuPresentationBackendSession() {}

	virtual bool Open(bool request_validation, const GpuNativeWindowDesc& native_window,
	                  String& error) = 0;
	virtual void Close() = 0;
	virtual bool IsReady() const = 0;
	virtual String GetError() const = 0;
	virtual bool Present(Size requested_size, const UiDisplayList& list,
	                     Rgba8 background, String& error) = 0;
};

class GpuPresentationBackend {
public:
	virtual ~GpuPresentationBackend() {}

	virtual One<GpuPresentationBackendContext> CreateContext(String& error) = 0;
	virtual One<GpuPresentationBackendSession> CreateSession(GpuPresentationBackendContext& context,
	                                                          String& error) = 0;
};

// Registration is intended for compiled backend packages during process start.
// One provider owns each backend kind; duplicate registration is rejected.
bool RegisterGpuPresentationBackend(GpuBackendKind kind, GpuPresentationBackend& backend);
GpuPresentationBackend *FindGpuPresentationBackend(GpuBackendKind kind);
bool IsGpuPresentationBackendRegistered(GpuBackendKind kind);

}
