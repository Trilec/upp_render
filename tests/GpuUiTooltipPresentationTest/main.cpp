#include <GpuRender/GpuRender.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <Ui/UiDropdown.h>

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

static String DiagText(const VulkanTestHooks::VulkanRuntimeDeviceDiagnostics& d)
{
	String out;
	out << "runtime=" << d.runtime_live_count
	    << " instance=" << d.instance_live_count
	    << " debug=" << d.debug_messenger_live_count
	    << " surface=" << d.surface_live_count
	    << " device=" << d.device_live_count
	    << " swapchain=" << d.swapchain_live_count
	    << " surface_create=" << d.surface_create_count
	    << " swapchain_create=" << d.swapchain_create_count;
	return out;
}

static bool IsFinalOwnershipZero(const VulkanTestHooks::VulkanRuntimeDeviceDiagnostics& d)
{
	return d.runtime_live_count == 0 &&
	       d.instance_live_count == 0 &&
	       d.debug_messenger_live_count == 0 &&
	       d.surface_live_count == 0 &&
	       d.device_live_count == 0 &&
	       d.swapchain_live_count == 0;
}

static Vector<Ctrl *> FindOwnedTops(Ctrl& root)
{
	Vector<Ctrl *> out;
	Vector<Ctrl *> top = Ctrl::GetTopCtrls();
	for(Ctrl *q : top) {
		if(!q || q == &root || !q->IsOpen())
			continue;
		Ctrl *owner = q->GetOwner();
		for(int depth = 0; owner && depth < 16; ++depth) {
			if(owner == &root) {
				out.Add(q);
				break;
			}
			owner = owner->GetOwner();
		}
	}
	return out;
}

class OwnerWindow : public GpuTopWindow {
public:
	OwnerWindow()
	{
		Title("GpuUiTooltipPresentationTest");
		SetRect(160, 160, 720, 420);
		SetValidation(true);
		label.SetLabel("Real U++ tooltip over a real upp_Ui control");
		mode.Add("Balanced");
		mode.Add("Quiet");
		mode.Add("Responsive");
		mode.Select(0);
		mode.Tip("Real U++ tooltip attached to UiDropdown");

		Add(label.LeftPos(24, 420).TopPos(24, 28));
		Add(mode.LeftPos(24, 260).TopPos(120, 30));
		Add(blank.LeftPos(24, 260).TopPos(220, 30));
	}

	UiDropdown& GetDropdown() { return mode; }
	Label& GetBlankTarget() { return blank; }

private:
	Label label;
	UiDropdown mode;
	Label blank;
};

} // namespace

GUI_APP_MAIN
{
	VulkanTestHooks::ClearVulkanRuntimeDeviceDiagnostics();
	bool ok = true;

	OwnerWindow win;
	win.Open();
	ok &= Check(win.IsOpen(), "GPU owner should open");
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

	Cout() << "ROOT: " << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;

	for(int cycle = 0; cycle < 4 && ok; ++cycle) {
#ifdef PLATFORM_WIN32
		// The real tooltip show path requires the owner top window to be
		// foreground (CtrlLib ToolTip.cpp); reassert it before every hover.
		::SetForegroundWindow(win.GetHWND());
#endif
		ok &= Check(PumpUntil([&] { return win.IsForeground(); }, 300),
		            "root should be foreground before tooltip hover");
		Rect ctrl = win.GetDropdown().GetScreenRect();
		::SetCursorPos((ctrl.left + ctrl.right) / 2, (ctrl.top + ctrl.bottom) / 2);

		Ctrl *tooltip = nullptr;
		bool shown = PumpUntil([&] {
			Vector<Ctrl *> tops = FindOwnedTops(win);
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			if(tops.GetCount() == 1 && tops[0]->IsVisible() &&
			   d.surface_live_count == 2 && d.swapchain_live_count == 2 &&
			   d.device_live_count == 1) {
				tooltip = tops[0];
				return true;
			}
			return false;
		}, 1500);
		if(!shown) {
			Vector<Ctrl *> tops = FindOwnedTops(win);
			Cout() << "TOOLTIP_TIMEOUT cycle=" << cycle
			       << " owned_tops=" << tops.GetCount()
			       << " foreground=" << win.IsForeground()
			       << " mouse_ctrl_tip=" << (Ctrl::GetMouseCtrl() ? Ctrl::GetMouseCtrl()->GetTip() : String("<null>"))
			       << " " << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;
		}
		ok &= Check(shown, "hovering the upp_Ui control should show the real tooltip as a second GPU surface/swapchain");

		auto shown_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		Cout() << "TOOLTIP_SHOW cycle=" << cycle
		       << " popup=" << (tooltip != nullptr);
		if(tooltip) {
			Rect tr = tooltip->GetScreenRect();
			Cout() << " rect=" << tr;
			ok &= Check(tr.Width() > 10 && tr.Height() > 5,
			            "tooltip should have a non-degenerate rectangle");
		}
		Cout() << " " << DiagText(shown_diag) << EOL;

		Rect blank = win.GetBlankTarget().GetScreenRect();
		::SetCursorPos((blank.left + blank.right) / 2, (blank.top + blank.bottom) / 2);

		ok &= Check(PumpUntil([&] {
			Vector<Ctrl *> tops = FindOwnedTops(win);
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return tops.IsEmpty() &&
			       d.surface_live_count == 1 && d.swapchain_live_count == 1 &&
			       d.device_live_count == 1;
		}, 1500), "moving off the control should close the tooltip and return to root ownership");

		Cout() << "TOOLTIP_HIDE cycle=" << cycle << " "
		       << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;
	}

	win.Close();
	ok &= Check(PumpUntil([&] {
		return IsFinalOwnershipZero(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics());
	}), "final close should return all Vulkan ownership to zero");

	Cout() << "FINAL: " << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;
	if(ok) {
		Cout() << "GpuUiTooltipPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
