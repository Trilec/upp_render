#include "GpuCtrl.h"
#include "GpuCtrlTestHooks.h"
#include <RenderPlatformWin32/RenderPlatformWin32Internal.h>
#include "RenderPresentation.h"

namespace Upp {

namespace {
static bool BuildDefaultDisplayList(Size size, UiDisplayList& list, Rgba8& background, String& error)
{
	background = Rgba8(20, 61, 148, 255); error.Clear(); UiDisplayListBuilder builder;
	if(size.cx <= 0 || size.cy <= 0) { if(!builder.Finish(list)) { error = builder.GetError(); return false; } return true; }
	const double w=size.cx,h=size.cy,unit=max(1.0,min(w,h));
	builder.FillRect(Rectf(0.08*w,0.10*h,0.46*w,0.48*h),Rgba8(230,82,20,255));
	builder.StrokeRect(Rectf(0.12*w,0.16*h,0.62*w,0.70*h),max(1.0,unit*0.025),Rgba8(40,205,118,220));
	builder.Save(); builder.ClipRect(Rectf(0.24*w,0.18*h,0.90*w,0.88*h));
	Transform2D affine; affine.x.x=0.96; affine.x.y=0.16; affine.y.x=-0.10; affine.y.y=0.92; affine.t=Pointf(0.08*w,0.03*h); builder.ConcatTransform(affine);
	struct RoundedRect rounded(Rectf(0.30*w,0.24*h,0.76*w,0.72*h),max(1.0,unit*0.08)); builder.FillRoundedRect(rounded,Rgba8(55,118,238,188)); builder.Restore();
	builder.FillRect(Rectf(0.58*w,0.56*h,0.94*w,0.92*h),Rgba8(246,210,54,128));
	if(!builder.Finish(list)) { error=builder.GetError(); return false; } return true;
}
}

namespace GpuCtrlTestHooks {
bool BuildDefaultFrame(Size size, UiDisplayList& list, Rgba8& background, String& error) { return BuildDefaultDisplayList(size,list,background,error); }
}

struct GpuCtrl::Impl {
	class Host : public DHCtrl {
	public:
		Host(){NoWantFocus();} void Attach(Impl *p){impl=p;}
		Image MouseEvent(int event, Point p, int zdelta, dword keyflags) override
		{
			// DHCtrl owns the native child, but semantic input belongs to the
			// public control. Forward once, including wheel/cursor events.
			Ptr<GpuCtrl> target = impl ? impl->owner : nullptr;
			if(!target) return DHCtrl::MouseEvent(event, p, zdelta, keyflags);
			const Point offset = GetScreenView().TopLeft() - target->GetScreenView().TopLeft();
			// The callback may destroy both target and this host. No access after it.
			return target->MouseEvent(event, p + offset, zdelta, keyflags);
		}
		void State(int reason) override { if(reason==CLOSE){if(impl)impl->OnHostState(reason);DHCtrl::State(reason);return;} DHCtrl::State(reason);if(impl)impl->OnHostState(reason); }
		LRESULT WindowProc(UINT message,WPARAM wParam,LPARAM lParam) override { if(message==Win32GpuFrameClock::Message()){
			if(impl&&impl->clock.Consume(wParam)) {
				Ptr<Host> alive(this);
				// Continuous paint can starve low-priority WM_TIMER on a vsynced surface.
				// Service ordinary U++ timers before invalidating again, as GpuTopWindow does.
				if(!impl->timer_callback_active) {
					impl->timer_callback_active=true;
					TimerProc(msecs());
					if(alive&&alive->impl) alive->impl->timer_callback_active=false;
				}
				if(alive&&alive->impl&&IsWindowVisible(alive->GetHWND())&&
				   !IsIconic(GetAncestor(alive->GetHWND(),GA_ROOT))) alive->Refresh();
			}
			return 0;
		} if(message==WM_ERASEBKGND)return 1; if(message==WM_PAINT){PAINTSTRUCT ps;HWND hwnd=GetHWND();HDC dc=BeginPaint(hwnd,&ps);bool gpu_painted=impl&&impl->OnHostPaint();if(!gpu_painted)FillRect(dc,&ps.rcPaint,GetSysColorBrush(COLOR_WINDOW));EndPaint(hwnd,&ps);return 0;} return DHCtrl::WindowProc(message,wParam,lParam); }
	private: Impl *impl=nullptr;
	};
	Impl(GpuCtrl& o):owner(&o){host.Attach(this);} ~Impl(){destroying=true;StopGpuSession();}
	bool IsNativeHostReady() const { HWND hwnd=const_cast<Host&>(host).GetHWND();if(!hwnd||!IsWindow(hwnd))return false;DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);return pid==GetCurrentProcessId(); }
	bool IsGpuReady() const { return gpu_ready&&presenter.IsReady(); }
	String GetGpuError() const { if(!api_error.IsEmpty())return api_error;if(!session_error.IsEmpty())return session_error;if(!presentation_error.IsEmpty())return presentation_error;return presenter.GetError(); }
	Size GetNativeHostSize() const { HWND hwnd=const_cast<Host&>(host).GetHWND();RECT r{};if(!hwnd||!GetClientRect(hwnd,&r))return Size(0,0);return Size(max(0,(int)(r.right-r.left)),max(0,(int)(r.bottom-r.top))); }
	bool OnHostPaint()
	{
		if(!IsGpuReady()) return false;
		Size requested_size=GetNativeHostSize();
		if(custom_content) {
			UiDisplayList empty; String error;
			if(presenter.Present(requested_size, empty, Rgba8(), error)) { presentation_error.Clear(); return true; }
			presentation_error = error.IsEmpty() ? presenter.GetError() : error; return false;
		}
		UiDisplayList list; Rgba8 background; String error; bool built=false;
		if(owner&&owner->WhenBuildFrame)
			built=owner->WhenBuildFrame(requested_size,list,background,error);
		else {
			GpuPainter painter(requested_size);
			if(owner) { owner->GpuPaint(painter); if(owner->WhenGpuPaint) owner->WhenGpuPaint(painter); }
			built=painter.FinishFrame(list,background,error);
		}
		if(!built){presentation_error=error.IsEmpty()?String("GpuCtrl frame builder failed"):error;return false;}
		if(!list.IsValid()){presentation_error=list.GetError().IsEmpty()?String("GpuCtrl frame builder returned an invalid display list"):list.GetError();return false;}
		if(presenter.Present(requested_size,list,background,error)){presentation_error.Clear();return true;}
		presentation_error=error.IsEmpty()?presenter.GetError():error;return false;
	}
	void RequestGpuRefresh(){if(IsNativeHostReady())host.Refresh();}
	void RetryGpuInit(){if(destroying)return;init_attempted=false;ClearError();if(host_ready&&!gpu_ready)StartGpuSession();}
	void SetBackend(GpuBackendKind kind){if(kind==backend_kind)return;if(IsGpuReady()){SetApiError("backend change while open is not supported");return;}backend_kind=kind;init_attempted=false;if(kind==GpuBackendKind::Vulkan)ClearError();else if(kind!=GpuBackendKind::Unknown)SetApiError("backend not supported");else SetApiError("backend not selected");if(host_ready&&!gpu_ready&&kind==GpuBackendKind::Vulkan)StartGpuSession();}
	void SetValidation(bool validation){if(validation_requested==validation)return;if(IsGpuReady()){SetApiError("validation change while open is not supported");return;}validation_requested=validation;init_attempted=false;ClearError();if(host_ready&&!gpu_ready)StartGpuSession();}
	void OnHostState(int reason){if(destroying)return;switch(reason){case OPEN:case SHOW:case ENABLE:case POSITION:case LAYOUTPOS:host_ready=IsNativeHostReady();if(host_ready&&!gpu_ready&&!init_attempted){ClearError();StartGpuSession();}SyncHostBounds();if(reason==SHOW&&IsGpuReady()){StartClock();presentation_error.Clear();host.Refresh();}break;case CLOSE:StopGpuSession();host_ready=false;break;}}
	void OnCtrlState(int reason){switch(reason){case CLOSE:StopGpuSession();host_ready=false;break;case SHOW:SyncHostBounds();if(IsGpuReady())StartClock();break;case ENABLE:case POSITION:case LAYOUTPOS:SyncHostBounds();break;}}
	// Preserve DHCtrl's disabled native-child convention: native input reaches
	// the top-level U++ dispatcher, which hit-tests this child and forwards once.
	// Semantic enabled state is inherited through the Ctrl tree, not HWND input.
	void SyncHostBounds(){Size sz=owner->GetSize();bool changed=sz!=last_host_size;if(changed)presentation_error.Clear();last_host_size=sz;host.SetRect(0,0,sz.cx,sz.cy);if(IsNativeHostReady())EnableWindow(host.GetHWND(),FALSE);if(changed&&IsGpuReady()&&sz.cx>0&&sz.cy>0)host.Refresh();}
	void StartGpuSession(){if(destroying)return;init_attempted=true;if(backend_kind!=GpuBackendKind::Vulkan){gpu_ready=false;SetSessionError(backend_kind==GpuBackendKind::Unknown?"backend not selected":"backend not supported");return;}GpuNativeWindowDesc native_window;String native_error;if(!BuildWin32GpuNativeWindowDesc(const_cast<Host&>(host).GetHWND(),native_window,native_error)){SetSessionError(native_error);return;}String open_error;if(!presenter.Open(backend_kind,validation_requested,native_window,open_error)){SetSessionError(open_error.IsEmpty()?presenter.GetError():open_error);return;}gpu_ready=presenter.IsReady();if(gpu_ready){ClearError();StartClock();host.Refresh();}else SetSessionError(presenter.GetError());}
	void StopGpuSession(){clock.Stop();presenter.Close();ClearError();session_error.Clear();presentation_error.Clear();host_ready=false;gpu_ready=false;init_attempted=false;}
	void SetApiError(const String& m){api_error=m;} void SetSessionError(const String& m){session_error=m;gpu_ready=false;} void ClearError(){api_error.Clear();session_error.Clear();presentation_error.Clear();}
	void StartClock() {
		clock.Stop();
		if(continuous_fps && IsGpuReady() && owner->IsVisible() && !clock.Start(host.GetHWND(), continuous_fps))
			SetApiError("GPU continuous refresh clock could not start");
	}
	Win32GpuFrameClock clock;
	int continuous_fps = 0;
	bool custom_content = false, timer_callback_active = false;
	GpuCtrl *owner=nullptr; Host host; GpuDisplayPresenter presenter; GpuBackendKind backend_kind=GpuBackendKind::Vulkan; String api_error,session_error,presentation_error; Size last_host_size=Size(-1,-1); bool validation_requested=false,host_ready=false,gpu_ready=false,init_attempted=false,destroying=false;
};

GpuCtrl::GpuCtrl(){BackPaint(EXCLUDEPAINT);impl.Create(*this);Add(impl->host.SizePos());}
GpuCtrl::~GpuCtrl(){if(impl){impl->destroying=true;impl->StopGpuSession();}}
GpuCtrl& GpuCtrl::SetGpuContent(std::unique_ptr<GpuSurfaceContent> content)
{
	if(!impl) return *this;
	const bool custom = content != nullptr; String error;
	if(IsOpen() || !impl->presenter.SetContent(std::move(content), error)) {
		impl->SetApiError(error.IsEmpty() ? String("GPU content must be selected before opening") : error);
		return *this;
	}
	impl->custom_content = custom; return *this;
}
GpuCtrl& GpuCtrl::SetContinuousRefresh(int fps)
{
	if(!impl) return *this;
	if(fps < 0 || fps > 240) { impl->SetApiError("GPU refresh target must be 0..240"); return *this; }
	impl->continuous_fps = fps;
	if(impl->IsGpuReady()) impl->StartClock();
	return *this;
}
GpuPresentationStats GpuCtrl::GetGpuStats() const { return impl ? impl->presenter.GetStats() : GpuPresentationStats(); }
void GpuCtrl::GpuPaint(GpuPainter&) {}
bool GpuCtrl::IsNativeHostReady()const{return impl&&impl->IsNativeHostReady();}
bool GpuCtrl::IsGpuReady()const{return impl&&impl->IsGpuReady();}
String GpuCtrl::GetGpuError()const{return impl?impl->GetGpuError():String();}
void GpuCtrl::RequestGpuRefresh(){if(impl)impl->RequestGpuRefresh();}
GpuCtrl& GpuCtrl::SetBackend(GpuBackendKind kind){if(impl)impl->SetBackend(kind);return *this;}
GpuCtrl& GpuCtrl::SetValidation(bool validation){if(impl)impl->SetValidation(validation);return *this;}
GpuCtrl& GpuCtrl::RetryGpuInit(){if(impl)impl->RetryGpuInit();return *this;}
void GpuCtrl::State(int reason){if(impl)impl->OnCtrlState(reason);}
void GpuCtrl::Layout(){Ctrl::Layout();if(impl)impl->SyncHostBounds();}

}
