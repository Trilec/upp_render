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

static Ctrl *FindOwnedTop(Ctrl& root)
{
	Vector<Ctrl *> top = Ctrl::GetTopCtrls();
	for(Ctrl *q : top) {
		if(!q || q == &root || !q->IsOpen())
			continue;
		Ctrl *owner = q->GetOwner();
		for(int depth = 0; owner && depth < 16; ++depth) {
			if(owner == &root)
				return q;
			owner = owner->GetOwner();
		}
	}
	return nullptr;
}

class OwnerWindow : public GpuTopWindow {
public:
	OwnerWindow()
	{
		Title("GpuUiDropdownPopupPresentationTest");
		SetRect(120, 120, 720, 420);
		SetValidation(true);

		label.SetLabel("upp_Ui UiDropdown GPU popup probe");
		mode.Add("Balanced");
		mode.Add("Quiet");
		mode.Add("Responsive");
		mode.Select(0);

		Add(label.LeftPos(24, 360).TopPos(24, 28));
		Add(mode.LeftPos(24, 260).TopPos(72, 30));
	}

	UiDropdown& GetDropdown() { return mode; }

private:
	Label label;
	UiDropdown mode;
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
		UiDropdown& dropdown = win.GetDropdown();
		Ctrl *popup = nullptr;
		auto before = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();

		dropdown.OpenPopup();
		ok &= Check(PumpUntil([&] {
			popup = FindOwnedTop(win);
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return dropdown.IsPopupOpen() && popup && popup->IsOpen() && popup->IsVisible() &&
			       d.surface_live_count == 2 && d.swapchain_live_count == 2 &&
			       d.device_live_count == 1;
		}), "UiDropdown popup should open on a second GPU surface/swapchain sharing the root device");

		auto opened = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		Cout() << "UI_DROPDOWN cycle=" << cycle
		       << " popup_open=" << dropdown.IsPopupOpen()
		       << " popup_found=" << (popup != nullptr);
		if(popup)
			Cout() << " open=" << popup->IsOpen()
			       << " visible=" << popup->IsVisible()
			       << " rect=" << popup->GetScreenRect();
		Cout() << " " << DiagText(opened)
		       << " create_delta_surface=" << (opened.surface_create_count - before.surface_create_count)
		       << " create_delta_swapchain=" << (opened.swapchain_create_count - before.swapchain_create_count)
		       << EOL;

		if(popup) {
			Rect pr = popup->GetScreenRect();
			ok &= Check(pr.Width() > 1 && pr.Height() > 1,
			            "UiDropdown popup should have a non-degenerate rectangle");
		}

		dropdown.ClosePopup();
		ok &= Check(PumpUntil([&] {
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return !dropdown.IsPopupOpen() && FindOwnedTop(win) == nullptr &&
			       d.surface_live_count == 1 && d.swapchain_live_count == 1 &&
			       d.device_live_count == 1;
		}), "UiDropdown popup close should return presentation ownership to root only");

		dropdown.Select((cycle + 1) % dropdown.GetCount());
		ok &= Check(dropdown.GetSelection() == (cycle + 1) % dropdown.GetCount(),
		            "UiDropdown collapsed selection should remain functional after popup close");
	}

	win.Close();
	ok &= Check(PumpUntil([&] {
		return IsFinalOwnershipZero(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics());
	}), "final close should return all Vulkan ownership to zero");

	if(!IsFinalOwnershipZero(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()))
		Cout() << "FINAL: " << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;

	if(ok) {
		Cout() << "GpuUiDropdownPopupPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
