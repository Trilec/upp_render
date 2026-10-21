#include "GpuControls.h"
#include <RenderVulkan/RenderVulkanTestHooks.h>
#include "GpuCapture.h"
#include <plugin/png/png.h>
#include <chrono>

namespace Upp {
namespace {
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
	Title("Vulkan UI controls and text").Sizeable().Zoomable()
	     .SetRect(0,0,1240,800);
	heading.SetText("File browser, colour pickers and multilingual text");
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

	micro.WhenLayoutChange=[=] { Layout(); };
	Add(heading); Add(tabs); Add(status); Add(close);
}
GpuControlsDialog::~GpuControlsDialog()
{
	KillTimeCallback(993);
	if(theme_changed) { UiTheme::Set(original_theme); Ctrl::SwapDarkLight(); }
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
	browser.SetFolder(GetFileFolder(GetExeFilePath())); SetTimeCallback(-100,[=] { Tick(); },993);
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
	KillTimeCallback(993); Close();
}
void GpuControlsDialog::Tick()
{
	if(ControlsNow()-started>30000 || !GetGpuError().IsEmpty() || GetSoftwareFallbackCount()) {
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
	Finish(ok,"scan/preview/selection/accept/cancel/nested micro/16 picker states/themes/resize/text composition overlay/cancel/commit");

}
}
