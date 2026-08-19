#pragma once

#include <CtrlLib/CtrlLib.h>
#include <RenderPresentation/RenderPresentation.h>

namespace Upp {

// Top-level U++ window whose client area is presented through one GPU surface.
// U++ remains the window/input/layout/state/theme authority. By default the
// resolved control tree is recorded into the neutral display list; subclasses
// may still override BuildGpuFrame() for deliberately custom frame sources.
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
	// implementation records this window's resolved U++ control tree through
	// RecordCtrlDisplayList(). Overrides can provide custom neutral intent while
	// leaving native-window and presentation ownership unchanged.
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
