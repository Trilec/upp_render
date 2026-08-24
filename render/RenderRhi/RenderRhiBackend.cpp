#include "RenderRhiBackend.h"

namespace Upp {

namespace {

struct BackendRegistration : Moveable<BackendRegistration> {
	GpuBackendKind kind = GpuBackendKind::Unknown;
	GpuPresentationBackend *backend = nullptr;
};

static Vector<BackendRegistration>& BackendRegistry()
{
	static Vector<BackendRegistration> registry;
	return registry;
}

static int FindBackendIndex(GpuBackendKind kind)
{
	const Vector<BackendRegistration>& registry = BackendRegistry();
	for(int i = 0; i < registry.GetCount(); ++i)
		if(registry[i].kind == kind)
			return i;
	return -1;
}

} // namespace

bool RegisterGpuPresentationBackend(GpuBackendKind kind, GpuPresentationBackend& backend)
{
	if(kind == GpuBackendKind::Unknown || FindBackendIndex(kind) >= 0)
		return false;
	BackendRegistration& registration = BackendRegistry().Add();
	registration.kind = kind;
	registration.backend = &backend;
	return true;
}

GpuPresentationBackend *FindGpuPresentationBackend(GpuBackendKind kind)
{
	int index = FindBackendIndex(kind);
	return index >= 0 ? BackendRegistry()[index].backend : nullptr;
}

bool IsGpuPresentationBackendRegistered(GpuBackendKind kind)
{
	return FindGpuPresentationBackend(kind) != nullptr;
}

}
