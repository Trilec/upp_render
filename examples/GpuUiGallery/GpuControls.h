#pragma once
#include <GpuRender/GpuRender.h>
#include <Ui/Ui.h>
#include "GpuWidgets.h"
#include <Ui/UiFileBrowser/UiFileBrowser.h>

namespace Upp {
// Ordinary Ui controls hosted by a Vulkan root, including the optional browser.
class GpuControlsDialog : public GpuTopWindow {
public:
	explicit GpuControlsDialog(bool validation);
	~GpuControlsDialog() override;
	void Layout() override;
	void StartQualification(String& report);
	void ShowTextPage() { tabs.SetActiveTab(3); }
	bool QualificationPassed() const { return passed; }
private:
	TimeCallback qualification_clock;
	void Tick();
	void ApplyTabStyle();
	void Finish(bool ok,const String& detail);
	UiFileBrowser browser;
	UiColorPicker picker;
	UiColorPickerMicro micro;
	UiPanel micro_page,text_page;
	UiLabel text_help;
	UiLineEdit text_entry;
	UiDoc text_document;
	GpuWidgets widgets;
	UiTab tabs;
	UiLabel heading,status;
	UiButton close;
	UiThemeContext original_theme;
	String *report=nullptr;
	int step=0, picker_cases=0, actions=0, accepts=0, cancels=0;
	bool passed=false, theme_changed=false;
	double started=0;
	uint64 frame=0;
	int preedit_device_count=0;
};
}
