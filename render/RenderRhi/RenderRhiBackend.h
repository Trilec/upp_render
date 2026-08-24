#pragma once

#include <RenderRhi/RenderRhi.h>

namespace Upp {

// Internal provider seam between application presentation and a compiled GPU backend.
// It contains no backend-native types and deliberately stops at the neutral GpuDevice.
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
	virtual GpuDevice *GetDevice() const = 0;
};

class GpuPresentationBackend {
public:
	virtual ~GpuPresentationBackend() {}

	virtual One<GpuPresentationBackendContext> CreateContext(String& error) = 0;
	virtual One<GpuPresentationBackendSession> CreateSession(GpuPresentationBackendContext& context,
	                                                          String& error) = 0;
};

// Compiled backend packages register during process start. One provider owns each
// backend kind; duplicate registration and Unknown are rejected.
bool RegisterGpuPresentationBackend(GpuBackendKind kind, GpuPresentationBackend& backend);
GpuPresentationBackend *FindGpuPresentationBackend(GpuBackendKind kind);
bool IsGpuPresentationBackendRegistered(GpuBackendKind kind);

}
