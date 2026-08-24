#include "RenderPresentation.h"
#include "RenderPresentationBackend.h"

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

struct GpuContext::Impl {
	struct ContextEntry : Moveable<ContextEntry> {
		GpuBackendKind kind = GpuBackendKind::Unknown;
		One<GpuPresentationBackendContext> context;
	};

	GpuPresentationBackendContext *GetOrCreate(GpuBackendKind kind, String& error)
	{
		error.Clear();
		for(ContextEntry& entry : contexts)
			if(entry.kind == kind)
				return entry.context;

		GpuPresentationBackend *backend = FindGpuPresentationBackend(kind);
		if(!backend) {
			error = kind == GpuBackendKind::Unknown ? String("backend not selected")
			                                      : String("backend not supported");
			return nullptr;
		}

		One<GpuPresentationBackendContext> created = backend->CreateContext(error);
		if(!created) {
			if(error.IsEmpty())
				error = "GPU backend context creation failed";
			return nullptr;
		}

		ContextEntry& entry = contexts.Add();
		entry.kind = kind;
		entry.context = pick(created);
		return entry.context;
	}

	Vector<ContextEntry> contexts;
};

GpuContext::GpuContext()
{
	impl.Create();
}

GpuContext::~GpuContext() = default;

GpuContext& GpuContext::Default()
{
	static GpuContext context;
	return context;
}

struct GpuDisplayPresenter::Impl {
	One<GpuPresentationBackendSession> session;
	GpuContext *context = nullptr;
	GpuBackendKind backend = GpuBackendKind::Unknown;
	String error;
};

GpuDisplayPresenter::GpuDisplayPresenter()
{
	impl.Create();
}

GpuDisplayPresenter::~GpuDisplayPresenter()
{
	Close();
}

bool GpuDisplayPresenter::Open(GpuBackendKind backend, bool request_validation,
                               const GpuNativeWindowDesc& native_window,
                               String& out_error)
{
	return Open(GpuContext::Default(), backend, request_validation, native_window, out_error);
}

bool GpuDisplayPresenter::Open(GpuContext& context, GpuBackendKind backend,
                               bool request_validation,
                               const GpuNativeWindowDesc& native_window,
                               String& out_error)
{
	Close();
	out_error.Clear();
	impl->context = &context;
	impl->backend = backend;

	GpuPresentationBackend *provider = FindGpuPresentationBackend(backend);
	if(!provider) {
		impl->error = backend == GpuBackendKind::Unknown ? String("backend not selected")
		                                                  : String("backend not supported");
		out_error = impl->error;
		return false;
	}

	String context_error;
	GpuPresentationBackendContext *backend_context = context.impl
	                                               ? context.impl->GetOrCreate(backend, context_error)
	                                               : nullptr;
	if(!backend_context) {
		impl->error = context_error.IsEmpty() ? String("GPU backend context is unavailable") : context_error;
		out_error = impl->error;
		return false;
	}

	String create_error;
	impl->session = provider->CreateSession(*backend_context, create_error);
	if(!impl->session) {
		impl->error = create_error.IsEmpty() ? String("GPU backend session creation failed") : create_error;
		out_error = impl->error;
		return false;
	}

	if(!impl->session->Open(request_validation, native_window, out_error)) {
		impl->error = out_error.IsEmpty() ? impl->session->GetError() : out_error;
		impl->session.Clear();
		return false;
	}
	if(!impl->session->IsReady()) {
		impl->error = impl->session->GetError();
		if(impl->error.IsEmpty())
			impl->error = "GPU presentation backend did not become ready";
		out_error = impl->error;
		impl->session->Close();
		impl->session.Clear();
		return false;
	}
	impl->error.Clear();
	out_error.Clear();
	return true;
}

void GpuDisplayPresenter::Close()
{
	if(!impl)
		return;
	if(impl->session)
		impl->session->Close();
	impl->session.Clear();
	impl->context = nullptr;
	impl->backend = GpuBackendKind::Unknown;
	impl->error.Clear();
}

bool GpuDisplayPresenter::IsReady() const
{
	return impl && impl->session && impl->session->IsReady();
}

GpuBackendKind GpuDisplayPresenter::GetBackend() const
{
	return impl ? impl->backend : GpuBackendKind::Unknown;
}

String GpuDisplayPresenter::GetError() const
{
	if(!impl)
		return String();
	if(!impl->error.IsEmpty())
		return impl->error;
	return impl->session ? impl->session->GetError() : String();
}

bool GpuDisplayPresenter::Present(Size requested_size, const UiDisplayList& list,
                                  Rgba8 background, String& out_error)
{
	out_error.Clear();
	if(!IsReady()) {
		impl->error = GetError();
		if(impl->error.IsEmpty())
			impl->error = "GPU presenter is not ready";
		out_error = impl->error;
		return false;
	}
	if(!impl->session->Present(requested_size, list, background, out_error)) {
		impl->error = out_error.IsEmpty() ? impl->session->GetError() : out_error;
		out_error = impl->error;
		return false;
	}
	impl->error.Clear();
	out_error.Clear();
	return true;
}

}
