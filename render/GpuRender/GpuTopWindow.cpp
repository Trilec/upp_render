#include "GpuTopWindow.h"
#include "RenderCtrlBridge.h"

#ifdef PLATFORM_WIN32
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
#endif

namespace Upp {

struct GpuTopWindow::Impl {
	Impl(GpuTopWindow& _owner) : owner(&_owner) {}
	~Impl() { destroying = true; StopGpuSession(); }
	bool IsGpuReady() const { return presenter.IsReady(); }
	String GetGpuError() const
	{
		if(!api_error.IsEmpty()) return api_error;
		if(!session_error.IsEmpty()) return session_error;
		if(!presentation_error.IsEmpty()) return presentation_error;
		return presenter.GetError();
	}
	void RequestGpuRefresh() { if(owner && owner->IsOpen()) owner->Refresh(); }
	void RetryGpuInit()
	{
		if(destroying || !owner || !owner->IsOpen()) return;
		init_attempted = false;
		ClearError();
#ifdef PLATFORM_WIN32
		StartGpuSession(owner->GetHWND());
#endif
	}
	void SetBackend(GpuBackendKind kind)
	{
		if(kind == backend_kind) return;
		if(IsGpuReady()) { SetApiError("backend change while open is not supported"); return; }
		backend_kind = kind;
		init_attempted = false;
		ClearError();
		if(kind == GpuBackendKind::Unknown) SetApiError("backend not selected");
		else if(kind != GpuBackendKind::Vulkan) SetApiError("backend not supported");
	}
	void SetValidation(bool validation)
	{
		if(validation_requested == validation) return;
		if(IsGpuReady()) { SetApiError("validation change while open is not supported"); return; }
		validation_requested = validation;
		init_attempted = false;
		ClearError();
	}
#ifdef PLATFORM_WIN32
	void StartGpuSession(HWND hwnd)
	{
		if(destroying || init_attempted || !hwnd || !IsWindow(hwnd)) return;
		init_attempted = true;
		if(backend_kind != GpuBackendKind::Vulkan) { SetSessionError(backend_kind == GpuBackendKind::Unknown ? "backend not selected" : "backend not supported"); return; }
		GpuNativeWindowDesc native_window;
		String native_error;
		if(!BuildWin32GpuNativeWindowDesc(hwnd, native_window, native_error)) { SetSessionError(native_error); return; }
		String open_error;
		if(!presenter.Open(backend_kind, validation_requested, native_window, open_error)) { SetSessionError(open_error.IsEmpty() ? presenter.GetError() : open_error); return; }
		ClearError();
		if(owner) owner->Refresh();
	}
	Size GetClientSize(HWND hwnd) const
	{
		RECT rect{};
		if(!hwnd || !GetClientRect(hwnd, &rect)) return Size(0, 0);
		LONG width = rect.right - rect.left;
		LONG height = rect.bottom - rect.top;
		return Size(width > 0 ? (int)width : 0, height > 0 ? (int)height : 0);
	}
	bool PresentRoot(HWND hwnd)
	{
		if(!owner || !presenter.IsReady()) return false;
		Size size = GetClientSize(hwnd);
		if(size.cx <= 0 || size.cy <= 0) return true;
		UiDisplayList list;
		Rgba8 background;
		String error;
		if(!owner->BuildGpuFrame(size, list, background, error)) { presentation_error = error.IsEmpty() ? String("root GPU frame build failed") : error; return false; }
		if(!presenter.Present(size, list, background, error)) { presentation_error = error.IsEmpty() ? presenter.GetError() : error; return false; }
		presentation_error.Clear();
		return true;
	}
#endif
	void StopGpuSession() { presenter.Close(); init_attempted = false; session_error.Clear(); presentation_error.Clear(); }
	void SetApiError(const String& message) { api_error = message; }
	void SetSessionError(const String& message) { session_error = message; }
	void ClearError() { api_error.Clear(); session_error.Clear(); presentation_error.Clear(); }
	GpuTopWindow *owner = nullptr;
	GpuDisplayPresenter presenter;
	GpuBackendKind backend_kind = GpuBackendKind::Vulkan;
	String api_error;
	String session_error;
	String presentation_error;
	bool validation_requested = false;
	bool init_attempted = false;
	bool destroying = false;
};

GpuTopWindow::GpuTopWindow() { impl.Create(*this); }
GpuTopWindow::~GpuTopWindow() { if(impl) { impl->destroying = true; impl->StopGpuSession(); } }
bool GpuTopWindow::IsGpuReady() const { return impl && impl->IsGpuReady(); }
String GpuTopWindow::GetGpuError() const { return impl ? impl->GetGpuError() : String(); }
void GpuTopWindow::RequestGpuRefresh() { if(impl) impl->RequestGpuRefresh(); }
GpuTopWindow& GpuTopWindow::RetryGpuInit() { if(impl) impl->RetryGpuInit(); return *this; }
GpuTopWindow& GpuTopWindow::SetBackend(GpuBackendKind kind) { if(impl) impl->SetBackend(kind); return *this; }
GpuTopWindow& GpuTopWindow::SetValidation(bool validation) { if(impl) impl->SetValidation(validation); return *this; }
bool GpuTopWindow::BuildGpuFrame(Size, UiDisplayList& list, Rgba8& background, String& error)
{
	background = Rgba8(32, 32, 32, 255);
	error.Clear();
	if(!RecordCtrlDisplayList(*this, list, error)) {
		if(error.IsEmpty()) error = "root U++ control recording failed";
		return false;
	}
	return true;
}
#ifdef PLATFORM_WIN32
void GpuTopWindow::NcCreate(HWND hwnd) { TopWindow::NcCreate(hwnd); if(impl) impl->StartGpuSession(hwnd); }
void GpuTopWindow::PreDestroy() { if(impl) impl->StopGpuSession(); TopWindow::PreDestroy(); }
LRESULT GpuTopWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if(message == WM_ERASEBKGND && impl && impl->IsGpuReady()) return 1;
	if(message == WM_PAINT && impl && impl->IsGpuReady()) {
		HWND hwnd = GetHWND();
		if(hwnd && IsWindow(hwnd) && impl->PresentRoot(hwnd)) {
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
			return 0;
		}
	}
	return TopWindow::WindowProc(message, wParam, lParam);
}
#endif

}
