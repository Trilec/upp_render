#include <GpuRender/GpuRender.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>

using namespace Upp;

namespace {

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static bool PumpUntil(Function<bool ()> condition, int loops = 1000)
{
	for(int i = 0; i < loops; ++i) {
		Ctrl::ProcessEvents();
		if(condition())
			return true;
		Ctrl::GuiSleep(2);
	}
	return condition();
}

class PopupProbe : public Ctrl {
public:
	int GetPaintCount() const { return paint_count; }

	void Paint(Draw& w) override
	{
		++paint_count;
		w.DrawRect(GetSize(), Color(34, 43, 61));
		w.DrawRect(10, 10, max(1, GetSize().cx - 20), max(1, GetSize().cy - 20), Color(66, 87, 122));
		w.DrawText(18, 18, "Transient U++ popup", SansSerif(14).Bold(), Color(238, 244, 252));
		w.DrawText(18, 44, "Recorded and presented through shared GPU context", SansSerif(11), Color(182, 197, 219));
	}

private:
	int paint_count = 0;
};

class OwnerWindow : public GpuTopWindow {
public:
	OwnerWindow()
	{
		Title("GpuTransientPopupPresentationTest");
		SetRect(120, 120, 720, 420);
		SetValidation(true);
		label.SetLabel("GpuTopWindow owner");
		Add(label.LeftPos(24, 260).TopPos(24, 28));
	}

private:
	Label label;
};

static bool IsFinalOwnershipZero(const VulkanTestHooks::VulkanRuntimeDeviceDiagnostics& d)
{
	return d.runtime_live_count == 0 &&
	       d.instance_live_count == 0 &&
	       d.debug_messenger_live_count == 0 &&
	       d.surface_live_count == 0 &&
	       d.device_live_count == 0 &&
	       d.swapchain_live_count == 0;
}

} // namespace

GUI_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	bool ok = true;

	OwnerWindow win;
	win.Open();
	ok &= Check(win.IsOpen(), "GPU owner window should open");
	if(!win.IsOpen()) {
		SetExitCode(1);
		return;
	}

	ok &= Check(PumpUntil([&] {
		auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		return win.IsGpuReady() && win.GetGpuError().IsEmpty() &&
		       d.surface_live_count == 1 && d.swapchain_live_count == 1 &&
		       d.device_live_count == 1;
	}), "GPU owner should reach one live surface/swapchain/device");

	for(int cycle = 0; cycle < 4; ++cycle) {
		PopupProbe popup;
		Rect owner_rect = win.GetScreenRect();
		popup.SetRect(RectC(owner_rect.left + 80 + cycle * 8,
		                    owner_rect.top + 100 + cycle * 6,
		                    360, 110));
		popup.PopUp(&win, true, false, false, false);
		ok &= Check(popup.IsOpen(), "owned transient popup should open");

		ok &= Check(PumpUntil([&] {
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return popup.GetPaintCount() > 0 &&
			       d.surface_live_count == 2 && d.swapchain_live_count == 2 &&
			       d.device_live_count == 1;
		}), "owned popup should add one GPU surface/swapchain while sharing one device");

		auto active = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		ok &= Check(active.surface_live_count == 2, "root plus popup should own two live Vulkan surfaces");
		ok &= Check(active.swapchain_live_count == 2, "root plus popup should own two live Vulkan swapchains");
		ok &= Check(active.device_live_count == 1, "root plus popup should share one live logical device");

		int paint_before = popup.GetPaintCount();
		popup.Refresh();
		ok &= Check(PumpUntil([&] { return popup.GetPaintCount() > paint_before; }, 300),
		            "popup refresh should re-record transient U++ painting");

		popup.Close();
		ok &= Check(PumpUntil([&] {
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return !popup.IsOpen() && d.surface_live_count == 1 &&
			       d.swapchain_live_count == 1 && d.device_live_count == 1;
		}), "closing popup should release only its surface/swapchain and keep root device alive");
	}

	ok &= Check(win.IsGpuReady() && win.GetGpuError().IsEmpty(),
	            "root GPU window should remain ready after repeated popup lifecycle");

	win.Close();
	ok &= Check(PumpUntil([&] {
		return IsFinalOwnershipZero(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics());
	}), "closing root should return all Vulkan ownership to zero");

	if(ok) {
		Cout() << "GpuTransientPopupPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
