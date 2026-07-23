#include "GpuCtrl.h"
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
	}

	bool IsReady() const override
	{
		return session.IsReady();
	}

	const String& GetError() const override
	{
		return session.GetError();
	}

private:
	VulkanSurfaceSession session;
};

static One<GpuCtrlBackendSession> CreateBackendSession(GpuBackendKind kind)
{
	if(kind == GpuBackendKind::Vulkan)
		return new VulkanGpuCtrlBackendSession;
	return One<GpuCtrlBackendSession>();
}

}

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
				BeginPaint(hwnd, &ps);
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
		return api_error.IsEmpty() ? session_error : api_error;
	}

	void SetShutdownCallback(Callback3<bool, bool, const String&> cb)
	{
		shutdown_callback = cb;
	}

	void RequestGpuRefresh()
	{
		if(IsGpuReady())
			host.Refresh();
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
		if(kind == GpuBackendKind::Vulkan)
			ClearError();
		else if(kind != GpuBackendKind::Unknown)
			SetApiError("backend not supported");
		else
			SetApiError("backend not selected");
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
			if(host_ready && !gpu_ready) {
				ClearError();
				StartGpuSession();
			}
			SyncHostBounds();
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
		host.SetRect(0, 0, sz.cx, sz.cy);
		if(IsNativeHostReady()) {
			HWND hwnd = host.GetHWND();
			EnableWindow(hwnd, owner->IsEnabled());
		}
	}

	void StartGpuSession()
	{
		if(destroying)
			return;
		if(backend_kind != GpuBackendKind::Vulkan) {
			gpu_ready = false;
			if(backend_kind == GpuBackendKind::Unknown)
				SetSessionError("backend not selected");
			else
				SetSessionError("backend not supported");
			return;
		}

		GpuNativeWindowDesc native_window;
		String desc_error;
		if(!BuildNativeWindowDesc(native_window, desc_error)) {
			SetSessionError(desc_error);
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
		if(gpu_ready)
			ClearError();
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
		host_ready = false;
		gpu_ready = false;
	}

	bool BuildNativeWindowDesc(GpuNativeWindowDesc& desc, String& native_error) const
	{
		desc = GpuNativeWindowDesc();
		HWND hwnd = const_cast<Host&>(host).GetHWND();
		if(!hwnd || !IsWindow(hwnd)) {
			native_error = "native host not ready";
			return false;
		}
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		if(pid != GetCurrentProcessId()) {
			native_error = "native host belongs to another process";
			return false;
		}
		desc.kind = GpuNativeWindowKind::Win32;
		desc.handle = (uintptr_t)hwnd;
		return true;
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
	}

	GpuCtrl *owner = nullptr;
	Host host;
	One<GpuCtrlBackendSession> backend;
	GpuBackendKind backend_kind = GpuBackendKind::Unknown;
	String api_error;
	String session_error;
	Callback3<bool, bool, const String&> shutdown_callback;
	bool validation_requested = false;
	bool host_ready = false;
	bool gpu_ready = false;
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
			if(impl->shutdown_callback)
				impl->shutdown_callback(impl->host_ready, impl->gpu_ready, impl->GetGpuError());
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

GpuCtrl& GpuCtrl::SetShutdownCallback(Callback3<bool, bool, const String&> cb)
{
	if(impl)
		impl->SetShutdownCallback(cb);
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
