#include "RenderPresentationBackend.h"

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

class VulkanPresentationContext : public GpuPresentationBackendContext {
public:
	VulkanSurfaceSessionGroup group;
};

class VulkanPresentationSession : public GpuPresentationBackendSession {
public:
	explicit VulkanPresentationSession(VulkanPresentationContext& context)
		: session(context.group)
	{
	}

	~VulkanPresentationSession() override
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

class VulkanPresentationBackend : public GpuPresentationBackend {
public:
	One<GpuPresentationBackendContext> CreateContext(String& error) override
	{
		error.Clear();
		return new VulkanPresentationContext;
	}

	One<GpuPresentationBackendSession> CreateSession(GpuPresentationBackendContext& context,
	                                                  String& error) override
	{
		error.Clear();
		return new VulkanPresentationSession(static_cast<VulkanPresentationContext&>(context));
	}
};

static VulkanPresentationBackend s_vulkan_backend;
static bool s_vulkan_backend_registered =
	RegisterGpuPresentationBackend(GpuBackendKind::Vulkan, s_vulkan_backend);

} // namespace

}
