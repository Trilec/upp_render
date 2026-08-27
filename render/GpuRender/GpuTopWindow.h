#pragma once

#include <CtrlLib/CtrlLib.h>
#include "RenderPresentation.h"

namespace Upp {

class GpuTopWindow : public TopWindow {
public:
	GpuTopWindow();
	~GpuTopWindow() override;
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
	struct Impl;
	One<Impl> impl;
};

}
