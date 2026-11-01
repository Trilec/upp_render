#include "GpuControls.h"
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include "GpuCapture.h"
#include <plugin/png/png.h>
#include <chrono>

namespace Upp {
namespace {
// Qualification reads the public control tree and minimum-size contract.
// No private picker coordinates or production styling are copied into the test.
bool PickerFieldBounds(Ctrl& root, String& report, int page = UiColorPicker::PAGE_IMAGE)
{
	bool ok = true;
	int drops = 0, toggles = 0, edits = 0;
	bool active_page_accent = false;
	const char *pages[] = { "Color", "Palettes", "Generator", "Image" };
	auto visit = [&](auto&& self, Ctrl& control) -> void {
		if(!control.IsVisible() || control.GetSize().IsEmpty()) return;
		if(auto *button = dynamic_cast<UiButton *>(&control))
			if(button->GetText() == pages[page])
				active_page_accent = button->GetStyle().underline &&
				    button->GetStyle().palette.ink[ST_NORMAL] == UiTheme::GetOliveTokens(UiTheme::GetMode()).accent;
		const char *kind = nullptr;
		bool width_required = false;
		if(dynamic_cast<UiDropdown *>(&control)) { ++drops; kind = "dropdown"; }
		else if(dynamic_cast<UiToggle *>(&control)) { ++toggles; kind = "toggle"; width_required = true; }
		else if(dynamic_cast<UiBaseEdit *>(&control)) { ++edits; kind = "edit"; }
		if(kind) {
			const Size size = control.GetSize(), need = control.GetMinSize();
			if(size.cy + 1 < need.cy || (width_required && size.cx + 1 < need.cx)) {
				ok = false; report << "clipped_picker_field kind=" << kind << " size=" << size << " minimum=" << need << "\n";
			}
		}
		for(Ctrl *child = control.GetFirstChild(); child; child = child->GetNext()) self(self, *child);
	};
	visit(visit, root);
	report << "picker_page=" << page << " picker_field_bounds=" << (ok ? "PASS" : "FAIL")
	       << " dropdowns=" << drops << " toggles=" << toggles << " edits=" << edits << "\n";
	report << "picker_active_page_accent=" << (active_page_accent ? "PASS" : "FAIL") << "\n";
	return ok && drops >= 1 && edits >= 2 && active_page_accent;
}

double ControlsNow() {
	return std::chrono::duration<double,std::milli>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
// Use the browser's existing focused-test seam; no production accessors or
// private-layout copies are added just to drive renderer qualification.
struct UiFileBrowserTest {
	static bool SelectFixture(UiFileBrowser& b) {
		for(int i=0;i<b.model_->GetCount();++i)
			if((*b.model_)[i].name=="GpuUi-browser-preview.png") {
				b.SelectEntry(i); return b.GetSelection().IsValid();
			}
		return false;
	}
	static bool PreviewReady(const UiFileBrowser& b) {
		return b.preview_image_size_==Size(64,32) && !b.preview_image_.GetIcon().IsEmpty();
	}
	static void Accept(UiFileBrowser& b) { b.AcceptSelection(); }
	static bool FilterOpen(const UiFileBrowser& b) { return b.options_popup_.IsOpen(); }
	static void OpenNestedColour(UiFileBrowser& b) {
		b.OpenOptions(UiFileBrowser::OptionsPage::NewItem,b.new_button_);
		b.new_item_kind_.SelectByData(3); b.ConfigureCreation(); b.ChooseCreationColour();
	}
	static bool NestedColourOpen(const UiFileBrowser& b) {
		return b.options_popup_.IsOpen() && b.colour_popup_.IsOpen();
	}
	static bool CommitNestedColour(UiFileBrowser& b) {
		bool ok=b.colour_micro_.CommitHex("#369ACF");
		return ok && AsString(b.new_image_colour_.GetData())=="#369ACF";
	}
};

GpuControlsDialog::GpuControlsDialog(bool validation)
{
	SetRequireGpu().SetValidation(validation).SetAsyncPresentation();
	Title("Vulkan UI - roles, states and controls").Sizeable().Zoomable()
	     .SetRect(0,0,1360,980);
	heading.SetText("Live Vulkan controls - paired theme profiles, roles and states");
	status.SetText("Select a tab. Browser previews and palette data are prepared once and uploaded.");
	close.SetText("Close"); close.WhenAction=[=] { Close(); };
	browser.WhenAccept=[=](const UiFileBrowserSelection& selection) {
		++accepts; status.SetText("Selected: "+selection.path);
	};
	browser.WhenCancel=[=] { ++cancels; status.SetText("Browser cancelled."); };
	browser.WhenError=[=](const String& error) { status.SetText(error); };
	picker.SetSlotCount(4).SetSlot(0,Color(40,130,220),180,false);
	picker.WhenAction=[=] { ++actions; status.SetText("Full picker colour changed."); };
	micro.SetColor(Color(40,130,220));
	micro.WhenAction=[=] { ++actions; status.SetText("Micro picker applied "+micro.GetHex()); };
	tabs.Add(browser,"File browser"); tabs.Add(picker,"Colour picker"); micro_page.Add(micro); tabs.Add(micro_page,"Micro picker");
	text_help.SetText("Select, edit and compose text with your active input method. All application text is drawn in Vulkan.");
	WString sample("Arabic: "); for(int cp:{0x633,0x644,0x627,0x645}) sample.Cat(cp);
	sample.Cat("\nIndic: "); for(int cp:{0x915,0x94d,0x937,0x93f}) sample.Cat(cp);
	sample.Cat("\nMixed: A "); for(int cp:{0x5d0,0x5d1,0x5d2}) sample.Cat(cp); sample.Cat(" 42");
	sample.Cat("\nColour emoji: "); sample.Cat(0x1f600);
	text_entry.SetText(WString("Type or paste here")); text_document.SetText(ToUtf8(sample));
	text_page.Add(text_help); text_page.Add(text_entry); text_page.Add(text_document); tabs.Add(text_page,"Text & IME");
	tabs.Add(widgets,"Collections"); tabs.Add(roles,"Roles & states");
	ApplyTabStyle();

	micro.WhenLayoutChange=[=] { Layout(); };
	Add(heading); Add(tabs); Add(status); Add(close);
}
void GpuControlsDialog::ApplyTabStyle()
{
	// These are the app's page tabs, not the colour picker's internal navigation.
	// Resolve a fresh palette per mode, then apply the authored compact geometry.
	tabs.SetVisual(UITAB_UNDERLINE);
	UiTab::Style style = UiTheme::ResolveTab(UITAB_UNDERLINE);
	style.tab_extent = DPI(32);
	style.metrics.content_margin = Rect(0, 0, 0, 0);
	style.strip_inset = Rect(DPI(8), 0, DPI(8), 0);
	style.body_gap = DPI(6);
	tabs.SetCustomStyle(style);
}

GpuControlsDialog::~GpuControlsDialog()
{
	qualification_clock.Kill();
	if(theme_changed) { UiTheme::Set(original_theme); Ctrl::SwapDarkLight(); }
}
void GpuControlsDialog::Paint(Draw& draw)
{
	draw.DrawRect(GetSize(),UiTheme::GetOliveTokens(UiTheme::GetMode()).background);
}
void GpuControlsDialog::Layout()
{
	GpuTopWindow::Layout(); Size s=GetSize(); int pad=DPI(12);
	heading.SetRect(pad,pad,max(0,s.cx-2*pad),DPI(26));
	tabs.SetRect(pad,pad+DPI(32),max(0,s.cx-2*pad),max(0,s.cy-2*pad-DPI(76)));
	status.SetRect(pad,max(pad,s.cy-pad-DPI(30)),max(0,s.cx-2*pad-DPI(100)),DPI(28));
	Size ms=micro.GetMinSize();
	micro.SetRect(pad,pad,min(ms.cx,max(0,micro_page.GetSize().cx-2*pad)),ms.cy);
	Size ts=text_page.GetSize(); int content_width=max(0,ts.cx-2*pad);
	text_help.SetRect(pad,pad,content_width,DPI(38));
	text_entry.SetRect(pad,pad+DPI(44),content_width,DPI(36));
	text_document.SetRect(pad,pad+DPI(88),content_width,max(0,ts.cy-2*pad-DPI(88)));
	close.SetRect(max(pad,s.cx-pad-DPI(92)),max(pad,s.cy-pad-DPI(30)),DPI(92),DPI(28));
}
void GpuControlsDialog::StartQualification(String& output)
{
	report=&output; started=ControlsNow(); original_theme=UiTheme::GetContext();
	ImageBuffer fixture(64,32); Fill(~fixture,RGBA(Color(40,150,210)),fixture.GetLength());
	if(!PNGEncoder().SaveFile(GetExeDirFile("GpuUi-browser-preview.png"),Image(fixture))) {
		Finish(false,"cannot write test preview"); return;
	}
	browser.SetFolder(GetFileFolder(GetExeFilePath())); qualification_clock.Set(-100,[=] { Tick(); });
}
void GpuControlsDialog::Finish(bool ok,const String& detail)
{
	passed=ok && GetGpuError().IsEmpty() && !GetSoftwareFallbackCount();
	if(report) *report<<"extended_controls="<<(passed?"PASS":"FAIL")<<" "<<detail
	                 <<" picker_cases="<<picker_cases<<" actions="<<actions
	                 <<" accepts="<<accepts<<" cancels="<<cancels
	                 <<" fallback="<<GetSoftwareFallbackCount()<<" error="<<GetGpuError()<<"\n";
	if(theme_changed) {
		UiTheme::Set(original_theme); Ctrl::SwapDarkLight(); theme_changed=false;
	}
	qualification_clock.Kill(); Close();
}
void GpuControlsDialog::Tick()
{
	// Debug readback/PNG and unoptimized control recording need a larger test budget.
#ifdef _DEBUG
	const int timeout_ms=120000;
#else
	const int timeout_ms=30000;
#endif
	if(ControlsNow()-started>timeout_ms || !GetGpuError().IsEmpty() || GetSoftwareFallbackCount()) {
		Finish(false,"GPU/timeout at step "+AsString(step)); return;
	}
	const auto stats=GetGpuStats();
	if(!IsGpuReady() || stats.presented_frames<=frame) return;
	frame=stats.presented_frames;
	if(step==0) {
		if(browser.IsScanning()) return;
		if(!browser.GetEntryCount() || !UiFileBrowserTest::SelectFixture(browser)) {
			Finish(false,"browser populated scan/selection"); return;
		}
		++step; return;
	}
	if(step==1) {
		if(!UiFileBrowserTest::PreviewReady(browser)) { RequestGpuRefresh(); return; }
		UiFileBrowserTest::Accept(browser);
		browser.Key(K_CTRL_F,1); ++step; return;
	}
	if(step==2) {
		if(accepts!=1 || !UiFileBrowserTest::FilterOpen(browser)) { Finish(false,"browser preview/accept/filter"); return; }
		browser.Key(K_ESCAPE,1);
		UiFileBrowserTest::OpenNestedColour(browser); ++step; return;
	}
	if(step==3) {
		if(!UiFileBrowserTest::NestedColourOpen(browser) || !UiFileBrowserTest::CommitNestedColour(browser)) {
			Finish(false,"browser nested micro popup"); return;
		}
		browser.ShowOptions(false);
		browser.SetMode(UiFileBrowser::Mode::Advanced).SetViewSize(UiFileBrowser::ViewSize::Large);
		browser.SetSequenceGrouping(false); ++step; return;
	}
	if(step==4) {
		if(browser.IsScanning()) { RequestGpuRefresh(); return; }
		if(browser.GetMode()!=UiFileBrowser::Mode::Advanced ||
		   browser.GetViewSize()!=UiFileBrowser::ViewSize::Large || browser.IsSequenceGrouping()) {
			Finish(false,"browser mode/view/grouping"); return;
		}
		browser.Key(K_ESCAPE,1);
		if(cancels!=1) { Finish(false,"browser cancellation callback"); return; }
		tabs.SetActiveTab(1); ++step; return;
	}
	if(step>=5 && step<21) {
		int n=step-5;
		picker.SetPageMode((UiColorPicker::PageMode)(n/4))
		      .SetSpectrumMode((UiColorPicker::SpectrumMode)(n%4))
		      .SetChannelMode((UiColorPicker::ChannelMode)(n%UiColorPicker::CHANNEL_COUNT));
		picker.SetActiveSlot(n%4).SetSlot(n%4,Color(40+n*5,130,220),80+n*8,true);
		if(picker.GetSlotAlpha(n%4)!=80+n*8 || picker.GetPageMode()!=n/4) { Finish(false,"full picker value"); return; }
		String bounds; bool fits=PickerFieldBounds(picker,bounds,n/4); *report<<bounds;
		if(!fits) { Finish(false,"picker page "+AsString(n/4)+" field bounds"); return; }
		++picker_cases; ++step; RequestGpuRefresh(); return;
	}
	if(step==21) {
		if(actions!=16) { Finish(false,"full picker notifications"); return; }
		tabs.SetActiveTab(2); micro.SetRGBMode(true).SetPaletteMode(UiColorPickerMicro::PaletteMode::Spectrum);
		if(!micro.CommitHex("#39A7D2") || micro.GetColor()!=Color(57,167,210) ||
		   micro.CommitHex("#bad-input") || actions!=17) { Finish(false,"micro hex/RGB"); return; }
		++step; return;
	}
	if(step==22) {
		micro.SetPaletteMode(UiColorPickerMicro::PaletteMode::Greyscale).ShowRamps(false).SetColumns(4);
		if(!micro.IsRGBMode() || micro.GetPaletteMode()!=UiColorPickerMicro::PaletteMode::Greyscale) {
			Finish(false,"micro palette"); return;
		}
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-micro-light",evidence);
		*report<<evidence; if(!ok) { Finish(false,"micro capture"); return; }
		UiThemeContext dark=original_theme; dark.mode=original_theme.mode==UiThemeMode::Dark?UiThemeMode::Light:UiThemeMode::Dark;
		UiTheme::Set(dark); Ctrl::SwapDarkLight(); theme_changed=true;
		ApplyTabStyle();
		SetRect(GetRect().TopLeft().x,GetRect().TopLeft().y,1000,720); ++step; return;
	}
	if(step==23) {
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-micro-dark",evidence);
		*report<<evidence; if(!ok) { Finish(false,"micro theme capture"); return; }
		tabs.SetActiveTab(1); picker.SetPageMode(UiColorPicker::PAGE_COLOR); ++step; return;
	}
	if(step==24) {
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-picker-dark",evidence);
		*report<<evidence; if(!ok) { Finish(false,"full picker capture"); return; }
		tabs.SetActiveTab(0); ++step; return;
	}
	if(step==25) {
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-browser-dark",evidence); *report<<evidence;
		if(!ok) { Finish(false,"browser capture"); return; }
		tabs.SetActiveTab(3); ++step; return;
	}
	if(step==26) {
		text_entry.SetFocus(); text_entry.SetSelection(0,text_entry.GetText().GetCount());
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-text-idle",evidence); *report<<evidence;
		if(!ok) { Finish(false,"text baseline capture"); return; }
		GpuImeComposition value; for(int cp:{0x633,0x644,0x627,0x645}) value.text.Cat(cp);
		value.caret=2; value.attributes.SetCount(4,1);
		preedit_device_count=VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics().device_live_count;
		if(!text_entry.UpdateGpuComposition(value)) { Finish(false,"text composition update"); return; }
		++step; return;
	}
	if(step==27) {
		if(text_entry.GetCaret().bottom<=text_entry.GetSize().cy ||
		   VulkanTestHooks::GetVulkanRuntimeDeviceDiagnostics().device_live_count!=preedit_device_count) {
			Finish(false,"composition must escape field clipping without a new GPU device"); return;
		}
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-text-preedit",evidence); *report<<evidence;
		text_entry.CancelGpuComposition();
		if(!ok || text_entry.GetText()!=WString("Type or paste here")) { Finish(false,"preedit capture/cancel"); return; }
		WString value; for(int cp:{0x633,0x644,0x627,0x645}) value.Cat(cp);
		if(!text_entry.CommitGpuComposition(value) || text_entry.GetText()!=value) { Finish(false,"text composition commit"); return; }
		++step; return;
	}
	if(step==29) {
		String evidence; bool ok=tabs.GetVisual()==UITAB_UNDERLINE && tabs.GetStyle().tab_extent==DPI(32) &&
		    !tabs.GetStyle().tab_metrics.frame_enabled && CaptureGpuUiClient(*this,"GpuUi-controls",evidence);
		*report<<evidence; if(!ok) { Finish(false,"compact underline controls tab"); return; }
		tabs.SetActiveTab(1); picker.SetPageMode(UiColorPicker::PAGE_IMAGE); ++step; return;
	}
	if(step==30 || step==31) {
		String evidence; bool ok=PickerFieldBounds(picker,evidence);
		ok &= CaptureGpuUiClient(*this,step==30?"GpuUi-picker-image-first-mode":"GpuUi-picker-image-second-mode",evidence);
		*report<<evidence; if(!ok) { Finish(false,"picker Image page field bounds"); return; }
		if(step==30) {
			UiThemeContext other=UiTheme::GetContext();
			other.mode=other.mode==UiThemeMode::Dark?UiThemeMode::Light:UiThemeMode::Dark;
			UiTheme::Set(other); Ctrl::SwapDarkLight(); ApplyTabStyle();
			// Refresh the picker's own mode-dependent child recipes.
			picker.SetPageMode(UiColorPicker::PAGE_COLOR); picker.SetPageMode(UiColorPicker::PAGE_IMAGE);
			++step; return;
		}
		++step; tabs.SetActiveTab(5); return;
	}
	if(step==32) {
		String evidence; bool ok=roles.Qualify(evidence); *report<<evidence;
		if(!ok) { Finish(false,"paired Roles & states profiles"); return; }
		SetRect(0,0,1360,980); ++step; RequestGpuRefresh(); return;
	}
	if(step==33) {
		String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-roles-states",evidence); *report<<evidence;
		if(!ok) { Finish(false,"Roles & states Vulkan capture"); return; }
		tabs.SetActiveTab(1); picker.SetPageMode(UiColorPicker::PAGE_COLOR); ++step; return;
	}
	if(step==34 || step==35) {
		String evidence; bool ok=true;
		for(int channel=0;channel<UiColorPicker::CHANNEL_COUNT;++channel) {
			picker.SetChannelMode((UiColorPicker::ChannelMode)channel);
			ok &= PickerFieldBounds(picker,evidence,UiColorPicker::PAGE_COLOR);
		}
		ok &= CaptureGpuUiClient(*this,step==34?"GpuUi-picker-readouts-first-mode":"GpuUi-picker-readouts-second-mode",evidence);
		*report<<evidence; if(!ok) { Finish(false,"colour readouts and channel fields"); return; }
		if(step==34) {
			UiThemeContext other=UiTheme::GetContext(); other.mode=other.mode==UiThemeMode::Dark?UiThemeMode::Light:UiThemeMode::Dark;
			UiTheme::Set(other); Ctrl::SwapDarkLight(); ApplyTabStyle();
			picker.SetPageMode(UiColorPicker::PAGE_IMAGE); picker.SetPageMode(UiColorPicker::PAGE_COLOR);
			++step; return;
		}
		Finish(true,"browser/full/micro/text, all picker pages, colour channels and paired Roles & states"); return;
	}
	*report<<"text_document_shown="<<text_document.IsShown()<<" size="<<text_document.GetSize()
	       <<" text_length="<<text_document.GetTextW().GetCount()<<" active_tab="<<tabs.GetActiveTab()<<"\n";
	String evidence; bool ok=CaptureGpuUiClient(*this,"GpuUi-text-committed",evidence); *report<<evidence;
	Image idle=StreamRaster::LoadFileAny(GetExeDirFile("GpuUi-text-idle.png"));
	Image preedit=StreamRaster::LoadFileAny(GetExeDirFile("GpuUi-text-preedit.png"));
	Image committed=StreamRaster::LoadFileAny(GetExeDirFile("GpuUi-text-committed.png"));
	Point origin=text_document.GetScreenView().TopLeft()-GetScreenView().TopLeft();
	Rect unchanged=RectC(origin.x+8,origin.y+20,text_document.GetSize().cx-16,text_document.GetSize().cy-28);
	int changed=0,ink_pixels=0;
	ok &= !idle.IsEmpty() && idle.GetSize()==preedit.GetSize() && idle.GetSize()==committed.GetSize() &&
	      unchanged.left>=0 && unchanged.top>=0 && unchanged.right<=idle.GetWidth() && unchanged.bottom<=idle.GetHeight();
	if(ok) for(int y=unchanged.top;y<unchanged.bottom;++y) for(int x=unchanged.left;x<unchanged.right;++x) {
		const RGBA& a=idle[y][x]; const RGBA& b=preedit[y][x]; const RGBA& c=committed[y][x];
		if(a.r>100 && a.g>100 && a.b>100) ++ink_pixels;
		if(a.r!=b.r || a.g!=b.g || a.b!=b.b || a.r!=c.r || a.g!=c.g || a.b!=c.b) ++changed;
	}
	*report<<"text_unchanged_document_pixels="<<changed<<" baseline_ink_pixels="<<ink_pixels<<"\n";
	ok &= changed==0 && ink_pixels>50;
	if(!ok) { Finish(false,"text document preservation"); return; }
	tabs.SetActiveTab(4); step=29;

}
}
