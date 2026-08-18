#include "GpuCtrl.h"
#include "GpuCtrlTestHooks.h"
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
#include <RenderPresentation/RenderPresentation.h>

namespace Upp {

namespace {

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
	struct RoundedRect rounded(Rectf(0.30 * w, 0.24 * h, 0.76 * w, 0.72 * h),
	                           max(1.0, unit * 0.08));
	builder.FillRoundedRect(rounded, Rgba8(55, 118, 238, 188));
	builder.Restore();
	builder.FillRect(Rectf(0.58 * w, 0.56 * h, 0.94 * w, 0.92 * h),
	                 Rgba8(246, 210, 54, 128));

	if(!builder.Finish(list)) {
		error = builder.GetError();
		return false;
	}
	return true;
}

} // namespace

namespace GpuCtrlTestHooks {

bool BuildDefaultFrame(Size size, UiDisplayList& list, Rgba8& background, String& error)
{
	return BuildDefaultDisplayList(size, list, background, error);
}

} // namespace GpuCtrlTestHooks

// The public control stays tiny. Native-child lifecycle lives here while the
// backend/session/render/swapchain ownership is shared with root presentation
// through GpuDisplayPresenter.
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
			// CLOSE must reach the implementation before DHCtrl tears down the
			// native child window that owns the Vulkan surface.
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
		return gpu_ready && presenter.IsReady();
	}

	String GetGpuError() const
	{
		if(!api_error.IsEmpty())
			return api_error;
		if(!session_error.IsEmpty())
			return session_error;
		if(!presentation_error.IsEmpty())
			return presentation_error;
		return presenter.GetError();
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
		if(!IsGpuReady())
			return false;
		Size requested_size = GetNativeHostSize();
		UiDisplayList list;
		Rgba8 background;
		String error;
		bool built = owner && owner->WhenBuildFrame
		           ? owner->WhenBuildFrame(requested_size, list, background, error)
		           : BuildDefaultDisplayList(requested_size, list, background, error);
		if(!built) {
			presentation_error = error.IsEmpty() ? String("GpuCtrl frame builder failed") : error;
			return false;
		}
		if(!list.IsValid()) {
			presentation_error = list.GetError().IsEmpty() ? String("GpuCtrl frame builder returned an invalid display list")
			                                             : list.GetError();
			return false;
		}
		if(presenter.Present(requested_size, list, background, error)) {
			presentation_error.Clear();
			return true;
		}
		presentation_error = error.IsEmpty() ? presenter.GetError() : error;
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
		if(IsGpuReady()) {
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
		if(!BuildWin32GpuNativeWindowDesc(const_cast<Host&>(host).GetHWND(),
		                                 native_window, native_error)) {
			SetSessionError(native_error);
			return;
		}

		String open_error;
		if(!presenter.Open(backend_kind, validation_requested, native_window, open_error)) {
			SetSessionError(open_error.IsEmpty() ? presenter.GetError() : open_error);
			return;
		}

		gpu_ready = presenter.IsReady();
		if(gpu_ready) {
			ClearError();
			host.Refresh();
		}
		else
			SetSessionError(presenter.GetError());
	}

	void StopGpuSession()
	{
		presenter.Close();
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
	GpuDisplayPresenter presenter;
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
