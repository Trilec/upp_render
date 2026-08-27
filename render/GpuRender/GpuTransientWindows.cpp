#include "GpuTransientWindows.h"
#include "GpuTopWindow.h"
#include "RenderCtrlBridge.h"

#ifdef PLATFORM_WIN32
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
#endif

namespace Upp {

#ifdef PLATFORM_WIN32
namespace {

class GpuTransientWindowHost;

static VectorMap<HWND, GpuTransientWindowHost *>& TransientHosts()
{
	static VectorMap<HWND, GpuTransientWindowHost *> hosts;
	return hosts;
}

static GpuTransientWindowHost *FindTransientHost(HWND hwnd)
{
	VectorMap<HWND, GpuTransientWindowHost *>& hosts = TransientHosts();
	int i = hosts.Find(hwnd);
	return i >= 0 ? hosts[i] : nullptr;
}

static GpuTopWindow *FindGpuOwner(Ctrl *ctrl)
{
	Ctrl *owner = ctrl ? ctrl->GetOwner() : nullptr;
	for(int depth = 0; owner && depth < 32; ++depth) {
		if(GpuTopWindow *gpu = dynamic_cast<GpuTopWindow *>(owner))
			return gpu;
		owner = owner->GetOwner();
	}
	return nullptr;
}

class GpuTransientWindowHost {
public:
	GpuTransientWindowHost(Ctrl& target, GpuTopWindow& root)
		: ctrl(&target), gpu_owner(&root)
	{
		hwnd = target.GetHWND();
	}

	~GpuTransientWindowHost()
	{
		presenter.Close();
	}

	bool Attach()
	{
		if(!ctrl || !gpu_owner || !hwnd || !IsWindow(hwnd))
			return false;

		GpuNativeWindowDesc native_window;
		String native_error;
		if(!BuildWin32GpuNativeWindowDesc(hwnd, native_window, native_error))
			return false;

		String open_error;
		if(!presenter.Open(gpu_owner->GetBackend(), gpu_owner->IsValidationRequested(),
		                   native_window, open_error))
			return false;

		old_proc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
		if(!old_proc) {
			presenter.Close();
			return false;
		}

		TransientHosts().Add(hwnd, this);
		SetLastError(0);
		LONG_PTR previous = SetWindowLongPtr(hwnd, GWLP_WNDPROC,
		                                     reinterpret_cast<LONG_PTR>(&WindowProc));
		if(previous == 0 && GetLastError() != 0) {
			int i = TransientHosts().Find(hwnd);
			if(i >= 0)
				TransientHosts().Remove(i);
			presenter.Close();
			old_proc = nullptr;
			return false;
		}
		return true;
	}

	bool HasPresentedFrame() const { return frame_presented; }
	bool IsActive() const { return gpu_active && presenter.IsReady(); }

	bool Present()
	{
		if(!ctrl || !IsActive() || !hwnd || !IsWindow(hwnd))
			return false;

		RECT rc{};
		if(!GetClientRect(hwnd, &rc))
			return false;
		Size size(max(0, (int)(rc.right - rc.left)), max(0, (int)(rc.bottom - rc.top)));
		if(size.cx <= 0 || size.cy <= 0)
			return true;

		UiDisplayList list;
		String error;
		if(!RecordCtrlDisplayList(*ctrl, list, error)) {
			last_error = error.IsEmpty() ? String("transient U++ control recording failed") : error;
			return false;
		}

		Rgba8 background = Rgba8::FromColor(SColorFace());
		if(!presenter.Present(size, list, background, error)) {
			last_error = error.IsEmpty() ? presenter.GetError() : error;
			return false;
		}

		last_error.Clear();
		frame_presented = true;
		return true;
	}

	void EnterSoftwareFallback()
	{
		presenter.Close();
		gpu_active = false;
		frame_presented = false;
		if(ctrl && ctrl->IsOpen())
			ctrl->Refresh();
	}

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		GpuTransientWindowHost *host = FindTransientHost(hwnd);
		if(!host)
			return DefWindowProc(hwnd, message, wParam, lParam);

		if(message == WM_NCDESTROY)
			return host->DetachForDestroy(message, wParam, lParam);

		if(message == WM_ERASEBKGND && host->IsActive() && host->HasPresentedFrame())
			return 1;

		if(message == WM_PAINT && host->IsActive()) {
			if(host->Present()) {
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
				return 0;
			}
			host->EnterSoftwareFallback();
		}

		return CallWindowProc(host->old_proc, hwnd, message, wParam, lParam);
	}

private:
	LRESULT DetachForDestroy(UINT message, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC proc = old_proc;
		presenter.Close();
		gpu_active = false;
		frame_presented = false;

		int i = TransientHosts().Find(hwnd);
		if(i >= 0)
			TransientHosts().Remove(i);

		if(proc)
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));

		LRESULT result = proc ? CallWindowProc(proc, hwnd, message, wParam, lParam)
		                      : DefWindowProc(hwnd, message, wParam, lParam);
		delete this;
		return result;
	}

	Ptr<Ctrl> ctrl;
	Ptr<GpuTopWindow> gpu_owner;
	HWND hwnd = nullptr;
	WNDPROC old_proc = nullptr;
	GpuDisplayPresenter presenter;
	String last_error;
	bool gpu_active = true;
	bool frame_presented = false;
};

static bool AttachTransientWindow(Ctrl *ctrl)
{
	if(!ctrl || dynamic_cast<GpuTopWindow *>(ctrl))
		return false;
	HWND hwnd = ctrl->GetHWND();
	if(!hwnd || !IsWindow(hwnd) || FindTransientHost(hwnd))
		return false;

	LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
	if(!(style & WS_POPUP))
		return false;

	GpuTopWindow *owner = FindGpuOwner(ctrl);
	if(!owner || !owner->IsGpuReady())
		return false;

	GpuTransientWindowHost *host = new GpuTransientWindowHost(*ctrl, *owner);
	if(!host->Attach()) {
		delete host;
		return false;
	}
	ctrl->Refresh();
	return true;
}

static bool TransientStateHook(Ctrl *ctrl, int reason)
{
	if(reason == Ctrl::OPEN)
		AttachTransientWindow(ctrl);
	return false;
}

} // namespace
#endif

void EnsureGpuTransientWindowSupport()
{
#ifdef PLATFORM_WIN32
	static bool installed = false;
	if(!installed) {
		Ctrl::InstallStateHook(&TransientStateHook);
		installed = true;
	}
#endif
}

}
