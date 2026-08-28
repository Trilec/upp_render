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

static void DumpTopCtrls(Ctrl& root, const char *label)
{
	Vector<Ctrl *> top = Ctrl::GetTopCtrls();
	Cout() << label << ": top_count=" << top.GetCount() << EOL;
	for(int i = 0; i < top.GetCount(); ++i) {
		Ctrl *q = top[i];
		if(!q) {
			Cout() << "  [" << i << "] null" << EOL;
			continue;
		}
		Cout() << "  [" << i << "] root=" << (q == &root)
		       << " open=" << q->IsOpen()
		       << " visible=" << q->IsVisible()
		       << " rect=" << q->GetScreenRect()
		       << " owner_is_root=" << (q->GetOwner() == &root)
		       << EOL;
	}
}

class PaintProbe : public Ctrl {
public:
	int GetPaintCount() const { return paint_count; }

	void Paint(Draw& w) override
	{
		++paint_count;
		w.DrawRect(GetSize(), Color(31, 42, 58));
		w.DrawText(14, 14, "Activated transient probe", SansSerif(13).Bold(), Color(240, 244, 250));
	}

private:
	int paint_count = 0;
};

class OwnerWindow : public GpuTopWindow {
public:
	OwnerWindow()
	{
		Title("GpuDropListPopupPresentationTest");
		SetRect(120, 120, 720, 420);
		SetValidation(true);

		label.SetLabel("Real DropList programmatic popup probe");
		mode.Add("Balanced");
		mode.Add("Quiet");
		mode.Add("Responsive");
		mode.SetIndex(0);
		mode.WhenDrop = [this] { ++drop_count; };

		Add(label.LeftPos(24, 360).TopPos(24, 28));
		Add(mode.LeftPos(24, 260).TopPos(72, 30));
	}

	DropList& GetDropList() { return mode; }
	int GetDropCount() const { return drop_count; }

private:
	Label label;
	DropList mode;
	int drop_count = 0;
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

	auto root_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Cout() << "ROOT: " << DiagText(root_diag) << EOL;

	PaintProbe synthetic;
	Rect wr = win.GetScreenRect();
	synthetic.SetRect(RectC(wr.left + 90, wr.top + 140, 330, 100));
	synthetic.PopUp(&win, true, true, true, false);
	ok &= Check(synthetic.IsOpen(), "activated synthetic popup should open");
	ok &= Check(PumpUntil([&] {
		auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		return synthetic.GetPaintCount() > 0 &&
		       d.surface_live_count == 2 && d.swapchain_live_count == 2 &&
		       d.device_live_count == 1;
	}), "activated synthetic popup should receive a second GPU surface/swapchain on the shared device");

	auto synthetic_diag = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Cout() << "ACTIVATED_SYNTHETIC: open=" << synthetic.IsOpen()
	       << " visible=" << synthetic.IsVisible()
	       << " rect=" << synthetic.GetScreenRect()
	       << " paint_count=" << synthetic.GetPaintCount()
	       << " " << DiagText(synthetic_diag) << EOL;

	synthetic.Close();
	ok &= Check(PumpUntil([&] {
		auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		return !synthetic.IsOpen() &&
		       d.surface_live_count == 1 && d.swapchain_live_count == 1 &&
		       d.device_live_count == 1;
	}), "activated synthetic popup close should leave only root presentation alive");

	win.SetForeground();
	Ctrl::ProcessEvents();

	auto before_drop = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	int drop_before = win.GetDropCount();
	win.GetDropList().Drop();
	bool when_drop_fired = win.GetDropCount() == drop_before + 1;
	Ctrl *drop_popup = FindOwnedTop(win);
	if(!drop_popup) {
		PumpUntil([&] {
			drop_popup = FindOwnedTop(win);
			return drop_popup != nullptr;
		}, 100);
	}

	auto after_drop = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
	Cout() << "REAL_DROPLIST: when_drop=" << when_drop_fired
	       << " popup_found=" << (drop_popup != nullptr);
	if(drop_popup)
		Cout() << " open=" << drop_popup->IsOpen()
		       << " visible=" << drop_popup->IsVisible()
		       << " rect=" << drop_popup->GetScreenRect();
	Cout() << " " << DiagText(after_drop) << EOL;
	Cout() << "REAL_DROPLIST_CREATE_DELTA: surface="
	       << (after_drop.surface_create_count - before_drop.surface_create_count)
	       << " swapchain="
	       << (after_drop.swapchain_create_count - before_drop.swapchain_create_count)
	       << EOL;
	DumpTopCtrls(win, "TOPS_AFTER_REAL_DROPLIST");

	ok &= Check(when_drop_fired, "programmatic DropList::Drop should invoke WhenDrop");
	ok &= Check(drop_popup != nullptr, "programmatic DropList::Drop should leave an owned top-level popup open");
	if(drop_popup) {
		Rect pr = drop_popup->GetScreenRect();
		ok &= Check(drop_popup->IsOpen(), "real DropList popup should be open");
		ok &= Check(drop_popup->IsVisible(), "real DropList popup should be visible");
		ok &= Check(pr.Width() > 1 && pr.Height() > 1,
		            "real DropList popup should reach a non-degenerate final rectangle");
	}
	ok &= Check(after_drop.surface_live_count == 2,
	            "real DropList popup should add one live Vulkan surface");
	ok &= Check(after_drop.swapchain_live_count == 2,
	            "real DropList popup should add one live Vulkan swapchain");
	ok &= Check(after_drop.device_live_count == 1,
	            "real DropList popup should share the root logical device");

	if(drop_popup && drop_popup->IsOpen())
		drop_popup->Close();

	ok &= Check(PumpUntil([&] {
		auto d = VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics();
		return d.surface_live_count == 1 && d.swapchain_live_count == 1 &&
		       d.device_live_count == 1;
	}), "closing real DropList popup should return presentation ownership to root only");

	win.Close();
	bool final_zero = PumpUntil([&] {
		return IsFinalOwnershipZero(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics());
	});
	if(!final_zero)
		Cout() << "FINAL: " << DiagText(VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics()) << EOL;
	ok &= Check(final_zero, "final close should return all Vulkan ownership to zero");

	if(ok) {
		Cout() << "GpuDropListPopupPresentationTest passed" << EOL;
		return;
	}
	SetExitCode(1);
}
