#include "RenderPresentation.h"

#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderRhi/RenderRhiBackend.h>
#include <memory>

namespace Upp {

namespace {

static GpuClearColor ToClearColor(Rgba8 color)
{
	const float scale = 1.0f / 255.0f;
	GpuClearColor out;
	out.red = color.r * scale;
	out.green = color.g * scale;
	out.blue = color.b * scale;
	out.alpha = color.a * scale;
	return out;
}

} // namespace

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
	std::unique_ptr<UiRenderer2D> renderer;
	GpuContext *context = nullptr;
	GpuBackendKind backend = GpuBackendKind::Unknown;
	GpuNativeWindowDesc native_window;
	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	Size swapchain_request_size = Size(0, 0);
	String error;

	GpuDevice *GetDevice() const
	{
		return session ? session->GetDevice() : nullptr;
	}

	void DestroyPresentationObjects()
	{
		renderer.reset();
		GpuDevice *device = GetDevice();
		if(device) {
			if(swapchain.IsValid())
				device->DestroySwapchain(swapchain);
			if(surface.IsValid())
				device->DestroySurface(surface);
		}
		swapchain = GpuSwapchainId();
		surface = GpuSurfaceId();
		swapchain_request_size = Size(0, 0);
		native_window = GpuNativeWindowDesc();
	}

	bool EnsureSwapchain(Size requested_size, String& out_error)
	{
		GpuDevice *device = GetDevice();
		if(!device) {
			error = "GPU presentation backend exposes no device";
			out_error = error;
			return false;
		}

		if(!surface.IsValid()) {
			GpuSurfaceDesc desc;
			desc.label = "GPU presentation surface";
			desc.size = requested_size;
			desc.native_window = native_window;
			GpuResult result = device->CreateSurface(desc, surface);
			if(result != GpuResult::Ok) {
				error = device->GetLastError();
				if(error.IsEmpty())
					error = "logical GPU surface creation failed";
				out_error = error;
				return false;
			}
		}

		if(!swapchain.IsValid()) {
			GpuSwapchainDesc desc;
			desc.label = "GPU presentation swapchain";
			desc.surface = surface;
			desc.size = requested_size;
			desc.color_format = GpuFormat::RGBA8;
			desc.image_count = 2;
			GpuResult result = device->CreateSwapchain(desc, swapchain);
			if(result != GpuResult::Ok) {
				error = device->GetLastError();
				if(error.IsEmpty())
					error = "logical GPU swapchain creation failed";
				out_error = error;
				return false;
			}
			swapchain_request_size = requested_size;
		}
		else if(requested_size != swapchain_request_size) {
			GpuResult result = device->ResizeSwapchain(swapchain, requested_size);
			if(result != GpuResult::Ok) {
				error = device->GetLastError();
				if(error.IsEmpty())
					error = "logical GPU swapchain resize failed";
				out_error = error;
				return false;
			}
			swapchain_request_size = requested_size;
		}
		return true;
	}

	GpuResult PresentOnce(const UiDisplayList& list, Rgba8 background, String& out_error)
	{
		GpuDevice *device = GetDevice();
		if(!device || !renderer) {
			error = "GPU presentation session is not ready";
			out_error = error;
			return GpuResult::InvalidState;
		}

		GpuFrameInfo frame;
		GpuResult result = device->BeginFrame(swapchain, frame);
		if(result != GpuResult::Ok) {
			error = device->GetLastError();
			if(error.IsEmpty())
				error = "GPU BeginFrame failed";
			out_error = error;
			return result;
		}

		if(!renderer->RenderFrame(list, frame, ToClearColor(background))) {
			String render_error = renderer->GetError();
			device->Present(frame.frame); // Release the acquired frame on the failure path.
			error = render_error.IsEmpty() ? String("UiRenderer2D frame render failed") : render_error;
			out_error = error;
			return GpuResult::InvalidState;
		}

		result = device->Present(frame.frame);
		if(result != GpuResult::Ok) {
			error = device->GetLastError();
			if(error.IsEmpty())
				error = "GPU Present failed";
			out_error = error;
			return result;
		}

		error.Clear();
		out_error.Clear();
		return GpuResult::Ok;
	}
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
	impl->native_window = native_window;

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

	GpuDevice *device = impl->GetDevice();
	if(!device || device->GetBackendKind() != backend) {
		impl->error = "GPU presentation backend returned an incompatible device";
		out_error = impl->error;
		impl->session->Close();
		impl->session.Clear();
		return false;
	}

	impl->renderer.reset(new UiRenderer2D(*device));
	if(!impl->renderer->IsReady()) {
		String failure = impl->renderer->GetError();
		impl->renderer.reset();
		impl->session->Close();
		impl->session.Clear();
		impl->error = failure.IsEmpty() ? String("UiRenderer2D initialization failed") : failure;
		out_error = impl->error;
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
	impl->DestroyPresentationObjects();
	if(impl->session)
		impl->session->Close();
	impl->session.Clear();
	impl->context = nullptr;
	impl->backend = GpuBackendKind::Unknown;
	impl->error.Clear();
}

bool GpuDisplayPresenter::IsReady() const
{
	return impl && impl->session && impl->session->IsReady() && impl->GetDevice() &&
	       impl->renderer && impl->renderer->IsReady();
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
	if(requested_size.cx <= 0 || requested_size.cy <= 0)
		return true;
	if(!IsReady()) {
		impl->error = GetError();
		if(impl->error.IsEmpty())
			impl->error = "GPU presenter is not ready";
		out_error = impl->error;
		return false;
	}
	if(!impl->EnsureSwapchain(requested_size, out_error))
		return false;

	GpuResult result = impl->PresentOnce(list, background, out_error);
	if(result == GpuResult::Ok)
		return true;
	if(result != GpuResult::OutOfDate)
		return false;

	GpuDevice *device = impl->GetDevice();
	if(!device || device->ResizeSwapchain(impl->swapchain, requested_size) != GpuResult::Ok) {
		impl->error = device ? device->GetLastError() : String();
		if(impl->error.IsEmpty())
			impl->error = "GPU swapchain recreation after out-of-date presentation failed";
		out_error = impl->error;
		return false;
	}
	impl->swapchain_request_size = requested_size;
	return impl->PresentOnce(list, background, out_error) == GpuResult::Ok;
}

}
