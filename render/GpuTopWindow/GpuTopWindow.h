#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderPresentation/RenderPresentation.h>

namespace Upp {

// Top-level U++ window whose client area can be presented through one GPU
// surface. U++ remains the window/input/layout authority; subclasses provide
// resolved neutral drawing intent through BuildGpuFrame().
//
// This is intentionally different from GpuCtrl: no native child host is
// created. The presenter binds directly to the TopWindow's own native window.
class GpuTopWindow : public TopWindow {
public:
	GpuTopWindow();
	~GpuTopWindow() override;

	bool IsGpuReady() const;
	String GetGpuError() const;
	void RequestGpuRefresh();
	GpuTopWindow& RetryGpuInit();

	GpuTopWindow& SetBackend(GpuBackendKind kind);
	GpuTopWindow& SetValidation(bool validation = true);

protected:
	// Build one immutable frame for the current root client size. The default
	// implementation emits an empty list and clears to a neutral background.
	virtual bool BuildGpuFrame(Size size, UiDisplayList& list,
	                           Rgba8& background, String& error);

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
