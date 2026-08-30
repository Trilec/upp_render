#include <GpuRender/GpuRender.h>
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include <Ui/UiMenu.h>

using namespace Upp;

namespace {

static bool Check(bool condition, const char *message)
{
	if(!condition)
		Cout() << "FAIL: " << message << EOL;
	return condition;
}

static bool PumpUntil(Function<bool ()> condition, int loops = 1200)
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
		Title("GpuUiMenuPopupPresentationTest");
		SetRect(120, 120, 720, 420);
		SetValidation(true);
		label.SetLabel("upp_Ui UiMenu GPU popup probe");
		Add(label.LeftPos(24, 360).TopPos(24, 28));

		UiMenuModel& model = menu.Model();
		UiMenuNodeRef root = model.Root();
		model.AddChild(root, UiMenuItem("Open", 10));
		UiMenuNodeRef more = model.AddChild(root, UiMenuItem("More"));
		model.AddChild(more, UiMenuItem("Inspect", 20));
		model.AddChild(more, UiMenuItem("Export", 30));
		menu.WhenAction = [=](UiMenuNodeRef, const UiMenuItem& item) {
			action_count++;
			last_action = item.text;
		};
	}

	UiMenu& GetMenu() { return menu; }
	int GetActionCount() const { return action_count; }
	String GetLastAction() const { return last_action; }

private:
	Label label;
	UiMenu menu;
	int action_count = 0;
	String last_action;
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
		UiMenu& menu = win.GetMenu();
		auto before = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		Point origin(win.GetScreenRect().left + 80, win.GetScreenRect().top + 120);
		menu.PopUp(&win, origin);

		Vector<Ctrl *> root_tops;
		ok &= Check(PumpUntil([&] {
			root_tops = FindOwnedTops(win);
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return menu.IsMenuOpen() && root_tops.GetCount() == 1 &&
			       root_tops[0]->IsVisible() &&
			       d.surface_live_count == 2 && d.swapchain_live_count == 2 &&
			       d.device_live_count == 1;
		}), "UiMenu root popup should use a second GPU surface/swapchain sharing the root device");

		auto root_open = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		Cout() << "UI_MENU_ROOT cycle=" << cycle
		       << " menu_open=" << menu.IsMenuOpen()
		       << " top_count=" << root_tops.GetCount();
		if(root_tops.GetCount())
			Cout() << " rect=" << root_tops[0]->GetScreenRect();
		Cout() << " " << DiagText(root_open)
		       << " create_delta_surface=" << (root_open.surface_create_count - before.surface_create_count)
		       << " create_delta_swapchain=" << (root_open.swapchain_create_count - before.swapchain_create_count)
		       << EOL;

		if(root_tops.IsEmpty()) {
			ok = false;
			break;
		}

		Rect rr = root_tops[0]->GetScreenRect();
		ok &= Check(rr.Width() > 1 && rr.Height() > 1,
		            "UiMenu root popup should have a non-degenerate rectangle");

		const UiMenu::Style& style = menu.GetStyle();
		Point submenu_row(style.popup_padding + DPI(18),
		                  style.popup_padding + style.row_height + style.row_height / 2);
		root_tops[0]->LeftDown(submenu_row, 0);

		Vector<Ctrl *> submenu_tops;
		ok &= Check(PumpUntil([&] {
			submenu_tops = FindOwnedTops(win);
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return menu.IsMenuOpen() && submenu_tops.GetCount() == 2 &&
			       d.surface_live_count == 3 && d.swapchain_live_count == 3 &&
			       d.device_live_count == 1;
		}), "UiMenu submenu should add a third GPU surface/swapchain while sharing the same device");

		auto submenu_open = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		Cout() << "UI_MENU_SUBMENU cycle=" << cycle
		       << " top_count=" << submenu_tops.GetCount()
		       << " " << DiagText(submenu_open) << EOL;

		if(submenu_tops.GetCount() != 2) {
			ok = false;
			break;
		}

		Ctrl *submenu = submenu_tops[0] == root_tops[0] ? submenu_tops[1] : submenu_tops[0];
		Rect sr = submenu->GetScreenRect();
		ok &= Check(sr.Width() > 1 && sr.Height() > 1,
		            "UiMenu submenu should have a non-degenerate rectangle");

		int actions_before = win.GetActionCount();
		Point first_row(style.popup_padding + DPI(18),
		                style.popup_padding + style.row_height / 2);
		submenu->LeftDown(first_row, 0);

		ok &= Check(PumpUntil([&] {
			auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
			return !menu.IsMenuOpen() && win.GetActionCount() == actions_before + 1 &&
			       d.surface_live_count == 1 && d.swapchain_live_count == 1 &&
			       d.device_live_count == 1;
		}), "UiMenu leaf activation should close all popup levels and return to root ownership");
		ok &= Check(win.GetLastAction() == "Inspect", "UiMenu submenu action should be Inspect");
	}

	win.Close();
	ok &= Check(PumpUntil([&] {
		return IsFinalOwnershipZero(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics());
	}), "final Vulkan ownership should return to zero");

	Cout() << "FINAL: " << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;
	if(ok)
		Cout() << "GpuUiMenuPopupPresentationTest passed" << EOL;
	SetExitCode(ok ? 0 : 1);
}
