#include "GpuCtrl.h"
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
#include <RenderVulkan/RenderVulkanSurfaceSession.h>

namespace Upp {

namespace {

class GpuCtrlBackendSession {
public:
	virtual ~GpuCtrlBackendSession() {}

	virtual bool Open(bool request_validation, const GpuNativeWindowDesc& native_window, String& error) = 0;
	virtual void Close() = 0;
	virtual bool IsReady() const = 0;
	virtual const String& GetError() const = 0;
	virtual bool Present(Size requested_size, String& error) = 0;
};

class VulkanGpuCtrlBackendSession : public GpuCtrlBackendSession {
public:
	bool Open(bool request_validation, const GpuNativeWindowDesc& native_window, String& error) override
	{
		if(session.Open(request_validation, native_window)) {
			error.Clear();
			return true;
		}
		error = session.GetError();
		return false;
	}

	void Close() override
	{
		session.Close();
		swapchain_request_size = Size(0, 0);
	}

	bool IsReady() const override
	{
		return session.IsReady();
	}

	const String& GetError() const override
	{
		return session.GetError();
	}

	bool Present(Size requested_size, String& error) override
	{
		error.Clear();
		if(requested_size.cx <= 0 || requested_size.cy <= 0)
			return true;
		if(!session.IsReady()) {
			error = session.GetError();
			return false;
		}
		if(!EnsureSwapchain(requested_size, error))
			return false;
		if(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f))
			return true;

		error = session.GetFrameReport().error;
		if(!session.GetFrameReport().out_of_date)
			return false;

		if(!session.DestroySwapchain()) {
			if(error.IsEmpty())
				error = session.GetReport().swapchain_error;
			if(error.IsEmpty())
				error = "Vulkan swapchain cleanup failed";
			return false;
		}
		swapchain_request_size = Size(0, 0);
		if(!EnsureSwapchain(requested_size, error))
			return false;
		if(session.PresentClearFrame(0.08f, 0.24f, 0.58f, 1.0f)) {
			error.Clear();
			return true;
		}
		error = session.GetFrameReport().error;
		return false;
	}

private:
	bool EnsureSwapchain(Size requested_size, String& error)
	{
		if(session.HasSwapchain() && requested_size != swapchain_request_size) {
			if(!session.DestroySwapchain()) {
				error = session.GetReport().swapchain_error;
				if(error.IsEmpty())
					error = "Vulkan swapchain cleanup failed";
				return false;
			}
			swapchain_request_size = Size(0, 0);
		}
		if(!session.HasSwapchain()) {
			if(!session.CreateSwapchain(requested_size)) {
				error = session.GetReport().swapchain_error;
				return false;
			}
			swapchain_request_size = requested_size;
		}
		return true;
	}

	VulkanSurfaceSession session;
	Size swapchain_request_size = Size(0, 0);
};

static One<GpuCtrlBackendSession> CreateBackendSession(GpuBackendKind kind)
{
	if(kind == GpuBackendKind::Vulkan)
		return new VulkanGpuCtrlBackendSession;
	return One<GpuCtrlBackendSession>();
}

}

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
		// API errors describe rejected configuration; session errors describe
		// backend bring-up failures; presentation errors preserve a healthy
		// session so a later invalidation can recover without a repaint loop.
		if(!api_error.IsEmpty())
			return api_error;
		if(!session_error.IsEmpty())
			return session_error;
		return presentation_error;
	}

	bool OnHostPaint()
	{
		if(!IsGpuReady() || !backend)
			return false;
		Size requested_size = host.GetSize();
		String error;
		if(backend->Present(requested_size, error)) {
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
		// One automatic attempt happens on first host readiness; explicit retry is
		// the deterministic path after a failure.
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
			if(reason == SHOW && IsGpuReady())
				host.Refresh();
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
		// Host sizing stays backend-neutral. A real size change invalidates one
		// frame; the private backend reconciles its swapchain on that paint.
		Size sz = owner->GetSize();
		bool size_changed = sz != last_host_size;
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
		// Vulkan is the current backend baseline; other values remain explicit
		// configuration errors until their real implementations arrive.
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

		String session_error;
		if(!backend->Open(validation_requested, native_window, session_error)) {
			SetSessionError(session_error);
			backend.Clear();
			return;
		}

		gpu_ready = backend->IsReady();
		if(gpu_ready) {
			ClearError();
			// Initial presentation is event driven exactly once after session bring-up.
			host.Refresh();
		}
		else
			SetSessionError(backend->GetError());
	}

	void StopGpuSession()
	{
		// Release backend resources before the child HWND disappears.  The child
		// host is a U++ implementation detail; the surface session is the actual
		// backend lifetime boundary for now.
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
	// Vulkan is the current default so ordinary code can just add the control;
	// explicit backend selection remains available for tests and future backends.
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
