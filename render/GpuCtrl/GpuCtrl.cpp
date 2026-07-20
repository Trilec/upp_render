#include "GpuCtrl.h"

namespace Upp {

GpuCtrl::Host::Host()
{
	NoWantFocus();
}

void GpuCtrl::Host::Attach(GpuCtrl& _owner)
{
	owner = &_owner;
}

void GpuCtrl::Host::State(int reason)
{
	DHCtrl::State(reason);
	if(owner)
		owner->OnHostState(reason);
}

LRESULT GpuCtrl::Host::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
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

GpuCtrl::GpuCtrl()
{
	BackPaint(EXCLUDEPAINT);
	host.Attach(*this);
	Add(host.SizePos());
}

GpuCtrl::~GpuCtrl()
{
	destroying = true;
}

bool GpuCtrl::IsNativeHostReady() const
{
	HWND hwnd = const_cast<Host&>(host).GetHWND();
	return host_ready && hwnd && IsWindow(hwnd);
}

String GpuCtrl::GetGpuError() const
{
	return error;
}

void GpuCtrl::RequestGpuRefresh()
{
	if(IsNativeHostReady())
		host.Refresh();
}

GpuCtrl& GpuCtrl::SetBackend(GpuBackendKind kind)
{
	backend_kind = kind;
	return *this;
}

void GpuCtrl::SetError(const String& message)
{
	error = message;
}

void GpuCtrl::ClearError()
{
	error.Clear();
}

void GpuCtrl::OnHostState(int reason)
{
	if(destroying)
		return;
	switch(reason) {
	case OPEN:
		{
			HWND hwnd = host.GetHWND();
			bool ready = hwnd && IsWindow(hwnd);
			if(ready) {
				host_ready = true;
				ClearError();
			}
			else {
				host_ready = false;
				SetError("native host not ready");
			}
		}
		SyncHostBounds();
		break;
	case CLOSE:
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

void GpuCtrl::SyncHostBounds()
{
	Size sz = GetSize();
	host.SetRect(0, 0, sz.cx, sz.cy);
	if(IsNativeHostReady()) {
		HWND hwnd = host.GetHWND();
		EnableWindow(hwnd, IsEnabled());
	}
}

void GpuCtrl::State(int reason)
{
	switch(reason) {
	case CLOSE:
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

void GpuCtrl::Layout()
{
	Ctrl::Layout();
	SyncHostBounds();
}

}
