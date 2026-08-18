#include "RenderPresentation.h"

#include <RenderGpu2D/RenderGpu2D.h>
#include <RenderVulkan/RenderVulkanRhi.h>

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

class BackendSession {
public:
	virtual ~BackendSession() {}

	virtual bool Open(bool request_validation, const GpuNativeWindowDesc& native_window,
	                  String& error) = 0;
	virtual void Close() = 0;
	virtual bool IsReady() const = 0;
	virtual String GetError() const = 0;
	virtual bool Present(Size requested_size, const UiDisplayList& list,
	                     Rgba8 background, String& error) = 0;
};

class VulkanBackendSession : public BackendSession {
public:
	~VulkanBackendSession() override
	{
		Close();
	}

	bool Open(bool request_validation, const GpuNativeWindowDesc& window,
	          String& out_error) override
	{
		Close();
		error.Clear();
		native_window = window;
		if(!session.Open(request_validation, native_window)) {
			error = session.GetError();
			out_error = error;
			return false;
		}

		device.reset(new VulkanGpuDevice(session));
		if(!device->IsReady()) {
			String failure = device->GetError();
			Close();
			error = failure.IsEmpty() ? String("VulkanGpuDevice initialization failed") : failure;
			out_error = error;
			return false;
		}

		renderer.reset(new UiRenderer2D(*device));
		if(!renderer->IsReady()) {
			String failure = renderer->GetError();
			Close();
			error = failure.IsEmpty() ? String("UiRenderer2D initialization failed") : failure;
			out_error = error;
			return false;
		}

		out_error.Clear();
		return true;
	}

	void Close() override
	{
		// Reverse the borrowing order: renderer -> logical swapchain/surface ->
		// adapter -> VulkanSurfaceSession. The adapter never owns session objects.
		renderer.reset();
		if(device) {
			if(swapchain.IsValid())
				device->DestroySwapchain(swapchain);
			swapchain = GpuSwapchainId();
			if(surface.IsValid())
				device->DestroySurface(surface);
			surface = GpuSurfaceId();
		}
		device.reset();
		session.Close();
		native_window = GpuNativeWindowDesc();
		swapchain_request_size = Size(0, 0);
		error.Clear();
	}

	bool IsReady() const override
	{
		return session.IsReady() && device && device->IsReady() &&
		       renderer && renderer->IsReady();
	}

	String GetError() const override
	{
		return error;
	}

	bool Present(Size requested_size, const UiDisplayList& list,
	             Rgba8 background, String& out_error) override
	{
		out_error.Clear();
		if(requested_size.cx <= 0 || requested_size.cy <= 0)
			return true;
		if(!IsReady()) {
			error = !device ? session.GetError() : device->GetError();
			if(error.IsEmpty())
				error = "GPU presentation session is not ready";
			out_error = error;
			return false;
		}
		if(!EnsureSwapchain(requested_size, out_error))
			return false;
		if(PresentOnce(list, background, out_error))
			return true;

		if(!session.GetFrameReport().out_of_date)
			return false;
		if(device->ResizeSwapchain(swapchain, requested_size) != GpuResult::Ok) {
			error = device->GetError();
			if(error.IsEmpty())
				error = "GPU swapchain recreation after out-of-date presentation failed";
			out_error = error;
			return false;
		}
		swapchain_request_size = requested_size;
		if(PresentOnce(list, background, out_error)) {
			out_error.Clear();
			return true;
		}
		return false;
	}

private:
	bool EnsureSwapchain(Size requested_size, String& out_error)
	{
		if(!surface.IsValid()) {
			GpuSurfaceDesc desc;
			desc.label = "GPU presentation surface";
			desc.size = requested_size;
			desc.native_window = native_window;
			GpuResult result = device->CreateSurface(desc, surface);
			if(result != GpuResult::Ok) {
				error = device->GetError();
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
				error = device->GetError();
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
				error = device->GetError();
				if(error.IsEmpty())
					error = "logical GPU swapchain resize failed";
				out_error = error;
				return false;
			}
			swapchain_request_size = requested_size;
		}
		return true;
	}

	bool PresentOnce(const UiDisplayList& list, Rgba8 background, String& out_error)
	{
		GpuFrameInfo frame;
		GpuResult result = device->BeginFrame(swapchain, frame);
		if(result != GpuResult::Ok) {
			error = device->GetError();
			if(error.IsEmpty())
				error = "GPU BeginFrame failed";
			out_error = error;
			return false;
		}

		if(!renderer->RenderFrame(list, frame, ToClearColor(background))) {
			String render_error = renderer->GetError();
			// Release an acquired frame even when renderer command setup fails.
			device->Present(frame.frame);
			error = render_error.IsEmpty() ? String("UiRenderer2D frame render failed") : render_error;
			out_error = error;
			return false;
		}

		result = device->Present(frame.frame);
		if(result != GpuResult::Ok) {
			error = device->GetError();
			if(error.IsEmpty())
				error = "GPU Present failed";
			out_error = error;
			return false;
		}

		error.Clear();
		out_error.Clear();
		return true;
	}

	VulkanSurfaceSession session;
	std::unique_ptr<VulkanGpuDevice> device;
	std::unique_ptr<UiRenderer2D> renderer;
	GpuNativeWindowDesc native_window;
	GpuSurfaceId surface;
	GpuSwapchainId swapchain;
	Size swapchain_request_size = Size(0, 0);
	String error;
};

static One<BackendSession> CreateBackendSession(GpuBackendKind kind)
{
	if(kind == GpuBackendKind::Vulkan)
		return new VulkanBackendSession;
	return One<BackendSession>();
}

} // namespace

struct GpuDisplayPresenter::Impl {
	One<BackendSession> session;
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
	Close();
	out_error.Clear();
	impl->backend = backend;
	impl->session = CreateBackendSession(backend);
	if(!impl->session) {
		impl->error = backend == GpuBackendKind::Unknown ?
		              String("backend not selected") : String("backend not supported");
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
