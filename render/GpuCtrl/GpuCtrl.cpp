#include "GpuCtrl.h"
#include "GpuCtrlTestHooks.h"
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
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

static bool BuildDefaultDisplayList(Size size, UiDisplayList& list, Rgba8& background, String& error)
{
	background = Rgba8(20, 61, 148, 255);
	error.Clear();
	UiDisplayListBuilder builder;
	if(size.cx <= 0 || size.cy <= 0) {
		if(!builder.Finish(list)) {
			error = builder.GetError();
			return false;
		}
		return true;
	}

	const double w = size.cx;
	const double h = size.cy;
	const double unit = max(1.0, min(w, h));

	builder.FillRect(Rectf(0.08 * w, 0.10 * h, 0.46 * w, 0.48 * h),
	                 Rgba8(230, 82, 20, 255));
	builder.StrokeRect(Rectf(0.12 * w, 0.16 * h, 0.62 * w, 0.70 * h),
	                   max(1.0, unit * 0.025), Rgba8(40, 205, 118, 220));
	builder.Save();
	builder.ClipRect(Rectf(0.24 * w, 0.18 * h, 0.90 * w, 0.88 * h));
	Transform2D affine;
	affine.x.x = 0.96;
	affine.x.y = 0.16;
	affine.y.x = -0.10;
	affine.y.y = 0.92;
	affine.t = Pointf(0.08 * w, 0.03 * h);
	builder.ConcatTransform(affine);
	builder.FillRoundedRect(RoundedRect(Rectf(0.30 * w, 0.24 * h, 0.76 * w, 0.72 * h),
	                                      max(1.0, unit * 0.08)),
	                        Rgba8(55, 118, 238, 188));
	builder.Restore();
	builder.FillRect(Rectf(0.58 * w, 0.56 * h, 0.94 * w, 0.92 * h),
	                 Rgba8(246, 210, 54, 128));

	if(!builder.Finish(list)) {
		error = builder.GetError();
		return false;
	}
	return true;
}

class GpuCtrlBackendSession {
public:
	virtual ~GpuCtrlBackendSession() {}

	virtual bool Open(bool request_validation, const GpuNativeWindowDesc& native_window, String& error) = 0;
	virtual void Close() = 0;
	virtual bool IsReady() const = 0;
	virtual const String& GetError() const = 0;
	virtual bool Present(Size requested_size, const UiDisplayList& list, Rgba8 background, String& error) = 0;
};

class VulkanGpuCtrlBackendSession : public GpuCtrlBackendSession {
public:
	~VulkanGpuCtrlBackendSession() override
	{
		Close();
	}

	bool Open(bool request_validation, const GpuNativeWindowDesc& window, String& out_error) override
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
		// adapter -> session. The adapter never owns the Vulkan session objects.
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
	}

	bool IsReady() const override
	{
		return session.IsReady() && device && device->IsReady() && renderer && renderer->IsReady();
	}

	const String& GetError() const override
	{
		return error;
	}

	bool Present(Size requested_size, const UiDisplayList& list, Rgba8 background, String& out_error) override
	{
		out_error.Clear();
		if(requested_size.cx <= 0 || requested_size.cy <= 0)
			return true;
		if(!IsReady()) {
			error = !device ? session.GetError() : device->GetError();
			if(error.IsEmpty())
				error = "GpuCtrl Vulkan renderer is not ready";
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
				error = "Vulkan swapchain recreation after out-of-date presentation failed";
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
			desc.label = "GpuCtrl surface";
			desc.size = requested_size;
			desc.native_window = native_window;
			GpuResult result = device->CreateSurface(desc, surface);
			if(result != GpuResult::Ok) {
				error = device->GetError();
				if(error.IsEmpty()) error = "GpuCtrl logical surface creation failed";
				out_error = error;
				return false;
			}
		}
		if(!swapchain.IsValid()) {
			GpuSwapchainDesc desc;
			desc.label = "GpuCtrl swapchain";
			desc.surface = surface;
			desc.size = requested_size;
			desc.color_format = GpuFormat::RGBA8;
			desc.image_count = 2;
			GpuResult result = device->CreateSwapchain(desc, swapchain);
			if(result != GpuResult::Ok) {
				error = device->GetError();
				if(error.IsEmpty()) error = "GpuCtrl logical swapchain creation failed";
				out_error = error;
				return false;
			}
			swapchain_request_size = requested_size;
		}
		else if(requested_size != swapchain_request_size) {
			GpuResult result = device->ResizeSwapchain(swapchain, requested_size);
			if(result != GpuResult::Ok) {
				error = device->GetError();
				if(error.IsEmpty()) error = "GpuCtrl logical swapchain resize failed";
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
			if(error.IsEmpty()) error = "GpuCtrl BeginFrame failed";
			out_error = error;
			return false;
		}

		if(!renderer->RenderFrame(list, frame, ToClearColor(background))) {
			String render_error = renderer->GetError();
			// Release the acquired frame even when rendering fails. RenderFrame's
			// own failure cleanup consumes any command list it successfully began.
			device->Present(frame.frame);
			error = render_error.IsEmpty() ? String("UiRenderer2D frame render failed") : render_error;
			out_error = error;
			return false;
		}

		result = device->Present(frame.frame);
		if(result != GpuResult::Ok) {
			error = device->GetError();
			if(error.IsEmpty()) error = "GpuCtrl Present failed";
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

static One<GpuCtrlBackendSession> CreateBackendSession(GpuBackendKind kind)
{
	if(kind == GpuBackendKind::Vulkan)
		return new VulkanGpuCtrlBackendSession;
	return One<GpuCtrlBackendSession>();
}

} // namespace

namespace GpuCtrlTestHooks {

bool BuildDefaultFrame(Size size, UiDisplayList& list, Rgba8& background, String& error)
{
	return BuildDefaultDisplayList(size, list, background, error);
}

} // namespace GpuCtrlTestHooks

// The public control stays tiny; platform/session ownership lives behind this
// private implementation so future backends do not leak into the ordinary API.
struct GpuCtrl::Impl {
	class Host : public DHCtrl {
	public:
		Host()
		{
			NoWantFocus();
		}

		void Attach(Impl *_impl)
		{
			impl = _impl;
		}

		void State(int reason) override
		{
			// CLOSE must reach the implementation before DHCtrl tears the native host down.
			if(reason == CLOSE) {
				if(impl)
					impl->OnHostState(reason);
				DHCtrl::State(reason);
				return;
			}
			DHCtrl::State(reason);
			if(impl)
				impl->OnHostState(reason);
		}

		LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
		{
			if(message == WM_ERASEBKGND)
				return 1;
			if(message == WM_PAINT) {
				PAINTSTRUCT ps;
				HWND hwnd = GetHWND();
				HDC dc = BeginPaint(hwnd, &ps);
				bool gpu_painted = impl && impl->OnHostPaint();
				if(!gpu_painted)
					FillRect(dc, &ps.rcPaint, GetSysColorBrush(COLOR_WINDOW));
				EndPaint(hwnd, &ps);
				return 0;
			}
			return DHCtrl::WindowProc(message, wParam, lParam);
		}

	private:
		Impl *impl = nullptr;
	};

	Impl(GpuCtrl& _owner)
		: owner(&_owner)
	{
		host.Attach(this);
	}

	~Impl()
	{
		destroying = true;
		StopGpuSession();
	}

	bool IsNativeHostReady() const
	{
		HWND hwnd = const_cast<Host&>(host).GetHWND();
		if(!hwnd || !IsWindow(hwnd))
			return false;
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		return pid == GetCurrentProcessId();
	}

	bool IsGpuReady() const
	{
		return gpu_ready && backend && backend->IsReady();
	}

	String GetGpuError() const
	{
		if(!api_error.IsEmpty())
			return api_error;
		if(!session_error.IsEmpty())
			return session_error;
		return presentation_error;
	}

	Size GetNativeHostSize() const
	{
		HWND hwnd = const_cast<Host&>(host).GetHWND();
		RECT rect{};
		if(!hwnd || !GetClientRect(hwnd, &rect))
			return Size(0, 0);
		LONG width = rect.right - rect.left;
		LONG height = rect.bottom - rect.top;
		return Size(width > 0 ? (int)width : 0, height > 0 ? (int)height : 0);
	}

	bool OnHostPaint()
	{
		if(!IsGpuReady() || !backend)
			return false;
		Size requested_size = GetNativeHostSize();
		UiDisplayList list;
		Rgba8 background;
		String error;
		if(!BuildDefaultDisplayList(requested_size, list, background, error)) {
			presentation_error = error;
			return false;
		}
		if(backend->Present(requested_size, list, background, error)) {
			presentation_error.Clear();
			return true;
		}
		presentation_error = error.IsEmpty() ? backend->GetError() : error;
		return false;
	}

	void RequestGpuRefresh()
	{
		if(IsNativeHostReady())
			host.Refresh();
	}

	void RetryGpuInit()
	{
		if(destroying)
			return;
		init_attempted = false;
		ClearError();
		if(host_ready && !gpu_ready)
			StartGpuSession();
	}

	void SetBackend(GpuBackendKind kind)
	{
		if(kind == backend_kind)
			return;
		if(IsGpuReady() && kind != backend_kind) {
			SetApiError("backend change while open is not supported");
			return;
		}
		backend_kind = kind;
		init_attempted = false;
		if(kind == GpuBackendKind::Vulkan)
			ClearError();
		else if(kind != GpuBackendKind::Unknown)
			SetApiError("backend not supported");
		else
			SetApiError("backend not selected");
		if(host_ready && !gpu_ready && kind == GpuBackendKind::Vulkan)
			StartGpuSession();
	}

	void SetValidation(bool validation)
	{
		if(validation_requested == validation)
			return;
		if(IsGpuReady()) {
			SetApiError("validation change while open is not supported");
			return;
		}
		validation_requested = validation;
		init_attempted = false;
		ClearError();
		if(host_ready && !gpu_ready)
			StartGpuSession();
	}

	void OnHostState(int reason)
	{
		if(destroying)
			return;
		switch(reason) {
		case OPEN:
		case SHOW:
		case ENABLE:
		case POSITION:
		case LAYOUTPOS:
			host_ready = IsNativeHostReady();
			if(host_ready && !gpu_ready && !init_attempted) {
				ClearError();
				StartGpuSession();
			}
			SyncHostBounds();
			if(reason == SHOW && IsGpuReady()) {
				presentation_error.Clear();
				host.Refresh();
			}
			break;
		case CLOSE:
			StopGpuSession();
			host_ready = false;
			break;
		}
	}

	void OnCtrlState(int reason)
	{
		switch(reason) {
		case CLOSE:
			StopGpuSession();
			host_ready = false;
			break;
		case SHOW:
		case ENABLE:
		case POSITION:
		case LAYOUTPOS:
			SyncHostBounds();
			break;
		}
	}

	void SyncHostBounds()
	{
		Size sz = owner->GetSize();
		bool size_changed = sz != last_host_size;
		if(size_changed)
			presentation_error.Clear();
		last_host_size = sz;
		host.SetRect(0, 0, sz.cx, sz.cy);
		if(IsNativeHostReady()) {
			HWND hwnd = host.GetHWND();
			EnableWindow(hwnd, owner->IsEnabled());
		}
		if(size_changed && IsGpuReady() && sz.cx > 0 && sz.cy > 0)
			host.Refresh();
	}

	void StartGpuSession()
	{
		if(destroying)
			return;
		init_attempted = true;
		if(backend_kind != GpuBackendKind::Vulkan) {
			gpu_ready = false;
			if(backend_kind == GpuBackendKind::Unknown)
				SetSessionError("backend not selected");
			else
				SetSessionError("backend not supported");
			return;
		}

		GpuNativeWindowDesc native_window;
		String native_error;
		if(!BuildWin32GpuNativeWindowDesc(const_cast<Host&>(host).GetHWND(), native_window, native_error)) {
			SetSessionError(native_error);
			return;
		}

		backend = CreateBackendSession(backend_kind);
		if(!backend) {
			SetSessionError("backend not supported");
			return;
		}

		String open_error;
		if(!backend->Open(validation_requested, native_window, open_error)) {
			SetSessionError(open_error);
			backend.Clear();
			return;
		}

		gpu_ready = backend->IsReady();
		if(gpu_ready) {
			ClearError();
			host.Refresh();
		}
		else
			SetSessionError(backend->GetError());
	}

	void StopGpuSession()
	{
		if(backend)
			backend->Close();
		backend.Clear();
		ClearError();
		session_error.Clear();
		presentation_error.Clear();
		host_ready = false;
		gpu_ready = false;
		init_attempted = false;
	}

	void SetApiError(const String& message)
	{
		api_error = message;
	}

	void SetSessionError(const String& message)
	{
		session_error = message;
		gpu_ready = false;
	}

	void ClearError()
	{
		api_error.Clear();
		session_error.Clear();
		presentation_error.Clear();
	}

	GpuCtrl *owner = nullptr;
	Host host;
	One<GpuCtrlBackendSession> backend;
	GpuBackendKind backend_kind = GpuBackendKind::Vulkan;
	String api_error;
	String session_error;
	String presentation_error;
	Size last_host_size = Size(-1, -1);
	bool validation_requested = false;
	bool host_ready = false;
	bool gpu_ready = false;
	bool init_attempted = false;
	bool destroying = false;
};

GpuCtrl::GpuCtrl()
{
	BackPaint(EXCLUDEPAINT);
	impl.Create(*this);
	Add(impl->host.SizePos());
}

GpuCtrl::~GpuCtrl()
{
	if(impl) {
		impl->destroying = true;
		impl->StopGpuSession();
	}
}

bool GpuCtrl::IsNativeHostReady() const
{
	return impl && impl->IsNativeHostReady();
}

bool GpuCtrl::IsGpuReady() const
{
	return impl && impl->IsGpuReady();
}

String GpuCtrl::GetGpuError() const
{
	return impl ? impl->GetGpuError() : String();
}

void GpuCtrl::RequestGpuRefresh()
{
	if(impl)
		impl->RequestGpuRefresh();
}

GpuCtrl& GpuCtrl::SetBackend(GpuBackendKind kind)
{
	if(impl)
		impl->SetBackend(kind);
	return *this;
}

GpuCtrl& GpuCtrl::SetValidation(bool validation)
{
	if(impl)
		impl->SetValidation(validation);
	return *this;
}

GpuCtrl& GpuCtrl::RetryGpuInit()
{
	if(impl)
		impl->RetryGpuInit();
	return *this;
}

void GpuCtrl::State(int reason)
{
	if(impl)
		impl->OnCtrlState(reason);
}

void GpuCtrl::Layout()
{
	Ctrl::Layout();
	if(impl)
		impl->SyncHostBounds();
}

}
