// Copyright (c) 2026 upp_render contributors. Apache-2.0; see repository LICENSE.
#ifndef _Ui3DViewUi_Ui3DViewport_h_
#define _Ui3DViewUi_Ui3DViewport_h_

#include <Ui3DView/Ui3DView.h>
#include <Ui/UiPanel.h>
#include <Ui/UiMenu.h>

namespace Upp {

// Cumulative GUI-thread diagnostics for actual native/tile paint and geometry
// operations. These are operation counts, not GPU timing or inferred blit counts.
struct Ui3DViewportChromeStats {
	uint64 native_paints = 0; // Win32 WM_PAINT calls with a valid paint DC.
	uint64 tile_paints = 0;   // Bounded DrawCtrl calls made by that native host.
	uint64 region_updates = 0;
	uint64 z_order_updates = 0;
	uint64 axis_paints = 0;
	uint64 annotation_paints = 0;
	uint64 max_tile_pixels = 0; // Largest such tile rectangle, in physical pixels.
};

// Optional standard native chrome. Scene, selection, camera catalogue, active
// pane and workspace layout remain application-owned. GUI thread only.
// The host retains View().WhenNavigation/WhenSelectionRequest; this wrapper
// does not replace them. Call RefreshChrome after host-driven view changes.
// Camera commands are silent programmatic base-view calls followed by one
// WhenCameraCommand notification; display/overlay changes use WhenViewOptions.
class Ui3DViewport : public UiPanel {
public:
	typedef Ui3DViewport CLASSNAME;
	Ui3DViewport();
	~Ui3DViewport() override;

	Ui3DView& View();
	const Ui3DView& View() const;
	Ui3DViewport& SetActive(bool active = true);
	Ui3DViewport& SetDark(bool dark = true);
	Ui3DViewport& ShowToolbar(bool show = true);
	Ui3DViewport& ShowOrientation(bool show = true);
	Ui3DViewport& ShowNavigation(bool show = true);
	Ui3DViewport& ShowAnnotations(bool show = true);
	Ui3DViewport& SetCompact(bool compact = true);
	Ui3DViewport& SetCameraLabel(const String& label);
	// Explicit lower bounds preserve the standard UiPanel setter vocabulary.
	// Zero clears the requested bound; the intrinsic 175x120 DPI-scaled floor remains.
	Ui3DViewport& SetSizeMin(Size size);
	Ui3DViewport& SetSizeMin(int cx, int cy) { return SetSizeMin(Size(cx, cy)); }
	Ui3DViewport& SetSizeFixed(Size size) { return SetSizeMin(size); }
	Ui3DViewport& SetSizeFixed(int cx, int cy) { return SetSizeMin(Size(cx, cy)); }
	bool IsToolbarShown() const;
	bool IsOrientationShown() const;
	bool IsNavigationShown() const;
	bool AreAnnotationsShown() const;
	bool IsCompact() const;
	bool IsActive() const;
	bool IsDark() const;
	// Returns a value snapshot; querying does not refresh, render or reset counts.
	Ui3DViewportChromeStats GetChromeStats() const;
	void RefreshChrome();

	Event<> WhenActivate;
	Event<> WhenMaximize;
	Event<> WhenCameraMenu;
	Event<> WhenCameraCommand;
	Event<> WhenViewOptions;
	// Add catalogue commands to this menu using its ordinary UiMenuModel API.
	// The wrapper owns this menu; callbacks may not retain a borrowed reference.
	Event<UiMenu&> PopulateCameraMenu;

	void Layout() override;
	// A fixed intrinsic floor: the current viewport rectangle must never feed
	// back into the parent's next minimum-size measurement.
	Size GetMinSize() const override;
	void Paint(Draw& draw) override;
	void ChildGotFocus() override;
	void LeftDown(Point point, dword flags) override;
protected:
	void State(int reason) override;
private:
	struct Impl;
	One<Impl> impl;
};

}
#endif
