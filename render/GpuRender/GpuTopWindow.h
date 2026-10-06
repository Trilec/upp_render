#pragma once

#include <CtrlLib/CtrlLib.h>
#include "RenderPresentation.h"

namespace Upp {

class GpuTopWindow : public TopWindow {
public:
	GpuTopWindow();
	~GpuTopWindow() override;
	void Close() override;
	// Select before opening; GUI recording, one replaceable pending frame.
	GpuTopWindow& SetAsyncPresentation(bool enabled = true);
	// Select before opening. Failed GPU frames remain failed until explicit retry;
	// root client painting never enters the ordinary software fallback path.
	GpuTopWindow& SetRequireGpu(bool required = true);
	bool IsGpuRequired() const;
	uint64 GetSoftwareFallbackCount() const;
	GpuPresentationStats GetGpuStats() const;
	// Opt-in precise Windows host wakes for existing UI/Animation timers.
	// Select before opening; one coalesced message, joined before window teardown.
	GpuTopWindow& SetFrameClock(bool enabled = true);
	bool IsFrameClockActive() const;
	bool IsGpuReady() const;
	String GetGpuError() const;
	GpuBackendKind GetBackend() const;
	bool IsValidationRequested() const;
	void RequestGpuRefresh();
	GpuTopWindow& RetryGpuInit();
	GpuTopWindow& SetBackend(GpuBackendKind kind);
	GpuTopWindow& SetValidation(bool validation = true);
protected:
	virtual bool BuildGpuFrame(Size size, UiDisplayList& list, Rgba8& background, String& error);
#ifdef PLATFORM_WIN32
	void NcCreate(HWND hwnd) override;
	void PreDestroy() override;
	LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
#endif
private:
	friend class GpuTransientWindowHost;
	void ReportGpuFailure(const String& error);
	struct Impl;
	One<Impl> impl;
};

}
