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
	if(reason == CLOSE) {
		if(owner)
			owner->OnHostState(reason);
		DHCtrl::State(reason);
		return;
	}
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
	StopGpuSession();
}

bool GpuCtrl::IsNativeHostReady() const
{
	HWND hwnd = const_cast<Host&>(host).GetHWND();
	if(!host_ready || !hwnd || !IsWindow(hwnd))
		return false;
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	return pid == GetCurrentProcessId();

}

bool GpuCtrl::IsGpuReady() const
{
	return gpu_ready && session.IsReady();
}

String GpuCtrl::GetGpuError() const
{
	return error;
}

void GpuCtrl::RequestGpuRefresh()
{
	if(IsGpuReady())
		host.Refresh();
}

GpuCtrl& GpuCtrl::SetBackend(GpuBackendKind kind)
{
	if(IsGpuReady() && kind != backend_kind) {
		SetError("backend change while open is not supported");
		return *this;
	}
	backend_kind = kind;
	if(kind == GpuBackendKind::Vulkan)
		ClearError();
	else if(kind != GpuBackendKind::Unknown)
		SetError("backend not supported");
	return *this;
}

GpuCtrl& GpuCtrl::SetValidation(bool validation)
{
	if(IsGpuReady()) {
		SetError("validation change while open is not supported");
		return *this;
	}
	validation_requested = validation;
	return *this;
}

bool GpuCtrl::BuildNativeWindowDesc(GpuNativeWindowDesc& desc, String& error) const
{
	desc = GpuNativeWindowDesc();
	HWND hwnd = const_cast<Host&>(host).GetHWND();
	if(!host_ready || !hwnd || !IsWindow(hwnd)) {
		error = "native host not ready";
		return false;
	}
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if(pid != GetCurrentProcessId()) {
		error = "native host belongs to another process";
		return false;
	}
	desc.kind = GpuNativeWindowKind::Win32;
	desc.handle = (uintptr_t)hwnd;
	return true;
}

void GpuCtrl::SetError(const String& message)
{
	error = message;
	gpu_ready = false;
}

void GpuCtrl::ClearError()
{
	error.Clear();
}

void GpuCtrl::StartGpuSession()
{
	if(destroying)
		return;
	if(backend_kind != GpuBackendKind::Vulkan) {
		gpu_ready = false;
		if(backend_kind == GpuBackendKind::Unknown)
			SetError("backend not selected");
		else
			SetError("backend not supported");
		return;
	}
	GpuNativeWindowDesc native_window;
	String desc_error;
	if(!BuildNativeWindowDesc(native_window, desc_error)) {
		SetError(desc_error);
		return;
	}
	if(!session.Open(validation_requested, native_window)) {
		SetError(session.GetError());
		gpu_ready = false;
		return;
	}
	gpu_ready = session.IsReady();
	if(gpu_ready)
		ClearError();
	else
		SetError(session.GetError());
}

void GpuCtrl::StopGpuSession()
{
	session.Close();
	gpu_ready = false;
}

void GpuCtrl::OnHostState(int reason)
{
	if(destroying)
		return;
	switch(reason) {
	case OPEN:
		host_ready = IsNativeHostReady();
		if(!host_ready) {
			SetError("native host not ready");
			break;
		}
		ClearError();
		StartGpuSession();
		SyncHostBounds();
		break;
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

void GpuCtrl::Layout()
{
	Ctrl::Layout();
	SyncHostBounds();
}

}
