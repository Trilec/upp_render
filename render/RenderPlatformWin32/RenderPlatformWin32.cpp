#include "RenderPlatformWin32.h"
#include "RenderPlatformWin32Internal.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace Upp {

struct Win32GpuFrameClock::Impl {
	HANDLE timer = nullptr, stop = nullptr;
	std::thread thread;
	std::atomic<bool> pending { false }, running { false };
	uintptr_t generation = 0;
	bool active = false;
};

Win32GpuFrameClock::Win32GpuFrameClock() { impl.Create(); }
Win32GpuFrameClock::~Win32GpuFrameClock() { Stop(); }
UINT Win32GpuFrameClock::Message()
{
	static const UINT message = RegisterWindowMessageW(L"UppRender.FrameClock.v1");
	return message;
}
bool Win32GpuFrameClock::IsActive() const { return impl->active && impl->running.load(); }

bool Win32GpuFrameClock::Start(HWND hwnd, int pulse_hz)
{
	Stop();
	if(!hwnd || !IsWindow(hwnd) || !Message() || pulse_hz < 1 || pulse_hz > 240) return false;
	DWORD pid = 0;
	if(GetWindowThreadProcessId(hwnd, &pid) != GetCurrentThreadId() || pid != GetCurrentProcessId())
		return false;
	auto& d = *impl;
	using CreateTimerFn = HANDLE (WINAPI *)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
	auto create = reinterpret_cast<CreateTimerFn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
	                                                             "CreateWaitableTimerExW"));
	// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Windows 10 1803+).
	if(create) d.timer = create(nullptr, nullptr, 0x00000002, TIMER_MODIFY_STATE | SYNCHRONIZE);
	if(!d.timer) return false; // do not disguise coarse fallback as a precise clock
	d.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if(!d.stop) { CloseHandle(d.timer); d.timer = nullptr; return false; }
	static std::atomic<uintptr_t> next_generation { 0 };
	d.generation = ++next_generation;
	const auto period = std::chrono::nanoseconds(1000000000LL / pulse_hz);
	const UINT message = Message();
	const uintptr_t generation = d.generation;
	d.running.store(true);
	try {
		d.thread = std::thread([&d, hwnd, period, message, generation] {
			using Clock = std::chrono::steady_clock;
			auto due = Clock::now() + period;
			HANDLE handles[] = { d.stop, d.timer };
			for(;;) {
				auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(due - Clock::now()).count();
				LARGE_INTEGER delay;
				delay.QuadPart = -max<int64>(1, (remaining + 99) / 100);
				if(!SetWaitableTimer(d.timer, &delay, 0, nullptr, nullptr, FALSE)) break;
				if(WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) break;
				if(!d.pending.exchange(true) && !PostMessageW(hwnd, message, (WPARAM)generation, 0))
					d.pending.store(false);
				due += period;
				if(due <= Clock::now()) due = Clock::now() + period; // skip missed pulses
			}
			d.running.store(false);
		});
	}
	catch(...) {
		d.running.store(false);
		CloseHandle(d.stop); CloseHandle(d.timer); d.stop = d.timer = nullptr;
		return false;
	}
	d.active = true;
	return true;
}
void Win32GpuFrameClock::Stop()
{
	auto& d = *impl;
	if(d.stop) SetEvent(d.stop);
	if(d.thread.joinable()) d.thread.join();
	if(d.timer) { CancelWaitableTimer(d.timer); CloseHandle(d.timer); d.timer = nullptr; }
	if(d.stop) { CloseHandle(d.stop); d.stop = nullptr; }
	d.active = false;
	d.pending.store(false);
}
bool Win32GpuFrameClock::Consume(WPARAM generation)
{
	auto& d = *impl;
	if(!d.active || (uintptr_t)generation != d.generation) return false;
	return d.pending.exchange(false);
}

bool BuildWin32GpuNativeWindowDesc(HWND hwnd, GpuNativeWindowDesc& out, String& error)
{
	out = GpuNativeWindowDesc();
	// Keep the native handle opaque; callers only get a typed descriptor, never
	// a logged pointer value.
	if(!hwnd || !IsWindow(hwnd)) {
		error = "invalid native handle";
		return false;
	}
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if(pid != GetCurrentProcessId()) {
		error = "native window belongs to another process";
		return false;
	}
	out.kind = GpuNativeWindowKind::Win32;
	out.handle = (uintptr_t)hwnd;
	return true;
}

GpuResult GetGpuNativeWindowDesc(const TopWindow& window, GpuNativeWindowDesc& out, String& error)
{
	out = GpuNativeWindowDesc();
	if(!window.IsOpen()) {
		error = "window not open";
		return GpuResult::InvalidState;
	}
	if(!BuildWin32GpuNativeWindowDesc(window.GetHWND(), out, error))
		return GpuResult::InvalidHandle;
	return GpuResult::Ok;
}

}
