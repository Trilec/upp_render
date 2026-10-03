#include "GpuTopWindow.h"
#include "GpuTransientWindows.h"
#include "RenderCtrlBridge.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef PLATFORM_WIN32
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
#endif

namespace Upp {

struct GpuTopWindow::Impl {
	Impl(GpuTopWindow& _owner) : owner(&_owner) {}
	~Impl() { destroying = true; StopGpuSession(); }
	bool IsGpuReady() const { return worker.joinable() ? worker_ready : presenter.IsReady(); }
	GpuPresentationStats GetStats() const
	{
		if(!worker.joinable()) return presenter.GetStats();
		std::lock_guard<std::mutex> guard(work_mutex);
		GpuPresentationStats result = worker_stats;
		result.dropped_frames = dropped_frames;
		result.pending_frames = pending_frame ? 1 : 0;
		return result;
	}
	struct PendingFrame {
		Size size;
		UiDisplayList list;
		Rgba8 background;
	};
	void StartWorker()
	{
		if(!async_presentation || worker.joinable()) return;
		worker_ready = presenter.IsReady();
		worker_stop = false;
		worker_error.Clear();
		worker_stats = GpuPresentationStats();
		dropped_frames = 0;
		worker = std::thread([this] {
			for(;;) {
				std::unique_ptr<PendingFrame> frame;
				{
					std::unique_lock<std::mutex> guard(work_mutex);
					work_changed.wait(guard, [this] { return worker_stop || pending_frame != nullptr; });
					if(worker_stop) break;
					frame = std::move(pending_frame);
				}
				String error;
				bool ok = presenter.Present(frame->size, frame->list, frame->background, error);
				GpuPresentationStats snapshot = presenter.GetStats();
				{
					std::lock_guard<std::mutex> guard(work_mutex);
					worker_stats = snapshot;
					if(!ok) worker_error = error.IsEmpty() ? String("asynchronous GPU replay failed") : error;
				}
				if(ok) frame_presented.store(true);
			}
		});
	}
	void StopWorker()
	{
		if(!worker.joinable()) return;
		{
			std::lock_guard<std::mutex> guard(work_mutex);
			worker_stop = true;
			pending_frame.reset();
		}
		work_changed.notify_one();
		{
			GuiUnlock unlock;
			worker.join();
		}
		worker_ready = false;
	}
	String WorkerError() const
	{
		std::lock_guard<std::mutex> guard(work_mutex);
		return worker_error;
	}
	bool HasPresentedFrame() const { return frame_presented.load(); }
	String GetGpuError() const
	{
		if(!api_error.IsEmpty()) return api_error;
		if(!session_error.IsEmpty()) return session_error;
		if(!presentation_error.IsEmpty()) return presentation_error;
		return worker.joinable() ? WorkerError() : presenter.GetError();
	}
	void RequestGpuRefresh() { if(owner && owner->IsOpen()) owner->Refresh(); }
	void RetryGpuInit()
	{
		if(destroying || !owner || !owner->IsOpen()) return;
		StopWorker();
		init_attempted = false;
		frame_presented = false;
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
		frame_presented = false;
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
		frame_presented = false;
		ClearError();
	}
#ifdef PLATFORM_WIN32
	void StartGpuSession(HWND hwnd)
	{
		if(destroying || init_attempted || !hwnd || !IsWindow(hwnd)) return;
		init_attempted = true;
		frame_presented = false;
		if(backend_kind != GpuBackendKind::Vulkan) { SetSessionError(backend_kind == GpuBackendKind::Unknown ? "backend not selected" : "backend not supported"); return; }
		GpuNativeWindowDesc native_window;
		String native_error;
		if(!BuildWin32GpuNativeWindowDesc(hwnd, native_window, native_error)) { SetSessionError(native_error); return; }
		String open_error;
		if(!presenter.Open(backend_kind, validation_requested, native_window, open_error)) { SetSessionError(open_error.IsEmpty() ? presenter.GetError() : open_error); return; }
		ClearError();
		StartWorker();
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
		if(!owner || !IsGpuReady()) return false;
		if(worker.joinable() && !WorkerError().IsEmpty()) return false;
		Size size = GetClientSize(hwnd);
		if(size.cx <= 0 || size.cy <= 0) return true;
		UiDisplayList list;
		Rgba8 background;
		String error;
		if(!owner->BuildGpuFrame(size, list, background, error)) { presentation_error = error.IsEmpty() ? String("root GPU frame build failed") : error; return false; }
		if(worker.joinable()) {
			std::unique_ptr<PendingFrame> frame(new PendingFrame);
			frame->size = size;
			frame->list = pick(list);
			frame->background = background;
			{
				std::lock_guard<std::mutex> guard(work_mutex);
				if(pending_frame) ++dropped_frames;
				pending_frame = std::move(frame);
			}
			work_changed.notify_one();
			return true;
		}
		if(!presenter.Present(size, list, background, error)) { presentation_error = error.IsEmpty() ? presenter.GetError() : error; return false; }
		presentation_error.Clear();
		frame_presented = true;
		return true;
	}
	void EnterSoftwareFallback()
	{
		String failure = GetGpuError();
		if(failure.IsEmpty())
			failure = "root GPU presentation failed";
		StopWorker();
		presenter.Close();
		init_attempted = false;
		frame_presented = false;
		session_error.Clear();
		presentation_error = failure;
		if(owner && owner->IsOpen())
			owner->Refresh();
	}
#endif
	void StopGpuSession()
	{
		StopWorker();
		presenter.Close();
		init_attempted = false;
		frame_presented = false;
		session_error.Clear();
		presentation_error.Clear();
	}
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
	std::atomic<bool> frame_presented { false };
	bool async_presentation = false;
	bool worker_ready = false;
	bool worker_stop = false;
	mutable std::mutex work_mutex;
	std::condition_variable work_changed;
	std::thread worker;
	std::unique_ptr<PendingFrame> pending_frame;
	GpuPresentationStats worker_stats;
	String worker_error;
	uint64 dropped_frames = 0;
	bool destroying = false;
};

GpuTopWindow::GpuTopWindow()
{
	EnsureGpuTransientWindowSupport();
	impl.Create(*this);
}
GpuTopWindow::~GpuTopWindow() { if(impl) { impl->destroying = true; impl->StopGpuSession(); } }
void GpuTopWindow::Close()
{
	if(InLoop()) {
		TopWindow::Close();
		return;
	}
	if(impl)
		impl->StopGpuSession();
	TopWindow::Close();
}
GpuTopWindow& GpuTopWindow::SetAsyncPresentation(bool enabled)
{
	if(impl) {
		if(IsOpen()) impl->SetApiError("presentation mode must be selected before opening");
		else impl->async_presentation = enabled;
	}
	return *this;
}
GpuPresentationStats GpuTopWindow::GetGpuStats() const { return impl ? impl->GetStats() : GpuPresentationStats(); }
bool GpuTopWindow::IsGpuReady() const { return impl && impl->IsGpuReady(); }
String GpuTopWindow::GetGpuError() const { return impl ? impl->GetGpuError() : String(); }
GpuBackendKind GpuTopWindow::GetBackend() const { return impl ? impl->backend_kind : GpuBackendKind::Unknown; }
bool GpuTopWindow::IsValidationRequested() const { return impl && impl->validation_requested; }
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
	if(message == WM_ERASEBKGND && impl && impl->IsGpuReady() && impl->HasPresentedFrame())
		return 1;
	if(message == WM_PAINT && impl && impl->IsGpuReady()) {
		HWND hwnd = GetHWND();
		if(hwnd && IsWindow(hwnd)) {
			if(impl->PresentRoot(hwnd)) {
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
				return 0;
			}
			impl->EnterSoftwareFallback();
		}
	}
	return TopWindow::WindowProc(message, wParam, lParam);
}
#endif

}
